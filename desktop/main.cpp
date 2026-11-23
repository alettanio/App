// main.cpp — aplikacja desktopowa Windows (Vulkan + GLFW), klient tylko-do-odczytu.
// Łączy się z komputerem-serwerem AutoSerwis Pro (HTTP API), pokazuje zlecenia
// i pobiera PDF. Tworzenie zleceń: tylko komputer-serwer (patrz README).
//
// Użycie: autoservis-desktop [--server http://127.0.0.1:8080] [--pdf-dir katalog]
#define GLFW_INCLUDE_VULKAN // deklaruje m.in. glfwCreateWindowSurface i samo dolacza <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../clients/shared/client_app.hpp"
#include "../clients/shared/vulkan_ui.hpp"

namespace fs = std::filesystem;

struct AppState {
    ClientApp* app = nullptr;
    VulkanUi* renderer = nullptr;
    double mouseX = 0, mouseY = 0;
    double downX = 0, downY = 0, lastY = 0;
    bool down = false;
    bool dragged = false;
};

static void saveAndOpenPdf(ClientApp& app, const std::string& serverUrl, const fs::path& pdfDir,
                           const Order& order, std::string& savedPath) {
    (void)app;
    ClientHttp http(serverUrl);
    auto r = http.getBinary("/api/orders/" + urlEncodeSegment(order.id) + "/pdf");
    if (!r.ok) return;
    std::error_code ec;
    fs::create_directories(pdfDir, ec);
    fs::path file = pdfDir / ("zlecenie-" + order.id + ".pdf");
    std::ofstream f(file, std::ios::binary);
    if (!f) return;
    f.write((const char*)r.binary.data(), (std::streamsize)r.binary.size());
    f.close();
    savedPath = file.string();
#if defined(_WIN32)
    // Otwórz PDF domyślną przeglądarką PDF w Windows.
    std::string cmd = "start \"\" \"" + savedPath + "\"";
    std::system(cmd.c_str());
#elif defined(__APPLE__)
    std::string cmd = "open \"" + savedPath + "\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + savedPath + "\" >/dev/null 2>&1 &";
    std::system(cmd.c_str());
#endif
}

int main(int argc, char** argv) {
    std::string serverUrl = "http://127.0.0.1:8080";
    fs::path pdfDir = fs::current_path() / "zlecenia-pdf";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--server" && i + 1 < argc) serverUrl = argv[++i];
        else if (a == "--pdf-dir" && i + 1 < argc) pdfDir = argv[++i];
        else if (a == "--help" || a == "-h") {
            std::cout << "autoservis-desktop [--server http://127.0.0.1:8080] [--pdf-dir katalog]\n";
            return 0;
        }
    }

    if (!glfwInit()) { std::cerr << "Nie mozna zainicjowac GLFW\n"; return 1; }
    if (!glfwVulkanSupported()) { std::cerr << "GLFW: Vulkan niedostepny (sterownik/loader)\n"; return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 820, "AutoSerwis Pro — Zlecenia (Vulkan)", nullptr, nullptr);
    if (!window) { std::cerr << "Nie mozna utworzyc okna\n"; return 1; }

    VulkanUi renderer;
    if (!renderer.createInstance(false)) { std::cerr << "Vulkan: createInstance nie powiodlo sie\n"; return 1; }
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(renderer.instance(), window, nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "Vulkan: nie mozna utworzyc surface\n"; return 1;
    }
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (!renderer.init(surface, (uint32_t)fbW, (uint32_t)fbH)) {
        std::cerr << "Vulkan: inicjalizacja renderera nie powiodla sie\n"; return 1;
    }

    ClientApp app(serverUrl);
    app.setPdfDownloader([&](const Order& order, std::string& savedPath) {
        auto before = savedPath;
        saveAndOpenPdf(app, serverUrl, pdfDir, order, savedPath);
        return savedPath != before && !savedPath.empty();
    });

    AppState state{&app, &renderer};
    glfwSetWindowUserPointer(window, &state);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
        auto* s = (AppState*)glfwGetWindowUserPointer(w);
        if (s && s->renderer) s->renderer->resize((uint32_t)width, (uint32_t)height);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
        auto* s = (AppState*)glfwGetWindowUserPointer(w);
        if (!s) return;
        s->mouseX = x; s->mouseY = y;
        if (s->down) {
            double dy = y - s->lastY;
            if (std::abs(x - s->downX) + std::abs(y - s->downY) > 6) s->dragged = true;
            if (s->dragged && dy != 0) s->app->onDrag(0, (float)dy, (float)x, (float)y);
        }
        s->lastY = y;
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
        (void)mods;
        auto* s = (AppState*)glfwGetWindowUserPointer(w);
        if (!s || button != GLFW_MOUSE_BUTTON_LEFT) return;
        if (action == GLFW_PRESS) {
            s->down = true; s->dragged = false; s->downX = s->mouseX; s->downY = s->mouseY; s->lastY = s->mouseY;
        } else if (action == GLFW_RELEASE) {
            s->down = false;
            if (!s->dragged) s->app->onTap((float)s->mouseX, (float)s->mouseY);
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* w, double xoff, double yoff) {
        (void)xoff;
        auto* s = (AppState*)glfwGetWindowUserPointer(w);
        if (s) s->app->onScroll((float)yoff, (float)s->mouseX, (float)s->mouseY);
    });
    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int sc, int action, int mods) {
        (void)sc; (void)mods;
        auto* s = (AppState*)glfwGetWindowUserPointer(w);
        if (!s) return;
        if (action == GLFW_PRESS && (key == GLFW_KEY_R || key == GLFW_KEY_F5)) s->app->refresh();
        if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
    });

    std::cout << "AutoSerwis Pro (desktop Vulkan)\nSerwer: " << serverUrl << "\nPDF: " << pdfDir << "\n";
    app.refresh(); // pierwsze pobranie (LAN; przy braku serwera pokaże błąd na pasku statusu)

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW > 0 && fbH > 0) renderer.render(app.draw((float)fbW, (float)fbH));
    }

    renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
