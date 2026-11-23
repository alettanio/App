// api.hpp — routing REST API + serwowanie plików statycznych (wersja przeglądarkowa)
#pragma once
#include <string>
#include "http_server.hpp"
#include "store.hpp"
#include "pdf.hpp"

struct ServerConfig {
    std::string dataFile = "data/orders.json";
    std::string webDir = "server/web";
    bool allowRemoteWrites = false; // domyślnie: tworzenie/edycja tylko z localhost (komputer-serwer)
    CompanyInfo company;
};

HttpHandler makeHandler(OrderStore& store, const ServerConfig& cfg);
