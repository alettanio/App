// pdf.cpp — implementacja generatora PDF (PDF 1.4, czcionka Helvetica, WinAnsi)
// Układ strony A4 (595x842 pt), margines 40 pt. Kolorystyka: granat #1e3a5f,
// akcent #2563eb, jasnoszary #f1f5f9. Obsługa wielu stron dla długiej tabeli napraw.
#include "pdf.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace {

// ---------- Konwersja UTF-8 -> WinAnsi (CP1250 dla polskich znaków) ----------
std::string utf8ToWinAnsi(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size();) {
        unsigned char c = (unsigned char)in[i];
        if (c < 0x80) { out += (char)c; i++; continue; }
        unsigned cp = 0; size_t len = 0;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { out += '?'; i++; continue; }
        if (i + len > in.size()) { out += '?'; i++; continue; }
        for (size_t k = 1; k < len; k++) cp = (cp << 6) | ((unsigned char)in[i + k] & 0x3F);
        i += len;
        // Mapowanie polskich znaków na WinAnsi/CP1250
        char mapped = '?';
        switch (cp) {
            case 0x0105: mapped = (char)0xB1; break; // ą
            case 0x0104: mapped = (char)0xA5; break; // Ą
            case 0x0107: mapped = (char)0xE6; break; // ć
            case 0x0106: mapped = (char)0xC6; break; // Ć
            case 0x0119: mapped = (char)0xEA; break; // ę
            case 0x0118: mapped = (char)0xCA; break; // Ę
            case 0x0142: mapped = (char)0xB3; break; // ł
            case 0x0141: mapped = (char)0xA3; break; // Ł
            case 0x0144: mapped = (char)0xF1; break; // ń
            case 0x0143: mapped = (char)0xD1; break; // Ń
            case 0x00F3: mapped = (char)0xF3; break; // ó
            case 0x00D3: mapped = (char)0xD3; break; // Ó
            case 0x015B: mapped = (char)0x9C; break; // ś
            case 0x015A: mapped = (char)0x8C; break; // Ś
            case 0x017A: mapped = (char)0x9F; break; // ź
            case 0x0179: mapped = (char)0x8F; break; // Ź
            case 0x017C: mapped = (char)0xBF; break; // ż (jak ź w WinAnsi)
            case 0x017B: mapped = (char)0xAF; break; // Ż
            case 0x20AC: mapped = (char)0x80; break; // €
            default:
                if (cp >= 0xA0 && cp <= 0xFF) mapped = (char)cp;
                else mapped = '?';
        }
        out += mapped;
    }
    return out;
}

std::string pdfEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '(' || c == ')' || c == '\\') { out += '\\'; out += c; }
        else out += c;
    }
    return out;
}

std::string fmtNum(double v) {
    char buf[32]; snprintf(buf, sizeof(buf), "%.2f", v);
    std::string s = buf;
    size_t dot = s.find('.');
    if (dot != std::string::npos) s.replace(dot, 1, ",");
    return s;
}

std::string fmtDate(const std::string& iso) {
    // YYYY-MM-DD -> DD.MM.YYYY
    if (iso.size() < 10) return iso;
    return iso.substr(8, 2) + "." + iso.substr(5, 2) + "." + iso.substr(0, 4);
}

// Szacunkowa szerokość tekstu Helvetica (przybl. 0.5 * rozmiar fontu na znak)
double textWidth(const std::string& s, double fontSize) {
    return (double)s.size() * fontSize * 0.52;
}

