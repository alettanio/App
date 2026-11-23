// store.hpp — magazyn zleceń: pamięć + trwały zapis JSON (data/orders.json)
// Zapis atomowy (plik tymczasowy + rename), ochrona wątków (mutex).
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "../../shared/order_model.hpp"

class OrderStore {
public:
    explicit OrderStore(std::string filePath) : filePath_(std::move(filePath)) {}

    bool load();                       // wczytuje z pliku (tworzy seed, jeśli brak)
    std::vector<Order> all() const;    // kopia wszystkich zleceń
    bool get(const std::string& id, Order& out) const;
    bool create(const Order& o);       // false, jeśli id już istnieje
    bool update(const Order& o);       // false, jeśli brak
    bool remove(const std::string& id);
    std::string nextId() const;        // kolejny numer ZL-YYYY-NNNN

private:
    bool saveLocked() const;           // wołane z zablokowanym mutexem
    std::string filePath_;
    mutable std::mutex mutex_;
    std::vector<Order> orders_;
};
