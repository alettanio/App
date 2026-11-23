#include "store.hpp"
#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

static std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

static std::string todayIso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

bool OrderStore::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream f(filePath_);
    if (!f) {
        // Brak pliku — tworzymy przykładowe dane startowe
        orders_.clear();
        auto mk = [&](const std::string& id, const std::string& client, const std::string& brand,
                      const std::string& model, const std::string& status, double gross) {
            Order o;
            o.id = id; o.clientName = client; o.carBrand = brand; o.carModel = model;
            o.status = status; o.dateReceived = todayIso(); o.dateDue = todayIso();
            o.createdAt = nowIso(); o.updatedAt = nowIso();
            o.clientPhone = "+48 600 000 000"; o.carPlate = "GD 00000";
            o.fault = "Przegląd okresowy i wymiana oleju.";
            o.diagnosis = "Do ustalenia po diagnostyce komputerowej.";
            o.mechanic = "Jan Kowalski";
            RepairItem r; r.name = "Przegląd okresowy"; r.code = "PRZ-001";
            r.qty = 1; r.priceNet = gross / 1.23; r.vatRate = 23;
            o.repairs.push_back(r);
            orders_.push_back(o);
        };
        mk("ZL-2026-0001", "Anna Nowak", "Volkswagen", "Golf VIII", "W realizacji", 899.00);
        mk("ZL-2026-0002", "Piotr Kowalski", "Toyota", "Corolla", "Przyjęte", 1250.00);
        mk("ZL-2026-0003", "Maria Nowak", "Skoda", "Octavia", "Gotowe", 2100.00);
        return saveLocked();
    }
    std::stringstream ss; ss << f.rdbuf();
    try {
        mini::Value root = mini::parse(ss.str());
        orders_.clear();
        if (root.isArray())
            for (auto& item : root.array) orders_.push_back(Order::fromJson(item));
        return true;
    } catch (const std::exception& e) {
        // Uszkodzony plik — start z pustej listy, nie wywalaj serwera
        orders_.clear();
        return false;
    }
}

bool OrderStore::saveLocked() const {
    mini::Value root = mini::Value::makeArray();
    for (auto& o : orders_) root.array.push_back(o.toJson());
    std::string tmp = filePath_ + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary);
        if (!f) return false;
        f << mini::dump(root);
    }
    // atomowy rename (na Windows MoveFileEx, na POSIX rename)
#if defined(_WIN32)
    MoveFileExA(tmp.c_str(), filePath_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    std::rename(tmp.c_str(), filePath_.c_str());
#endif
    return true;
}

std::vector<Order> OrderStore::all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return orders_;
}

bool OrderStore::get(const std::string& id, Order& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& o : orders_) if (o.id == id) { out = o; return true; }
    return false;
}

bool OrderStore::create(const Order& o) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& x : orders_) if (x.id == o.id) return false;
    orders_.push_back(o);
    return saveLocked();
}

bool OrderStore::update(const Order& o) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& x : orders_) if (x.id == o.id) { x = o; return saveLocked(); }
    return false;
}

bool OrderStore::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(orders_.begin(), orders_.end(),
                             [&](const Order& o) { return o.id == id; });
    if (it == orders_.end()) return false;
    orders_.erase(it, orders_.end());
    return saveLocked();
}

std::string OrderStore::nextId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ybuf[8]; strftime(ybuf, sizeof(ybuf), "%Y", &tm);
    int maxN = 0;
    std::string prefix = std::string("ZL-") + ybuf + "-";
    for (auto& o : orders_) {
        if (o.id.rfind(prefix, 0) == 0) {
            try { maxN = std::max(maxN, std::stoi(o.id.substr(prefix.size()))); } catch (...) {}
        }
    }
    std::ostringstream oss;
    oss << prefix << std::setw(4) << std::setfill('0') << (maxN + 1);
    return oss.str();
}
