#include "api.hpp"
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
    char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
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
    char buf[16]; strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

static std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [&](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) { out += (char)(hi * 16 + lo); i += 2; continue; }
        }
        out += (s[i] == '+') ? ' ' : s[i];
    }
    return out;
}

static std::map<std::string, std::string> parseQuery(const std::string& q) {
    std::map<std::string, std::string> m;
    std::stringstream ss(q);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq == std::string::npos) m[urlDecode(pair)] = "";
        else m[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
    }
    return m;
}

static std::vector<std::string> splitPath(const std::string& p) {
    std::vector<std::string> parts;
    std::stringstream ss(p);
    std::string part;
    while (std::getline(ss, part, '/')) if (!part.empty()) parts.push_back(part);
    return parts;
}

static std::string mimeFor(const std::string& path) {
    auto ends = [&](const std::string& ext) {
        return path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".css")) return "text/css; charset=utf-8";
    if (ends(".js")) return "application/javascript; charset=utf-8";
    if (ends(".json")) return "application/json; charset=utf-8";
    if (ends(".svg")) return "image/svg+xml";
    if (ends(".png")) return "image/png";
    if (ends(".ico")) return "image/x-icon";
    return "application/octet-stream";
}

static HttpResponse serveStatic(const std::string& webDir, const std::string& path) {
    std::string rel = (path == "/" || path.empty()) ? "index.html" : path.substr(1);
    // ochrona przed ../
    if (rel.find("..") != std::string::npos) return HttpResponse::text("Forbidden", 403);
    std::string full = webDir + "/" + rel;
    std::ifstream f(full, std::ios::binary);
    if (!f) return HttpResponse::text("Not Found", 404);
    std::stringstream ss; ss << f.rdbuf();
    HttpResponse r;
    r.status = 200;
    r.contentType = mimeFor(full);
    r.body = ss.str();
    return r;
}

static std::string lowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