// Zawijanie tekstu do szerokości (proste, po słowach)
std::vector<std::string> wrapText(const std::string& text, double maxWidth, double fontSize) {
    std::vector<std::string> lines;
    if (text.empty()) return lines;
    std::string cur;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        std::string trial = cur.empty() ? word : cur + " " + word;
        if (textWidth(trial, fontSize) <= maxWidth || cur.empty()) cur = trial;
        else { lines.push_back(cur); cur = word; }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// ---------- Budowniczy strony PDF ----------
struct PageBuilder {
    std::ostringstream stream;
    double y = 842.0 - 40.0; // bieżąca pozycja Y (od góry dokumentu, PDF liczy od dołu)

    void setFill(double r, double g, double b) {
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f %.3f %.3f rg\n", r, g, b);
        stream << buf;
    }
    void setStroke(double r, double g, double b) {
        char buf[64]; snprintf(buf, sizeof(buf), "%.3f %.3f %.3f RG\n", r, g, b);
        stream << buf;
    }
    void setLineWidth(double w) {
        char buf[32]; snprintf(buf, sizeof(buf), "%.2f w\n", w); stream << buf;
    }
    void rect(double x, double yBottom, double w, double h, bool fill) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%.2f %.2f %.2f %.2f re %s\n", x, yBottom, w, h, fill ? "f" : "S");
        stream << buf;
    }
    void line(double x1, double y1, double x2, double y2) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%.2f %.2f m %.2f %.2f l S\n", x1, y1, x2, y2);
        stream << buf;
    }
    // Tekst: x, yBottom = dolna linia tekstu
    void text(double x, double yBottom, const std::string& s, double size, bool bold = false) {
        stream << "BT /" << (bold ? "F2" : "F1") << " " << size << " Tf ";
        char buf[64]; snprintf(buf, sizeof(buf), "%.2f %.2f Td ", x, yBottom);
        stream << buf << "(" << pdfEscape(utf8ToWinAnsi(s)) << ") Tj ET\n";
    }
    void textRight(double xRight, double yBottom, const std::string& s, double size, bool bold = false) {
        double w = textWidth(utf8ToWinAnsi(s), size);
        text(xRight - w, yBottom, s, size, bold);
    }
    void textCenter(double cx, double yBottom, const std::string& s, double size, bool bold = false) {
        double w = textWidth(utf8ToWinAnsi(s), size);
        text(cx - w / 2.0, yBottom, s, size, bold);
    }
};

} // namespace

