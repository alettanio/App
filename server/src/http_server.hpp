// http_server.hpp — lekki serwer HTTP (sockets, bez bibliotek zewnętrznych)
// Działa na Windows (Winsock) i Linux. Jeden wątek na połączenie.
#pragma once
#include <string>
#include <map>
#include <functional>

struct HttpRequest {
    std::string method;   // GET, POST, PUT, DELETE
    std::string path;     // np. /api/orders/ZL-2026-0001/pdf
    std::string query;    // np. status=Gotowe&search=golf
    std::map<std::string, std::string> headers;
    std::string body;
    std::string remoteIp;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "text/html; charset=utf-8";
    std::string body;                       // tekst (JSON/HTML)
    std::vector<unsigned char> binaryBody;  // np. PDF
    bool isBinary = false;
    std::map<std::string, std::string> headers;

    static HttpResponse json(const std::string& body, int status = 200) {
        HttpResponse r; r.status = status; r.contentType = "application/json; charset=utf-8"; r.body = body; return r;
    }
    static HttpResponse text(const std::string& body, int status = 200, const std::string& ct = "text/plain; charset=utf-8") {
        HttpResponse r; r.status = status; r.contentType = ct; r.body = body; return r;
    }
    static HttpResponse pdf(const std::vector<unsigned char>& bytes) {
        HttpResponse r; r.status = 200; r.contentType = "application/pdf"; r.isBinary = true; r.binaryBody = bytes; return r;
    }
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(std::string host, int port, HttpHandler handler)
        : host_(std::move(host)), port_(port), handler_(std::move(handler)) {}
    bool run(); // blokuje — obsługuje pętlę accept
private:
    std::string host_;
    int port_;
    HttpHandler handler_;
};

// Pomocnicze: czy IP jest lokalne (loopback)? Tworzenie zleceń tylko z komputera-serwera.
bool isLoopbackIp(const std::string& ip);
