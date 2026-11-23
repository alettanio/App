#include "client_app.hpp"
#include <algorithm>
#include <cmath>

ClientApp::ClientApp(std::string serverUrl) : http_(std::move(serverUrl)) {}

bool ClientApp::refresh() {
    auto r = http_.get("/api/orders");
    if (!r.ok) {
        statusMsg_ = "BLAD: " + (r.error.empty() ? std::string("brak polaczenia") : r.error) +
                     " | Serwer: " + http_.baseUrl();
        return false;
    }
    try {
        mini::Value v = mini::parse(r.body);
        orders_.clear();
        if (v.isArray()) for (auto& item : v.array) orders_.push_back(Order::fromJson(item));
        std::sort(orders_.begin(), orders_.end(), [](const Order& a, const Order& b) {
            return a.dateReceived > b.dateReceived;
        });
        if (selected_ >= (int)orders_.size()) selected_ = orders_.empty() ? -1 : 0;
        statusMsg_ = "Polaczono: " + http_.baseUrl() + " | Zlecen: " + std::to_string(orders_.size()) +
                     " | Tylko odczyt (tworzenie na komputerze-serwerze)";
        listScroll_ = 0;
        return true;
    } catch (const std::exception& e) {
        statusMsg_ = std::string("BLAD danych: ") + e.what();
        return false;
    }
}

UiColor ClientApp::statusColor(const std::string& s) const {
    if (s == "Gotowe") return ui::ACCENT2;
    if (s == "W realizacji") return ui::WARN;
    if (s == "Anulowane") return ui::BAD;
    if (s == "Oczekuje na części") return UiColor(0.65f, 0.50f, 0.95f);
    return ui::BLUE;
}

void ClientApp::drawList(DrawList& dl, float x, float y, float w, float h) {
    dl.rect(x, y, w, h, ui::PANEL);
    dl.text(x + 14, y + 12, 2, ui::TEXT, "ZLECENIA (" + std::to_string(orders_.size()) + ")", w - 28);
    float listTop = y + 44;
    float cardH = 96, gap = 10;
    float contentH = orders_.empty() ? 0 : orders_.size() * (cardH + gap);
    float viewH = h - 44;
    listScroll_ = std::max(0.0f, std::min(listScroll_, std::max(0.0f, contentH - viewH)));
    if (orders_.empty()) {
        dl.text(x + 14, listTop + 10, 2, ui::MUTED, "BRAK ZLECEN.", w - 28);
        dl.text(x + 14, listTop + 34, 2, ui::MUTED, "SPRAWDZ SERWER I ODSWIEZ.", w - 28);
        return;
    }
    for (int i = 0; i < (int)orders_.size(); i++) {
        float cy = listTop + i * (cardH + gap) - listScroll_;
        if (cy + cardH < listTop || cy > y + h) continue;
        const Order& o = orders_[i];
        bool sel = (i == selected_);
        dl.rect(x + 10, cy, w - 20, cardH, sel ? ui::CARD_SEL : ui::CARD);
        dl.rect(x + 10, cy, 5, cardH, statusColor(o.status));
        float tx = x + 26;
        float tw = w - 46;
        dl.text(tx, cy + 10, 2, ui::ACCENT, o.id, tw * 0.55f);
        dl.text(tx + tw * 0.56f, cy + 10, 2, statusColor(o.status), o.status, tw * 0.44f);
        dl.text(tx, cy + 32, 2, ui::TEXT, o.carBrand + " " + o.carModel + " " + o.carYear + "  " + o.carPlate, tw);
        dl.text(tx, cy + 54, 2, ui::MUTED, o.clientName + "  " + o.dateReceived + " -> " + o.dateDue, tw);
        dl.text(tx, cy + 76, 2, ui::TEXT, "DO ZAPLATY: " + formatPLN(o.totalToPay()), tw);
        hits_.push_back({x + 10, cy, w - 20, cardH, Action::Select, i});
    }
}

