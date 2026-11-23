// pdf.hpp — zaawansowany generator PDF zleceń serwisowych (czysty C++17, bez bibliotek)
// Tworzy dokument PDF 1.4: nagłówek firmy, dane klienta i pojazdu, opis usterki,
// tabela napraw z VAT, podsumowanie kosztów, podpisy, kod kreskowy, stopka ze
// numeracją stron. Obsługuje polskie znaki (WinAnsiEncoding) i podział na strony.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../../shared/order_model.hpp"

struct CompanyInfo {
    std::string name = "AutoSerwis Pro";
    std::string address = "ul. Warsztatowa 12, 80-001 Gdansk";
    std::string nip = "583-000-00-00";
    std::string phone = "+48 600 100 200";
    std::string email = "serwis@autoserwispro.pl";
    std::string bank = "PL 00 0000 0000 0000 0000 0000 0000";
};

class PdfGenerator {
public:
    explicit PdfGenerator(const CompanyInfo& company = CompanyInfo()) : company_(company) {}

    // Generuje kompletny dokument PDF dla zlecenia. Zwraca bajty pliku.
    std::vector<uint8_t> generate(const Order& order) const;

private:
    CompanyInfo company_;
};
