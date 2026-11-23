#include "client_http.hpp"
#include <cstring>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    static const socket_t BAD_SOCK = INVALID_SOCKET;
    #define CLOSE_SOCK(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    using socket_t = int;
    static const socket_t BAD_SOCK = -1;
    #define CLOSE_SOCK(s) close(s)
#endif

std::string urlEncodeSegment(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { out += '%'; out += hex[(c >> 4) & 0xF]; out += hex[c & 0xF]; }
    }
    return out;
}

// Dekoduje body "chunked" (gdyby serwer/proxy tak odpowiadał).
static std::vector<uint8_t> dechunk(const uint8_t* p, size_t n) {
    std::vector<uint8_t> out;
    size_t i = 0;
    while (i < n) {
        size_t lineEnd = i;
        while (lineEnd + 1 < n && !(p[lineEnd] == '\r' && p[lineEnd + 1] == '\n')) lineEnd++;
        if (lineEnd + 1 >= n) break;
        std::string sizeStr((const char*)p + i, lineEnd - i);
        size_t chunk = 0;
        try { chunk = (size_t)std::stoul(sizeStr, nullptr, 16); } catch (...) { break; }
        if (chunk == 0) break;
        i = lineEnd + 2;
        if (i + chunk > n) chunk = n - i;
        out.insert(out.end(), p + i, p + i + chunk);
        i += chunk + 2; // dane + CRLF
    }
    return out;
}

HttpResult ClientHttp::request(const std::string& path) const {
    HttpResult res;
    // Parsuj baseUrl: http://host[:port]
    std::string u = baseUrl_;
    const std::string prefix = "http://";
    if (u.rfind(prefix, 0) != 0) { res.error = "Adres serwera musi zaczynac sie od http://"; return res; }
    std::string hostport = u.substr(prefix.size());
    size_t slash = hostport.find('/');
    if (slash != std::string::npos) hostport = hostport.substr(0, slash);
    std::string host = hostport;
    int port = 8080;
    size_t colon = hostport.rfind(':');
    if (colon != std::string::npos) { host = hostport.substr(0, colon); port = std::stoi(hostport.substr(colon + 1)); }

#if defined(_WIN32)
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* infos = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &infos) != 0 || !infos) {
        res.error = "Nie mozna znalezc hosta: " + host; return res;
    }
    socket_t sock = socket(infos->ai_family, infos->ai_socktype, infos->ai_protocol);
    if (sock == BAD_SOCK) { freeaddrinfo(infos); res.error = "socket() nie powiodlo sie"; return res; }

    // Timeout 5 s, żeby UI nie wisiało przy braku serwera.
#if defined(_WIN32)
    DWORD tv = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    timeval tv{}; tv.tv_sec = 5;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    if (connect(sock, infos->ai_addr, (int)infos->ai_addrlen) != 0) {
        freeaddrinfo(infos); CLOSE_SOCK(sock);
        res.error = "Brak polaczenia z serwerem " + host + ":" + portStr;
        return res;
    }
    freeaddrinfo(infos);

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + hostport +
                      "\r\nUser-Agent: AutoSerwisPro-Native/1.0\r\nConnection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < req.size()) {
        int n = send(sock, req.data() + sent, (int)(req.size() - sent), 0);
        if (n <= 0) { CLOSE_SOCK(sock); res.error = "Blad wysylania zapytania"; return res; }
        sent += (size_t)n;
    }
    std::vector<uint8_t> raw;
    uint8_t buf[8192];
    while (true) {
        int n = recv(sock, (char*)buf, sizeof(buf), 0);
        if (n < 0) break;
        if (n == 0) break;
        raw.insert(raw.end(), buf, buf + n);
    }
    CLOSE_SOCK(sock);

    // Podział nagłówki / body
    size_t hdrEnd = std::string::npos;
    for (size_t i = 0; i + 3 < raw.size(); i++) {
        if (raw[i] == '\r' && raw[i+1] == '\n' && raw[i+2] == '\r' && raw[i+3] == '\n') { hdrEnd = i; break; }
    }
    if (hdrEnd == std::string::npos) { res.error = "Pusta odpowiedz serwera"; return res; }
    std::string headers((const char*)raw.data(), hdrEnd);
    // status: "HTTP/1.1 200 OK"
    {
        size_t sp1 = headers.find(' ');
        size_t sp2 = headers.find(' ', sp1 == std::string::npos ? 0 : sp1 + 1);
        if (sp1 != std::string::npos && sp2 != std::string::npos) {
            try { res.status = std::stoi(headers.substr(sp1 + 1, sp2 - sp1 - 1)); } catch (...) {}
        }
    }
    std::vector<uint8_t> body(raw.begin() + (long)hdrEnd + 4, raw.end());
    // chunked?
    std::string lower = headers;
    for (auto& ch : lower) ch = (char)tolower((unsigned char)ch);
    if (lower.find("transfer-encoding: chunked") != std::string::npos) body = dechunk(body.data(), body.size());

    res.binary = body;
    res.body.assign(body.begin(), body.end());
    res.ok = res.status >= 200 && res.status < 300;
    if (!res.ok && res.error.empty()) res.error = "HTTP " + std::to_string(res.status);
    return res;
}

HttpResult ClientHttp::get(const std::string& path) const { return request(path); }
HttpResult ClientHttp::getBinary(const std::string& path) const { return request(path); }
