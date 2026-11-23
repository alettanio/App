// order_model.hpp — wspólny model danych zlecenia samochodowego (C++17)
// Współdzielony przez: serwer (Windows/Linux), aplikację desktop (Windows, Vulkan),
// aplikację Android (Vulkan, NDK). Wersja przeglądarkowa (JS) odwzorowuje ten sam schemat.
#pragma once
#include <string>
#include <vector>
#include "json.hpp"

struct RepairItem {
    std::string name;      // nazwa usługi / części, np. "Wymiana klocków hamulcowych"
    std::string code;      // kod magazynowy / kod usługi, np. "HAM-001"
    double qty = 1.0;      // ilość
    double priceNet = 0.0; // cena netto za 1 szt.
    double vatRate = 23.0; // stawka VAT [%]

    double net() const { return qty * priceNet; }
    double vat() const { return net() * vatRate / 100.0; }
    double gross() const { return net() + vat(); }

    mini::Value toJson() const {
        mini::Value v = mini::Value::makeObject();
        v.object["name"] = mini::Value::makeString(name);
        v.object["code"] = mini::Value::makeString(code);
        v.object["qty"] = mini::Value::makeNumber(qty);
        v.object["priceNet"] = mini::Value::makeNumber(priceNet);
        v.object["vatRate"] = mini::Value::makeNumber(vatRate);
        return v;
    }
    static RepairItem fromJson(const mini::Value& v) {
        RepairItem r;
        r.name = v.getString("name");
        r.code = v.getString("code");
        r.qty = v.getNumber("qty", 1.0);
        r.priceNet = v.getNumber("priceNet", 0.0);
        r.vatRate = v.getNumber("vatRate", 23.0);
        return r;
    }
};

struct Order {
    std::string id;            // np. "ZL-2026-0001"
    // Klient
    std::string clientName;
    std::string clientPhone;
    std::string clientEmail;
    std::string clientAddress;
    // Pojazd
    std::string carBrand;      // np. "Volkswagen"
    std::string carModel;      // np. "Golf VIII"
    std::string carYear;       // np. "2021"
    std::string carPlate;      // nr rejestracyjny, np. "GD 12345"
    std::string carVin;        // VIN
    std::string carMileage;    // przebieg, np. "128 500 km"
    std::string carFuel;       // np. "Benzyna", "Diesel", "Hybryda", "Elektryczny"
    std::string carEngine;     // np. "2.0 TDI"
    // Terminy i status
    std::string dateReceived;  // YYYY-MM-DD
    std::string dateDue;       // YYYY-MM-DD (planowane oddanie)
    std::string status;        // Przyjęte | W realizacji | Oczekuje na części | Gotowe | Odebrane | Anulowane
    std::string priority;      // Niski | Normalny | Wysoki | Pilny
    // Opis
    std::string fault;         // zgłoszenie klienta (usterka)
    std::string diagnosis;     // diagnoza serwisu
    std::string notes;         // uwagi dodatkowe
    std::string mechanic;      // osoba przyjmująca / mechanik prowadzący
    std::string warranty = "12 miesięcy na części i robociznę";
    // Finanse
    double discountPct = 0.0;  // rabat [%]
    double advance = 0.0;      // zaliczka [PLN]
    std::vector<RepairItem> repairs;
    // Metadane
    std::string createdAt;     // ISO 8601
    std::string updatedAt;     // ISO 8601

    // --- Obliczenia finansowe ---
    double netTotal() const {
        double s = 0; for (auto& r : repairs) s += r.net(); return s;
    }
    double vatTotal() const {
        double s = 0; for (auto& r : repairs) s += r.vat(); return s;
    }
    double grossTotal() const { return netTotal() + vatTotal(); }
    double discountAmount() const { return grossTotal() * discountPct / 100.0; }
    double totalToPay() const {
        double v = grossTotal() - discountAmount() - advance;
        return v < 0 ? 0 : v;
    }

