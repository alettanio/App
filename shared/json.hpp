// json.hpp — minimalna, bez-zależności biblioteka JSON (C++17, header-only)
// Obsługuje: obiekty, tablice, stringi (UTF-8 + escape), liczby, bool, null.
// Używana przez serwer do parsowania/serializacji zleceń.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <cmath>

namespace mini {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string str;
    std::vector<Value> array;
    std::map<std::string, Value> object; // std::map = stabilna kolejność kluczy

    static Value makeBool(bool b) { Value v; v.type = Type::Bool; v.boolean = b; return v; }
    static Value makeNumber(double n) { Value v; v.type = Type::Number; v.number = n; return v; }
    static Value makeString(const std::string& s) { Value v; v.type = Type::String; v.str = s; return v; }
    static Value makeArray() { Value v; v.type = Type::Array; return v; }
    static Value makeObject() { Value v; v.type = Type::Object; return v; }

    bool isNull() const { return type == Type::Null; }
    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }

    const Value& at(const std::string& key) const {
        auto it = object.find(key);
        if (it == object.end()) throw std::runtime_error("Brak klucza JSON: " + key);
        return it->second;
    }
    Value& at(const std::string& key) {
        auto it = object.find(key);
        if (it == object.end()) throw std::runtime_error("Brak klucza JSON: " + key);
        return it->second;
    }
    const Value* find(const std::string& key) const {
        auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }
    std::string getString(const std::string& key, const std::string& def = "") const {
        auto* v = find(key); return (v && v->isString()) ? v->str : def;
    }
    double getNumber(const std::string& key, double def = 0) const {
        auto* v = find(key); return (v && v->isNumber()) ? v->number : def;
    }
    bool getBool(const std::string& key, bool def = false) const {
        auto* v = find(key); return (v && v->type == Type::Bool) ? v->boolean : def;
    }
};

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s), pos_(0) {}
    Value parse() {
        skipWs();
        Value v = parseValue();
        skipWs();
        if (pos_ != s_.size()) throw std::runtime_error("Nadmiarowe dane po JSON");
        return v;
    }
private:
    const std::string& s_;
    size_t pos_;
    void skipWs() { while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\r' || s_[pos_] == '\t')) pos_++; }
    char peek() { if (pos_ >= s_.size()) throw std::runtime_error("Koniec JSON"); return s_[pos_]; }
    char get() { if (pos_ >= s_.size()) throw std::runtime_error("Koniec JSON"); return s_[pos_++]; }
    void expect(char c) { skipWs(); if (get() != c) throw std::runtime_error(std::string("Oczekiwano '") + c + "'"); }

    Value parseValue() {
        skipWs();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return Value::makeString(parseString());
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') { parseLiteral("null"); return Value{}; }
        return parseNumber();
    }
    Value parseObject() {
        Value v = Value::makeObject();
        expect('{'); skipWs();
        if (peek() == '}') { get(); return v; }
        while (true) {
            skipWs();
            std::string key = parseString();
            expect(':');
            v.object[key] = parseValue();
            skipWs();
            char c = get();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("Oczekiwano ',' w obiekcie");
        }
        return v;
    }
    Value parseArray() {
        Value v = Value::makeArray();
        expect('['); skipWs();
        if (peek() == ']') { get(); return v; }
        while (true) {
            v.array.push_back(parseValue());
            skipWs();
            char c = get();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("Oczekiwano ',' w tablicy");
        }
        return v;
    }
    std::string parseString() {
        expect('"');
        std::string out;
        while (true) {
            char c = get();
            if (c == '"') break;
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned cp = parseHex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            // para surrogate
                            if (pos_ + 1 < s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                                pos_ += 2; unsigned lo = parseHex4();
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: throw std::runtime_error("Zly escape w stringu");
                }
            } else out += c;
        }
        return out;
    }
    unsigned parseHex4() {
        unsigned v = 0;
        for (int i = 0; i < 4; i++) {
            char c = get(); v <<= 4;
            if (c >= '0' && c <= '9') v += c - '0';
            else if (c >= 'a' && c <= 'f') v += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v += c - 'A' + 10;
            else throw std::runtime_error("Zly hex w \\u");
        }
        return v;
    }
    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    Value parseBool() {
        if (s_.compare(pos_, 4, "true") == 0) { pos_ += 4; return Value::makeBool(true); }
        if (s_.compare(pos_, 5, "false") == 0) { pos_ += 5; return Value::makeBool(false); }
        throw std::runtime_error("Zly bool");
    }
    void parseLiteral(const char* lit) {
        size_t n = std::string(lit).size();
        if (s_.compare(pos_, n, lit) != 0) throw std::runtime_error("Zly literal");
        pos_ += n;
    }
    Value parseNumber() {
        size_t start = pos_;
        if (peek() == '-') get();
        while (pos_ < s_.size() && ((s_[pos_] >= '0' && s_[pos_] <= '9') || s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E' || s_[pos_] == '+' || s_[pos_] == '-')) pos_++;
        if (start == pos_) throw std::runtime_error("Zla wartosc JSON");
        return Value::makeNumber(std::stod(s_.substr(start, pos_ - start)));
    }
};

inline std::string escapeString(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                else out += (char)c;
        }
    }
    out += "\"";
    return out;
}

inline void dumpInto(const Value& v, std::string& out, int indent) {
    std::string pad(indent * 2, ' ');
    std::string pad2((indent + 1) * 2, ' ');
    switch (v.type) {
        case Value::Type::Null: out += "null"; break;
        case Value::Type::Bool: out += v.boolean ? "true" : "false"; break;
        case Value::Type::Number: {
            if (std::floor(v.number) == v.number && std::abs(v.number) < 1e15) {
                out += std::to_string((long long)v.number);
            } else {
                std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(2);
                oss << v.number; out += oss.str();
            }
            break;
        }
        case Value::Type::String: out += escapeString(v.str); break;
        case Value::Type::Array: {
            if (v.array.empty()) { out += "[]"; break; }
            out += "[\n";
            for (size_t i = 0; i < v.array.size(); i++) {
                out += pad2; dumpInto(v.array[i], out, indent + 1);
                if (i + 1 < v.array.size()) out += ",";
                out += "\n";
            }
            out += pad + "]";
            break;
        }
        case Value::Type::Object: {
            if (v.object.empty()) { out += "{}"; break; }
            out += "{\n";
            size_t i = 0;
            for (auto& kv : v.object) {
                out += pad2 + escapeString(kv.first) + ": ";
                dumpInto(kv.second, out, indent + 1);
                if (++i < v.object.size()) out += ",";
                out += "\n";
            }
            out += pad + "}";
            break;
        }
    }
}

inline Value parse(const std::string& s) { return Parser(s).parse(); }
inline std::string dump(const Value& v) { std::string out; dumpInto(v, out, 0); return out; }

} // namespace mini
