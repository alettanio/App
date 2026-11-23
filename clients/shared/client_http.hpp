// client_http.hpp — minimalny klient HTTP/1.1 (sockets Winsock/POSIX, bez TLS).
// Do komunikacji w sieci LAN z komputerem-serwerem AutoSerwis Pro.
#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string body;                 // odpowiedź tekstowa (JSON)
    std::vector<uint8_t> binary;      // odpowiedź binarna (PDF)
    std::string error;
};

class ClientHttp {
public:
    // baseUrl np. "http://192.168.1.10:8080" (bez końcowego '/')
    explicit ClientHttp(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}
    const std::string& baseUrl() const { return baseUrl_; }
    void setBaseUrl(std::string u) { baseUrl_ = std::move(u); }

    HttpResult get(const std::string& path) const;        // GET, body tekstowe
    HttpResult getBinary(const std::string& path) const;  // GET, body binarne

private:
    HttpResult request(const std::string& path) const;
    std::string baseUrl_;
};

// Kodowanie pojedynczego segmentu ścieżki URL (np. numeru zlecenia).
std::string urlEncodeSegment(const std::string& s);