void ClientApp::drawDetail(DrawList& dl, float x, float y, float w, float h) {
    dl.rect(x, y, w, h, ui::PANEL);
    if (!hasSelection()) {
        dl.text(x + 16, y + 16, 2, ui::TEXT, "WYBIERZ ZLECENIE Z LISTY", w - 32);
        dl.text(x + 16, y + 42, 2, ui::MUTED, "TWORZENIE ZLECEN TYLKO NA", w - 32);
        dl.text(x + 16, y + 64, 2, ui::MUTED, "KOMPUTERZE-SERWERZE (PRZEGLADARKA).", w - 32);
        dl.text(x + 16, y + 96, 2, ui::MUTED, "TU: PODGLAD + PDF.", w - 32);
        return;
    }
    const Order& o = orders_[selected_];
    bool fs = detailFullscreen(lastW_);
    float curY = y + 12 - detailScroll_;
    float top = y + 8, bottom = y + h - 8;
    auto line = [&](float scale, UiColor c, const std::string& s, float gapAfter = 6) {
        if (curY + 7 * scale >= top && curY <= bottom) dl.text(x + 16, curY, scale, c, s, w - 32);
        curY += 7 * scale + gapAfter;
    };
    auto section = [&](const std::string& s) {
        curY += 6;
        if (curY >= top && curY <= bottom) {
            dl.rect(x + 12, curY - 4, w - 24, 24, UiColor(0.14f, 0.11f, 0.07f));
            dl.text(x + 16, curY, 2, ui::ACCENT, s, w - 32);
        }
        curY += 26;
    };

    // Nagłówek szczegółów + przyciski (zawsze widoczne, poza scrollem)
    dl.rect(x, y, w, 52, UiColor(0.05f, 0.07f, 0.13f));
    dl.text(x + 16, y + 10, 3, ui::TEXT, o.id, w * 0.45f);
    dl.text(x + 16, y + 34, 2, statusColor(o.status), o.status + " | " + o.priority, w * 0.45f);
    float bx = x + w - 16;
    // PDF (prawy)
    bx -= 190;
    dl.rect(bx, y + 9, 190, 36, ui::ACCENT2);
    dl.text(bx + 14, y + 19, 2, ui::TEXT, "POBIERZ PDF", 170);
    hits_.push_back({bx, y + 9, 190, 36, Action::Pdf, selected_});
    if (fs) {
        bx -= 130;
        dl.rect(bx, y + 9, 120, 36, ui::CARD);
        dl.text(bx + 12, y + 19, 2, ui::TEXT, "< WROC", 104);
        hits_.push_back({bx, y + 9, 120, 36, Action::Back, -1});
    }
    curY = y + 64 - detailScroll_;

    section("KLIENT");
    line(2, ui::TEXT, o.clientName);
    line(2, ui::MUTED, "TEL: " + o.clientPhone + "  " + o.clientEmail);
    if (!o.clientAddress.empty()) line(2, ui::MUTED, o.clientAddress);

    section("POJAZD");
    line(2, ui::TEXT, o.carBrand + " " + o.carModel + " (" + o.carYear + ")");
    line(2, ui::MUTED, "SILNIK: " + o.carEngine + "  PALIWO: " + o.carFuel);
    line(2, ui::MUTED, "REJ: " + o.carPlate + "  PRZEBIEG: " + o.carMileage);
    line(2, ui::MUTED, "VIN: " + o.carVin);

    section("TERMINY");
    line(2, ui::MUTED, "PRZYJECIE: " + o.dateReceived + "  ODDANIE: " + o.dateDue);
    line(2, ui::MUTED, "MECHANIK: " + o.mechanic);
    line(2, ui::MUTED, "GWARANCJA: " + o.warranty);

    section("ZGLOSZENIE / DIAGNOZA");
    line(2, ui::TEXT, o.fault);
    line(2, ui::MUTED, o.diagnosis);
    if (!o.notes.empty()) line(2, ui::MUTED, "UWAGI: " + o.notes);

    section("NAPRAWY / CZESCI");
    if (o.repairs.empty()) line(2, ui::MUTED, "BRAK POZYCJI");
    for (size_t i = 0; i < o.repairs.size(); i++) {
        const auto& r = o.repairs[i];
        line(2, ui::TEXT, std::to_string(i + 1) + ". " + r.name + (r.code.empty() ? "" : " [" + r.code + "]"), 2);
        char buf[160];
        snprintf(buf, sizeof(buf), "    %.2f X %s  VAT %.0f%%  = %s",
                 r.qty, formatPLN(r.priceNet).c_str(), r.vatRate, formatPLN(r.gross()).c_str());
        line(2, ui::MUTED, buf, 8);
    }

    section("PODSUMOWANIE");
    line(2, ui::MUTED, "NETTO:  " + formatPLN(o.netTotal()));
    line(2, ui::MUTED, "VAT:    " + formatPLN(o.vatTotal()));
    line(2, ui::MUTED, "BRUTTO: " + formatPLN(o.grossTotal()));
    if (o.discountPct > 0) line(2, ui::MUTED, "RABAT " + std::to_string((int)o.discountPct) + "%: -" + formatPLN(o.discountAmount()));
    if (o.advance > 0) line(2, ui::MUTED, "ZALICZKA: -" + formatPLN(o.advance));
    curY += 4;
    if (curY >= top && curY <= bottom) {
        dl.rect(x + 12, curY - 4, w - 24, 30, UiColor(0.20f, 0.13f, 0.04f));
        dl.text(x + 16, curY + 2, 3, ui::ACCENT, "DO ZAPLATY: " + formatPLN(o.totalToPay()), w - 32);
    }
    curY += 34;
    float contentH = curY - (y + 64) + detailScroll_;
    float maxScroll = std::max(0.0f, contentH - (h - 72));
    if (detailScroll_ > maxScroll) detailScroll_ = maxScroll;
    if (detailScroll_ < 0) detailScroll_ = 0;
}

