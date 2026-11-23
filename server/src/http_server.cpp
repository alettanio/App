#include "http_server.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cstring>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    static const socket_t INVALID_SOCK = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    static const socket_t INVALID_SOCK = -1;
#endif

bool isLoopbackIp(const std::string& ip) {
    return ip == "127.0.0.1" || ip == "::1" || ip == "localhost" || ip.rfind("127.", 0) == 0;
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static void sendAll(socket_t sock, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(sock, data.data() + sent, (int)(data.size() - sent), 0);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

static void handleClient(socket_t client, HttpHandler& handler) {
    // Odczyt nagłówków (do \r\n\r\n)
    std::string raw;
    char buf[8192];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) {
#if defined(_WIN32)
            closesocket(client);
#else
            close(client);
#endif
            return;
        }
        raw.append(buf, (size_t)n);
        headerEnd = raw.find("\r\n\r\n");
        if (raw.size() > 1024 * 1024) break; // limit nagłówków
    }
    if (headerEnd == std::string::npos) {
#if defined(_WIN32)
        closesocket(client);
#else
        close(client);
#endif
        return;
    }
    std::string headerPart = raw.substr(0, headerEnd);
    std::string bodyPart = raw.substr(headerEnd + 4);

    HttpRequest req;
    std::istringstream hs(headerPart);
    std::string line;
    if (!std::getline(hs, line)) {
#if defined(_WIN32)
        closesocket(client);
#else
        close(client);
#endif
        return;
    }
    // Linia żądania: METHOD PATH HTTP/1.1
    {
        std::istringstream ls(trim(line));
        std::string target;
        ls >> req.method >> target;
        size_t q = target.find('?');
        if (q != std::string::npos) { req.path = target.substr(0, q); req.query = target.substr(q + 1); }
        else req.path = target;
    }
    size_t contentLength = 0;
    while (std::getline(hs, line)) {
        line = trim(line);
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = toLower(trim(line.substr(0, colon)));
        std::string val = trim(line.substr(colon + 1));
        req.headers[key] = val;
        if (key == "content-length") contentLength = (size_t)std::stoul(val);
    }
    // Doczytanie treści
    while (bodyPart.size() < contentLength) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) break;
        bodyPart.append(buf, (size_t)n);
    }
    req.body = bodyPart.substr(0, contentLength);

    // IP klienta
    {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (getpeername(client, (sockaddr*)&addr, &len) == 0)
            req.remoteIp = inet_ntoa(addr.sin_addr);
    }

    HttpResponse res;
    try {
        res = handler(req);
    } catch (const std::exception& e) {
        res = HttpResponse::json(std::string("{\"error\":\"") + e.what() + "\"}", 500);
    }

    // Odpowiedź HTTP/1.1
    std::string payload;
    if (res.isBinary) payload.assign((const char*)res.binaryBody.data(), res.binaryBody.size());
    else payload = res.body;

    std::ostringstream head;
    head << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "\r\n";
    head << "Content-Type: " << res.contentType << "\r\n";
    head << "Content-Length: " << payload.size() << "\r\n";
    head << "Access-Control-Allow-Origin: *\r\n";
    head << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    head << "Access-Control-Allow-Headers: Content-Type, X-Client\r\n";
    head << "Connection: close\r\n";
    for (auto& kv : res.headers) head << kv.first << ": " << kv.second << "\r\n";
    head << "\r\n";
    sendAll(client, head.str());
    if (!payload.empty()) sendAll(client, payload);

#if defined(_WIN32)
    closesocket(client);
#else
    close(client);
#endif
}

bool HttpServer::run() {
#if defined(_WIN32)
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
    socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCK) return false;

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port_);
    if (host_ == "0.0.0.0") addr.sin_addr.s_addr = INADDR_ANY;
    else addr.sin_addr.s_addr = inet_addr(host_.c_str());

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "Nie mozna zbindowac adresu " << host_ << ":" << port_ << "\n";
        return false;
    }
    if (listen(listener, 64) != 0) return false;

    std::cout << "Serwer nasluchuje na " << host_ << ":" << port_ << "\n";
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        socket_t client = accept(listener, (sockaddr*)&clientAddr, &len);
        if (client == INVALID_SOCK) continue;
        std::thread(handleClient, client, std::ref(handler_)).detach();
    }
}