HttpHandler makeHandler(OrderStore& store, const ServerConfig& cfg) {
    return [&store, cfg](const HttpRequest& req) -> HttpResponse {
        // CORS preflight
        if (req.method == "OPTIONS") return HttpResponse::text("", 204);

        auto parts = splitPath(req.path);

        // ---------- API ----------
        if (!parts.empty() && parts[0] == "api") {
            // GET /api/health
            if (parts.size() == 2 && parts[1] == "health" && req.method == "GET") {
                return HttpResponse::json("{\"status\":\"ok\",\"service\":\"AutoSerwis Pro\"}");
            }
            // GET /api/stats
            if (parts.size() == 2 && parts[1] == "stats" && req.method == "GET") {
                auto all = store.all();
                double revenue = 0; int active = 0;
                std::map<std::string, int> byStatus;
                for (auto& o : all) {
                    byStatus[o.status]++;
                    if (o.status == "Gotowe" || o.status == "Odebrane") revenue += o.grossTotal();
                    if (o.status == "Przyjęte" || o.status == "W realizacji" || o.status == "Oczekuje na części") active++;
                }
                mini::Value v = mini::Value::makeObject();
                v.object["total"] = mini::Value::makeNumber((double)all.size());
                v.object["active"] = mini::Value::makeNumber((double)active);
                v.object["revenue"] = mini::Value::makeNumber(revenue);
                mini::Value bs = mini::Value::makeObject();
                for (auto& kv : byStatus) bs.object[kv.first] = mini::Value::makeNumber((double)kv.second);
                v.object["byStatus"] = bs;
                return HttpResponse::json(mini::dump(v));
            }

            // /api/orders ...
            if (parts.size() >= 2 && parts[1] == "orders") {
                // GET /api/orders?status=&search=
                if (parts.size() == 2 && req.method == "GET") {
                    auto q = parseQuery(req.query);
                    auto all = store.all();
                    std::vector<Order> filtered;
                    for (auto& o : all) {
                        if (q.count("status") && o.status != q["status"]) continue;
                        if (q.count("search")) {
                            std::string s = lowerStr(q["search"]);
                            std::string hay = lowerStr(o.id + " " + o.clientName + " " + o.carBrand + " " + o.carModel + " " + o.carPlate);
                            if (hay.find(s) == std::string::npos) continue;
                        }
                        filtered.push_back(o);
                    }
                    mini::Value arr = mini::Value::makeArray();
                    for (auto& o : filtered) arr.array.push_back(o.toJson());
                    return HttpResponse::json(mini::dump(arr));
                }
                // POST /api/orders — tylko komputer-serwer (localhost)
                if (parts.size() == 2 && req.method == "POST") {
                    if (!cfg.allowRemoteWrites && !isLoopbackIp(req.remoteIp))
                        return HttpResponse::json("{\"error\":\"Tworzenie zlecen tylko na komputerze-serwerze\"}", 403);
                    try {
                        mini::Value v = mini::parse(req.body);
                        Order o = Order::fromJson(v);
                        if (o.id.empty()) o.id = store.nextId();
                        if (o.dateReceived.empty()) o.dateReceived = todayIso();
                        if (o.dateDue.empty()) o.dateDue = todayIso();
                        if (o.status.empty()) o.status = "Przyjęte";
                        o.createdAt = nowIso(); o.updatedAt = nowIso();
                        if (!store.create(o))
                            return HttpResponse::json("{\"error\":\"Zlecenie o tym numerze juz istnieje\"}", 409);
                        return HttpResponse::json(mini::dump(o.toJson()), 201);
                    } catch (const std::exception& e) {
                        return HttpResponse::json(std::string("{\"error\":\"") + e.what() + "\"}", 400);
                    }
                }
                // /api/orders/:id[/pdf]
                if (parts.size() >= 3) {
                    std::string id = urlDecode(parts[2]);
                    // GET /api/orders/:id/pdf
                    if (parts.size() == 4 && parts[3] == "pdf" && req.method == "GET") {
                        Order o;
                        if (!store.get(id, o)) return HttpResponse::json("{\"error\":\"Brak zlecenia\"}", 404);
                        PdfGenerator gen(cfg.company);
                        auto bytes = gen.generate(o);
                        HttpResponse r = HttpResponse::pdf(bytes);
                        r.headers["Content-Disposition"] = "attachment; filename=\"zlecenie-" + id + ".pdf\"";
                        return r;
                    }
                    // GET /api/orders/:id
                    if (parts.size() == 3 && req.method == "GET") {
                        Order o;
                        if (!store.get(id, o)) return HttpResponse::json("{\"error\":\"Brak zlecenia\"}", 404);
                        return HttpResponse::json(mini::dump(o.toJson()));
                    }
                    // PUT /api/orders/:id — tylko localhost
                    if (parts.size() == 3 && req.method == "PUT") {
                        if (!cfg.allowRemoteWrites && !isLoopbackIp(req.remoteIp))
                            return HttpResponse::json("{\"error\":\"Edycja zlecen tylko na komputerze-serwerze\"}", 403);
                        try {
                            mini::Value v = mini::parse(req.body);
                            Order o = Order::fromJson(v);
                            o.id = id;
                            Order existing;
                            if (store.get(id, existing)) o.createdAt = existing.createdAt;
                            o.updatedAt = nowIso();
                            if (!store.update(o)) return HttpResponse::json("{\"error\":\"Brak zlecenia\"}", 404);
                            return HttpResponse::json(mini::dump(o.toJson()));
                        } catch (const std::exception& e) {
                            return HttpResponse::json(std::string("{\"error\":\"") + e.what() + "\"}", 400);
                        }
                    }
                    // DELETE /api/orders/:id — tylko localhost
                    if (parts.size() == 3 && req.method == "DELETE") {
                        if (!cfg.allowRemoteWrites && !isLoopbackIp(req.remoteIp))
                            return HttpResponse::json("{\"error\":\"Usuwanie zlecen tylko na komputerze-serwerze\"}", 403);
                        if (!store.remove(id)) return HttpResponse::json("{\"error\":\"Brak zlecenia\"}", 404);
                        return HttpResponse::text("", 204);
                    }
                }
                return HttpResponse::json("{\"error\":\"Nieznana sciezka API\"}", 404);
            }
            return HttpResponse::json("{\"error\":\"Nieznana sciezka API\"}", 404);
        }

        // ---------- Pliki statyczne (przeglądarka) ----------
        return serveStatic(cfg.webDir, req.path);
    };
}