DrawList ClientApp::draw(float width, float height) {
    lastW_ = width; lastH_ = height;
    hits_.clear();
    DrawList dl;
    dl.rect(0, 0, width, height, ui::BG);

    // Pasek górny
    dl.rect(0, 0, width, 58, UiColor(0.05f, 0.07f, 0.13f));
    dl.rect(0, 56, width, 2, ui::ACCENT);
    float scaleTitle = width < 600 ? 2 : 3;
    dl.text(14, 10, scaleTitle, ui::TEXT, "AUTOSERWIS PRO", width * 0.5f);
    dl.text(14, 36, 2, ui::MUTED, "ZLECENIA | VULKAN | " + http_.baseUrl(), width * 0.62f);
    float bw = 170;
    dl.rect(width - bw - 12, 11, bw, 36, ui::ACCENT);
    dl.text(width - bw + 18, 21, 2, UiColor(0.08f, 0.05f, 0.02f), "ODSWIEZ", bw - 24);
    hits_.push_back({width - bw - 12, 11, bw, 36, Action::Refresh, -1});

    float top = 58;
    float availH = height - top - 28;
    if (detailFullscreen(width)) {
        drawDetail(dl, 0, top, width, availH);
    } else if (width >= 950) {
        float lw = 430;
        drawList(dl, 10, top + 8, lw, availH - 8);
        drawDetail(dl, lw + 20, top + 8, width - lw - 30, availH - 8);
    } else {
        drawList(dl, 0, top, width, availH);
    }

    // Pasek statusu
    dl.rect(0, height - 28, width, 28, UiColor(0.05f, 0.07f, 0.13f));
    dl.text(10, height - 20, 2, statusMsg_.rfind("BLAD", 0) == 0 ? ui::BAD : ui::MUTED, statusMsg_, width - 20);
    return dl;
}

void ClientApp::onTap(float x, float y) {
    for (int i = (int)hits_.size() - 1; i >= 0; i--) {
        const auto& ht = hits_[i];
        if (x >= ht.x && x <= ht.x + ht.w && y >= ht.y && y <= ht.y + ht.h) {
            switch (ht.action) {
                case Action::Refresh: refresh(); break;
                case Action::Select: selected_ = ht.index; detailScroll_ = 0; break;
                case Action::Back: selected_ = -1; break;
                case Action::Pdf: {
                    if (hasSelection()) {
                        const Order o = orders_[selected_];
                        if (pdfDl_) {
                            std::string path;
                            statusMsg_ = pdfDl_(o, path) ? ("Zapisano PDF: " + path) : "BLAD pobierania PDF z serwera";
                        } else if (pdfCb_) pdfCb_(o);
                    }
                    break;
                }
            }
            return;
        }
    }
}

void ClientApp::onDrag(float dx, float dy, float x, float y) {
    (void)dx;
    if (detailFullscreen(lastW_)) { detailScroll_ -= dy; if (detailScroll_ < 0) detailScroll_ = 0; return; }
    if (lastW_ >= 950) {
        if (x < 450) listScroll_ -= dy; else detailScroll_ -= dy;
    } else listScroll_ -= dy;
    if (listScroll_ < 0) listScroll_ = 0;
    if (detailScroll_ < 0) detailScroll_ = 0;
}

void ClientApp::onScroll(float dy, float x, float y) { onDrag(0, dy * 28.0f, x, y); }
