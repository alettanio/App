// main.cpp — punkt startowy serwera AutoSerwis Pro (Windows / Linux, C++17)
// Komputer z tym programem działa jako SERWER: przechowuje dane zleceń (data/orders.json),
// generuje PDF-y i serwuje aplikację przeglądarkową. Telefon (Android/Vulkan) i inne
// komputery łączą się do niego przez API (tylko odczyt + PDF; tworzenie tylko lokalnie).
#include <filesystem>
#include <iostream>
#include <string>
#include "http_server.hpp"
#include "store.hpp"
#include "api.hpp"

static void printHelp() {
    std::cout <<
        "AutoSerwis Pro - serwer zlecen serwisowych\n"
        "Uzycie: autoservis-server [--host 0.0.0.0] [--port 8080] [--data data/orders.json] [--web server/web] [--allow-remote-writes]\n"
        "  --allow-remote-writes  zezwol na tworzenie/edycje zlecen z sieci LAN (domyslnie tylko localhost)\n";
}

int main(int argc, char** argv) {
    ServerConfig cfg;
    std::string host = "0.0.0.0";
    int port = 8080;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printHelp(); return 0; }
        else if (a == "--host" && i + 1 < argc) host = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (a == "--data" && i + 1 < argc) cfg.dataFile = argv[++i];
        else if (a == "--web" && i + 1 < argc) cfg.webDir = argv[++i];
        else if (a == "--allow-remote-writes") cfg.allowRemoteWrites = true;
    }

    // Gdy exe uruchomiono z katalogu build/ (CMake kopiuje tam web/), a nie z root repo:
    if (!std::filesystem::exists(cfg.webDir) && std::filesystem::exists("web")) cfg.webDir = "web";
    if (!std::filesystem::exists(cfg.webDir))
        std::cout << "Uwaga: brak katalogu aplikacji przegladarkowej: " << cfg.webDir << " (API bedzie dzialac)\n";

    OrderStore store(cfg.dataFile);
    if (!store.load()) {
        std::cout << "Uwaga: nie udalo sie wczytac pliku danych - start z pustej listy.\n";
    }
    std::cout << "AutoSerwis Pro - serwer zlecen\n";
    std::cout << "Dane: " << cfg.dataFile << "\n";
    std::cout << "Aplikacja przegladarkowa: http://localhost:" << port << "/\n";
    std::cout << "API: http://localhost:" << port << "/api/orders\n";
    std::cout << "Tworzenie/edycja zlecen: " << (cfg.allowRemoteWrites ? "LAN + localhost" : "tylko localhost (komputer-serwer)") << "\n";

    HttpServer server(host, port, makeHandler(store, cfg));
    return server.run() ? 0 : 1;
}
