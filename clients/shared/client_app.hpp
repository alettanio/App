// client_app.hpp — logika aplikacji natywnej (Windows Vulkan / Android Vulkan).
// Tryb tylko-do-odczytu: pobiera zlecenia z komputera-serwera przez HTTP API,
// pokazuje listę i szczegóły, pobiera PDF. Tworzenie zleceń odbywa się wyłącznie
// na komputerze-serwerze (przeglądarka lokalnie / serwer C++).
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../../shared/order_model.hpp"
#include "client_http.hpp"
#include "ui_draw.hpp"

class ClientApp {
public:
    explicit ClientApp(std::string serverUrl);

    // Wywoływane przez warstwę platformy po kliknięciu "POBIERZ PDF"
    // (Windows: zapis + otwarcie, Android: zapis do katalogu aplikacji).
    void setPdfCallback(std::function<void(const Order&)> cb) { pdfCb_ = std::move(cb); }
    void setPdfDownloader(std::function<bool(const Order&, std::string& savedPath)> dl) { pdfDl_ = std::move(dl); }

    bool refresh();                       // GET /api/orders (blokujące, LAN)
    const std::string& statusMessage() const { return statusMsg_; }
    const std::string& serverUrl() const { return http_.baseUrl(); }
    bool hasSelection() const { return selected_ >= 0 && selected_ < (int)orders_.size(); }
    bool detailFullscreen(float width) const { return hasSelection() && width < 950.0f; }

    DrawList draw(float width, float height);
    void onTap(float x, float y);
    void onDrag(float dx, float dy, float x, float y);
    void onScroll(float dy, float x, float y);

private:
    enum class Action { Refresh, Select, Back, Pdf };
    struct Hit { float x, y, w, h; Action action; int index = -1; };

    void drawList(DrawList& dl, float x, float y, float w, float h);
    void drawDetail(DrawList& dl, float x, float y, float w, float h);
    UiColor statusColor(const std::string& status) const;

    ClientHttp http_;
    std::vector<Order> orders_;
    int selected_ = -1;
    float listScroll_ = 0;
    float detailScroll_ = 0;
    std::string statusMsg_ = "Laczenie z serwerem...";
    mutable std::vector<Hit> hits_;   // wypełniane w draw(), używane w onTap()
    float lastW_ = 0, lastH_ = 0;
    std::function<void(const Order&)> pdfCb_;
    std::function<bool(const Order&, std::string&)> pdfDl_;
};