std::vector<uint8_t> PdfGenerator::generate(const Order& o) const {
    const double PAGE_W = 595.0, PAGE_H = 842.0;
    const double MARGIN = 40.0;
    const double CONTENT_W = PAGE_W - 2 * MARGIN;
    // Kolory
    const double NAVY_R = 0.118, NAVY_G = 0.227, NAVY_B = 0.373;   // #1e3a5f
    const double BLUE_R = 0.145, BLUE_G = 0.388, BLUE_B = 0.922;   // #2563eb
    const double LIGHT_R = 0.945, LIGHT_G = 0.965, LIGHT_B = 0.976; // #f1f5f9
    const double GREY_R = 0.39, GREY_G = 0.44, GREY_B = 0.51;
    const double DARK_R = 0.06, DARK_G = 0.09, DARK_B = 0.15;

    std::vector<std::string> pages; // strumienie treści stron

    auto newPage = [&]() -> PageBuilder {
        PageBuilder p;
        p.y = PAGE_H - MARGIN;
        return p;
    };

    PageBuilder pg = newPage();

    // ===== NAGŁÓWEK FIRMY =====
    {
        double headerH = 64;
        double top = PAGE_H - MARGIN;
        pg.setFill(NAVY_R, NAVY_G, NAVY_B);
        pg.rect(MARGIN, top - headerH, CONTENT_W, headerH, true);
        pg.setFill(1, 1, 1);
        pg.text(MARGIN + 14, top - 28, company_.name, 20, true);
        pg.text(MARGIN + 14, top - 46, company_.address + "  |  NIP: " + company_.nip, 8);
        pg.text(MARGIN + 14, top - 58, "Tel: " + company_.phone + "  |  " + company_.email, 8);
        // Prawa strona nagłówka: tytuł dokumentu
        pg.textRight(MARGIN + CONTENT_W - 14, top - 28, "ZLECENIE SERWISOWE", 13, true);
        pg.textRight(MARGIN + CONTENT_W - 14, top - 46, "Nr: " + o.id, 11, true);
        pg.textRight(MARGIN + CONTENT_W - 14, top - 60,
                     "Przyjęto: " + fmtDate(o.dateReceived) + "   Termin: " + fmtDate(o.dateDue), 8);
        pg.y = top - headerH - 14;
    }

    // ===== SEKCJA: KLIENT / POJAZD (dwie kolumny) =====
    auto sectionHeader = [&](PageBuilder& p, const std::string& title, double x, double w) {
        p.setFill(BLUE_R, BLUE_G, BLUE_B);
        p.rect(x, p.y - 18, w, 18, true);
        p.setFill(1, 1, 1);
        p.text(x + 8, p.y - 13, title, 10, true);
        p.y -= 18;
    };
    auto infoRow = [&](PageBuilder& p, double x, double w, const std::string& label, const std::string& value) {
        p.setFill(GREY_R, GREY_G, GREY_B);
        p.text(x + 8, p.y - 11, label, 8);
        p.setFill(DARK_R, DARK_G, DARK_B);
        p.text(x + 110, p.y - 11, value.empty() ? "-" : value, 9, true);
        p.setStroke(0.85, 0.88, 0.92); p.setLineWidth(0.5);
        p.line(x + 8, p.y - 15, x + w - 8, p.y - 15);
        p.y -= 16;
    };

    {
        double colW = CONTENT_W / 2 - 6;
        double startY = pg.y;
        // Kolumna lewa: Klient
        sectionHeader(pg, "KLIENT", MARGIN, colW);
        infoRow(pg, MARGIN, colW, "Imię i nazwisko", o.clientName);
        infoRow(pg, MARGIN, colW, "Telefon", o.clientPhone);
        infoRow(pg, MARGIN, colW, "E-mail", o.clientEmail);
        infoRow(pg, MARGIN, colW, "Adres", o.clientAddress);
        double leftEndY = pg.y;
        // Kolumna prawa: Pojazd
        pg.y = startY;
        sectionHeader(pg, "POJAZD", MARGIN + colW + 12, colW);
        std::string carFull = (o.carBrand + " " + o.carModel).empty() ? "-" : (o.carBrand + " " + o.carModel);
        infoRow(pg, MARGIN + colW + 12, colW, "Marka / model", carFull);
        infoRow(pg, MARGIN + colW + 12, colW, "Rok / silnik", o.carYear + "  " + o.carEngine);
        infoRow(pg, MARGIN + colW + 12, colW, "Nr rejestracyjny", o.carPlate);
        infoRow(pg, MARGIN + colW + 12, colW, "VIN", o.carVin);
        infoRow(pg, MARGIN + colW + 12, colW, "Przebieg / paliwo", o.carMileage + "  " + o.carFuel);
        pg.y = std::min(leftEndY, pg.y) - 10;
    }

    // ===== STATUS / PRIORYTET / MECHANIK =====
    {
        pg.setFill(LIGHT_R, LIGHT_G, LIGHT_B);
        pg.rect(MARGIN, pg.y - 20, CONTENT_W, 20, true);
        pg.setFill(DARK_R, DARK_G, DARK_B);
        pg.text(MARGIN + 10, pg.y - 14, "Status: " + o.status, 9, true);
        pg.text(MARGIN + 200, pg.y - 14, "Priorytet: " + o.priority, 9);
        pg.text(MARGIN + 330, pg.y - 14, "Mechanik: " + o.mechanic, 9);
        pg.y -= 30;
    }

    // ===== ZGŁOSZENIE / DIAGNOZA =====
    auto textBox = [&](PageBuilder& p, const std::string& title, const std::string& body) {
        sectionHeader(p, title, MARGIN, CONTENT_W);
        p.setFill(1, 1, 1);
        p.setStroke(0.85, 0.88, 0.92); p.setLineWidth(0.5);
        auto lines = wrapText(body.empty() ? "-" : body, CONTENT_W - 20, 9);
        double boxH = 10 + (double)lines.size() * 13 + 8;
        p.rect(MARGIN, p.y - boxH, CONTENT_W, boxH, false);
        p.setFill(DARK_R, DARK_G, DARK_B);
        double ty = p.y - 14;
        for (auto& ln : lines) { p.text(MARGIN + 10, ty, ln, 9); ty -= 13; }
        p.y -= boxH + 10;
    };
    textBox(pg, "ZGŁOSZENIE KLIENTA (USTERKA)", o.fault);
    textBox(pg, "DIAGNOZA SERWISU", o.diagnosis);

    // ===== TABELA NAPRAW =====
    sectionHeader(pg, "WYKONANE NAPRAWY / CZĘŚCI", MARGIN, CONTENT_W);
    // Kolumny: Lp | Nazwa | Kod | Ilosc | Netto | VAT% | Brutto
    double colX[7] = {MARGIN, MARGIN + 28, MARGIN + 250, MARGIN + 310, MARGIN + 360, MARGIN + 430, MARGIN + 475};
    double colW2[7] = {28, 222, 60, 50, 70, 45, 40};
    auto tableHeader = [&](PageBuilder& p) {
        p.setFill(NAVY_R, NAVY_G, NAVY_B);
        p.rect(MARGIN, p.y - 18, CONTENT_W, 18, true);
        p.setFill(1, 1, 1);
        const char* headers[7] = {"Lp.", "Nazwa usługi / części", "Kod", "Ilość", "Netto [zł]", "VAT %", "Brutto [zł]"};
        for (int i = 0; i < 7; i++) {
            if (i >= 3) p.textRight(colX[i] + colW2[i] - 6, p.y - 12, headers[i], 8, true);
            else p.text(colX[i] + 6, p.y - 12, headers[i], 8, true);
        }
        p.y -= 18;
    };
    tableHeader(pg);

    int lp = 1;
    for (auto& r : o.repairs) {
        if (pg.y < 180) { // nowa strona
            pages.push_back(pg.stream.str());
            pg = newPage();
            pg.text(MARGIN, pg.y, "Zlecenie " + o.id + " - cd.", 9, true);
            pg.y -= 16;
            sectionHeader(pg, "WYKONANE NAPRAWY / CZĘŚCI (cd.)", MARGIN, CONTENT_W);
            tableHeader(pg);
        }
        double rowH = 18;
        // tło naprzemienne
        if (lp % 2 == 0) { pg.setFill(LIGHT_R, LIGHT_G, LIGHT_B); pg.rect(MARGIN, pg.y - rowH, CONTENT_W, rowH, true); }
        pg.setFill(DARK_R, DARK_G, DARK_B);
        pg.text(colX[0] + 6, pg.y - 12, std::to_string(lp), 8);
        // nazwa może się zawinąć — skracamy do jednej linii dla czytelności tabeli
        std::string nm = r.name;
        while (textWidth(nm, 8) > 210 && nm.size() > 4) nm = nm.substr(0, nm.size() - 2);
        if (nm != r.name) nm += "..";
        pg.text(colX[1] + 6, pg.y - 12, nm, 8);
        pg.text(colX[2] + 6, pg.y - 12, r.code, 8);
        pg.textRight(colX[3] + colW2[3] - 6, pg.y - 12, fmtNum(r.qty), 8);
        pg.textRight(colX[4] + colW2[4] - 6, pg.y - 12, fmtNum(r.net()), 8);
        pg.textRight(colX[5] + colW2[5] - 6, pg.y - 12, fmtNum(r.vatRate), 8);
        pg.textRight(colX[6] + colW2[6] - 6, pg.y - 12, fmtNum(r.gross()), 8, true);
        pg.setStroke(0.85, 0.88, 0.92); pg.setLineWidth(0.5);
        pg.line(MARGIN, pg.y - rowH, MARGIN + CONTENT_W, pg.y - rowH);
        pg.y -= rowH;
        lp++;
    }
    if (o.repairs.empty()) {
        pg.setFill(GREY_R, GREY_G, GREY_B);
        pg.text(MARGIN + 10, pg.y - 12, "Brak pozycji - diagnoza w trakcie.", 9);
        pg.y -= 18;
    }
    pg.y -= 8;

    // ===== PODSUMOWANIE KOSZTÓW =====
    {
        double boxW = 260;
        double boxX = MARGIN + CONTENT_W - boxW;
        double rows = 6;
        double boxH = rows * 16 + 8;
        pg.setFill(LIGHT_R, LIGHT_G, LIGHT_B);
        pg.rect(boxX, pg.y - boxH, boxW, boxH, true);
        pg.setStroke(0.85, 0.88, 0.92); pg.setLineWidth(0.5);
        pg.rect(boxX, pg.y - boxH, boxW, boxH, false);
        double ty = pg.y - 16;
        auto sumRow = [&](const std::string& label, double val, bool bold = false) {
            pg.setFill(DARK_R, DARK_G, DARK_B);
            pg.text(boxX + 10, ty, label, 9, bold);
            pg.textRight(boxX + boxW - 10, ty, fmtNum(val) + " zł", 9, bold);
            ty -= 16;
        };
        sumRow("Suma netto:", o.netTotal());
        sumRow("VAT:", o.vatTotal());
        sumRow("Suma brutto:", o.grossTotal(), true);
        if (o.discountPct > 0) sumRow("Rabat (" + fmtNum(o.discountPct) + "%):", -o.discountAmount());
        if (o.advance > 0) sumRow("Zaliczka:", -o.advance);
        // linia oddzielająca + do zapłaty
        pg.setStroke(BLUE_R, BLUE_G, BLUE_B); pg.setLineWidth(1);
        pg.line(boxX + 8, ty + 4, boxX + boxW - 8, ty + 4);
        ty -= 4;
        pg.setFill(BLUE_R, BLUE_G, BLUE_B);
        pg.text(boxX + 10, ty, "DO ZAPŁATY:", 11, true);
        pg.textRight(boxX + boxW - 10, ty, fmtNum(o.totalToPay()) + " zł", 11, true);
        pg.y -= boxH + 14;
    }

    // ===== UWAGI / GWARANCJA =====
    if (!o.notes.empty()) textBox(pg, "UWAGI", o.notes);
    {
        pg.setFill(LIGHT_R, LIGHT_G, LIGHT_B);
        pg.rect(MARGIN, pg.y - 20, CONTENT_W, 20, true);
        pg.setFill(DARK_R, DARK_G, DARK_B);
        pg.text(MARGIN + 10, pg.y - 14, "Gwarancja: " + o.warranty, 9);
        pg.y -= 30;
    }

    // ===== PODPISY + KOD KRESKOWY =====
    {
        double sigY = pg.y - 40;
        pg.setStroke(GREY_R, GREY_G, GREY_B); pg.setLineWidth(0.7);
        pg.line(MARGIN + 20, sigY, MARGIN + 200, sigY);
        pg.line(MARGIN + CONTENT_W - 220, sigY, MARGIN + CONTENT_W - 20, sigY);
        pg.setFill(GREY_R, GREY_G, GREY_B);
        pg.textCenter(MARGIN + 110, sigY - 14, "Podpis klienta", 8);
        pg.textCenter(MARGIN + CONTENT_W - 120, sigY - 14, "Podpis serwisu", 8);
        // Kod kreskowy (symulacja): paski wg znaków numeru zlecenia
        double bx = MARGIN + CONTENT_W / 2 - 60;
        double by = sigY - 50;
        pg.setFill(0, 0, 0);
        unsigned seed = 0;
        for (char c : o.id) seed = seed * 31 + (unsigned char)c;
        for (int i = 0; i < 60; i++) {
            seed = seed * 1103515245 + 12345;
            double w = ((seed >> 16) % 3) + 1;
            if ((seed >> 8) % 2 == 0) pg.rect(bx, by, w, 26, true);
            bx += w + 1.5;
        }
        pg.setFill(DARK_R, DARK_G, DARK_B);
        pg.textCenter(MARGIN + CONTENT_W / 2, by - 12, o.id, 8);
        pg.y = by - 30;
    }

    pages.push_back(pg.stream.str());
    int totalPages = (int)pages.size();

    // ===== SKŁADANIE PDF =====
    std::ostringstream pdf;
    pdf << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<size_t> offsets;
    auto addObject = [&](const std::string& body) {
        offsets.push_back((size_t)pdf.tellp());
        pdf << offsets.size() << " 0 obj\n" << body << "\nendobj\n";
    };

    // Obiekty: 1=Catalog, 2=Pages, 3=Font F1, 4=Font F2, potem pary Page/Content
    // Najpierw policzymy numery obiektów stron
    int firstPageObj = 6; // obiekty 1-4 + 5=ToUnicode
    // Pages obj (2) uzupełnimy po poznaniu liczby stron — zbudujemy wszystko w kolejności:
    // 1 Catalog, 2 Pages, 3 F1, 4 F2, 5.. Page/Content
    addObject("<< /Type /Catalog /Pages 2 0 R >>"); // 1
    {
        std::ostringstream pagesObj;
        pagesObj << "<< /Type /Pages /Count " << totalPages << " /Kids [";
        for (int i = 0; i < totalPages; i++) pagesObj << (firstPageObj + i * 2) << " 0 R ";
        pagesObj << "] >>";
        addObject(pagesObj.str()); // 2
    }
    // Kodowanie: CP1250 jako Differences wzgledem WinAnsi + ToUnicode (poprawne
    // polskie znaki w podgladzie, druku, wyszukiwaniu i kopiowaniu tekstu z PDF).
    const char* fontEnc = "/Encoding << /Type /Encoding /BaseEncoding /WinAnsiEncoding "
        "/Differences [140 /Sacute 143 /Zacute 156 /sacute 159 /zacute "
        "163 /Lslash 165 /Aogonek 175 /Zdotaccent 177 /aogonek 179 /lslash 191 /zdotaccent "
        "198 /Cacute 202 /Eogonek 209 /Nacute 230 /cacute 234 /eogonek 241 /nacute] >> /ToUnicode 5 0 R";
    addObject(std::string("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica ") + fontEnc + " >>"); // 3 F1
    addObject(std::string("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold ") + fontEnc + " >>"); // 4 F2
    {   // 5: ToUnicode CMap (bajt CP1250 -> Unicode)
        std::string cmap =
            "/CIDInit /ProcSet findresource begin\n"
            "12 dict begin\n"
            "begincmap\n"
            "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
            "/CMapName /Adobe-Identity-UCS def\n"
            "/CMapType 2 def\n"
            "1 begincodespacerange\n<00> <FF>\nendcodespacerange\n"
            "17 beginbfchar\n"
            "<8C> <015A>\n<8F> <0179>\n<9C> <015B>\n<9F> <017A>\n"
            "<A3> <0141>\n<A5> <0104>\n<AF> <017B>\n<B1> <0105>\n"
            "<B3> <0142>\n<BF> <017C>\n<C6> <0106>\n<CA> <0118>\n"
            "<D1> <0143>\n<E6> <0107>\n<EA> <0119>\n<F1> <0144>\n<F3> <00F3>\n"
            "endbfchar\n"
            "endcmap\n"
            "CMapName currentdict /CMap defineresource pop\n"
            "end\nend\n";
        std::ostringstream tu;
        tu << "<< /Length " << cmap.size() << " >>\nstream\n" << cmap << "endstream";
        addObject(tu.str()); // 5
    }

    for (int i = 0; i < totalPages; i++) {
        // Stopka strony: firma + numer strony + nr zlecenia
        std::string content = pages[i];
        {
            PageBuilder foot;
            foot.setFill(GREY_R, GREY_G, GREY_B);
            foot.setStroke(0.85, 0.88, 0.92); foot.setLineWidth(0.5);
            foot.line(MARGIN, 52, MARGIN + CONTENT_W, 52);
            foot.text(MARGIN, 40, company_.name + "  |  " + company_.phone + "  |  " + company_.email, 7);
            foot.textRight(MARGIN + CONTENT_W, 40,
                           "Zlecenie " + o.id + "  |  Strona " + std::to_string(i + 1) + " z " + std::to_string(totalPages), 7);
            content += foot.stream.str();
        }
        int pageObj = firstPageObj + i * 2;
        int contentObj = pageObj + 1;
        {
            std::ostringstream pageBody;
            pageBody << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << PAGE_W << " " << PAGE_H
                     << "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents " << contentObj << " 0 R >>";
            addObject(pageBody.str());
        }
        {
            std::ostringstream contentBody;
            contentBody << "<< /Length " << content.size() << " >>\nstream\n" << content << "endstream";
            addObject(contentBody.str());
        }
    }

    size_t xrefPos = (size_t)pdf.tellp();
    int objCount = (int)offsets.size() + 1;
    pdf << "xref\n0 " << objCount << "\n";
    pdf << "0000000000 65535 f \n";
    for (size_t off : offsets) {
        char buf[32]; snprintf(buf, sizeof(buf), "%010zu 00000 n \n", off);
        pdf << buf;
    }
    pdf << "trailer\n<< /Size " << objCount << " /Root 1 0 R >>\nstartxref\n" << xrefPos << "\n%%EOF\n";

    std::string s = pdf.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}