    mini::Value toJson() const {
        mini::Value v = mini::Value::makeObject();
        v.object["id"] = mini::Value::makeString(id);
        v.object["clientName"] = mini::Value::makeString(clientName);
        v.object["clientPhone"] = mini::Value::makeString(clientPhone);
        v.object["clientEmail"] = mini::Value::makeString(clientEmail);
        v.object["clientAddress"] = mini::Value::makeString(clientAddress);
        v.object["carBrand"] = mini::Value::makeString(carBrand);
        v.object["carModel"] = mini::Value::makeString(carModel);
        v.object["carYear"] = mini::Value::makeString(carYear);
        v.object["carPlate"] = mini::Value::makeString(carPlate);
        v.object["carVin"] = mini::Value::makeString(carVin);
        v.object["carMileage"] = mini::Value::makeString(carMileage);
        v.object["carFuel"] = mini::Value::makeString(carFuel);
        v.object["carEngine"] = mini::Value::makeString(carEngine);
        v.object["dateReceived"] = mini::Value::makeString(dateReceived);
        v.object["dateDue"] = mini::Value::makeString(dateDue);
        v.object["status"] = mini::Value::makeString(status);
        v.object["priority"] = mini::Value::makeString(priority);
        v.object["fault"] = mini::Value::makeString(fault);
        v.object["diagnosis"] = mini::Value::makeString(diagnosis);
        v.object["notes"] = mini::Value::makeString(notes);
        v.object["mechanic"] = mini::Value::makeString(mechanic);
        v.object["warranty"] = mini::Value::makeString(warranty);
        v.object["discountPct"] = mini::Value::makeNumber(discountPct);
        v.object["advance"] = mini::Value::makeNumber(advance);
        mini::Value arr = mini::Value::makeArray();
        for (auto& r : repairs) arr.array.push_back(r.toJson());
        v.object["repairs"] = arr;
        v.object["createdAt"] = mini::Value::makeString(createdAt);
        v.object["updatedAt"] = mini::Value::makeString(updatedAt);
        // pola obliczeniowe (tylko do odczytu, wygodne dla UI)
        v.object["netTotal"] = mini::Value::makeNumber(netTotal());
        v.object["vatTotal"] = mini::Value::makeNumber(vatTotal());
        v.object["grossTotal"] = mini::Value::makeNumber(grossTotal());
        v.object["totalToPay"] = mini::Value::makeNumber(totalToPay());
        return v;
    }

    static Order fromJson(const mini::Value& v) {
        Order o;
        o.id = v.getString("id");
        o.clientName = v.getString("clientName");
        o.clientPhone = v.getString("clientPhone");
        o.clientEmail = v.getString("clientEmail");
        o.clientAddress = v.getString("clientAddress");
        o.carBrand = v.getString("carBrand");
        o.carModel = v.getString("carModel");
        o.carYear = v.getString("carYear");
        o.carPlate = v.getString("carPlate");
        o.carVin = v.getString("carVin");
        o.carMileage = v.getString("carMileage");
        o.carFuel = v.getString("carFuel");
        o.carEngine = v.getString("carEngine");
        o.dateReceived = v.getString("dateReceived");
        o.dateDue = v.getString("dateDue");
        o.status = v.getString("status", "Przyjęte");
        o.priority = v.getString("priority", "Normalny");
        o.fault = v.getString("fault");
        o.diagnosis = v.getString("diagnosis");
        o.notes = v.getString("notes");
        o.mechanic = v.getString("mechanic");
        o.warranty = v.getString("warranty", "12 miesięcy na części i robociznę");
        o.discountPct = v.getNumber("discountPct", 0.0);
        o.advance = v.getNumber("advance", 0.0);
        o.createdAt = v.getString("createdAt");
        o.updatedAt = v.getString("updatedAt");
        if (auto* arr = v.find("repairs")) {
            if (arr->isArray())
                for (auto& item : arr->array) o.repairs.push_back(RepairItem::fromJson(item));
        }
        return o;
    }
};

// Formatowanie kwoty PLN: 1234.5 -> "1 234,50 zł"
inline std::string formatPLN(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", v);
    std::string s = buf;
    // zamień kropkę na przecinek i dodaj spacje tysięcy
    size_t dot = s.find('.');
    std::string intPart = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string fracPart = (dot == std::string::npos) ? "00" : s.substr(dot + 1);
    std::string out;
    bool neg = false;
    if (!intPart.empty() && intPart[0] == '-') { neg = true; intPart = intPart.substr(1); }
    for (size_t i = 0; i < intPart.size(); i++) {
        out += intPart[i];
        size_t remain = intPart.size() - i - 1;
        if (remain > 0 && remain % 3 == 0) out += ' ';
    }
    if (neg) out = "-" + out;
    return out + "," + fracPart + " zl";
}
