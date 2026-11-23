// main_android.cpp — aplikacja Android (Vulkan, NDK, NativeActivity).
// Klient tylko-do-odczytu: zlecenia i PDF z komputera-serwera w sieci LAN.
// Adres serwera: gradle.properties -> autoservis.serverUrl (patrz README).
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <fstream>
#include <string>

#include "client_app.hpp"
#include "client_http.hpp"
#include "vulkan_ui.hpp"
#include "client_config.h" // generowany przez CMake (adres serwera)

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AutoSerwisPro", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AutoSerwisPro", __VA_ARGS__)

struct Engine {
    android_app* app = nullptr;
    VulkanUi renderer;
    ClientApp* client = nullptr;
    bool rendererReady = false;
    float downX = 0, downY = 0, lastY = 0;
    bool touching = false;
    bool dragged = false;
};

static bool savePdf(Engine* e, const Order& order, std::string& savedPath) {
    ClientHttp http(kServerUrl);
    auto r = http.getBinary("/api/orders/" + urlEncodeSegment(order.id) + "/pdf");
    if (!r.ok) return false;
    std::string dir = e->app->activity->internalDataPath ? e->app->activity->internalDataPath : ".";
    savedPath = dir + "/zlecenie-" + order.id + ".pdf";
    std::ofstream f(savedPath, std::ios::binary);
    if (!f) return false;
    f.write((const char*)r.binary.data(), (std::streamsize)r.binary.size());
    return f.good();
}

static void initRenderer(Engine* e) {
    if (e->rendererReady || !e->app->window) return;
    if (!e->renderer.createInstance(false)) { LOGE("Vulkan createInstance failed"); return; }
    VkAndroidSurfaceCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    ci.window = e->app->window;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateAndroidSurfaceKHR(e->renderer.instance(), &ci, nullptr, &surface) != VK_SUCCESS) {
        LOGE("vkCreateAndroidSurfaceKHR failed"); return;
    }
    int32_t w = ANativeWindow_getWidth(e->app->window);
    int32_t h = ANativeWindow_getHeight(e->app->window);
    if (!e->renderer.init(surface, (uint32_t)w, (uint32_t)h)) { LOGE("Vulkan init failed"); return; }
    e->rendererReady = true;
    if (e->client) e->client->refresh();
}

static void handleCmd(android_app* app, int32_t cmd) {
    auto* e = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            initRenderer(e);
            break;
        case APP_CMD_TERM_WINDOW:
            if (e->rendererReady) { e->renderer.shutdown(); e->rendererReady = false; }
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
            if (e->rendererReady && app->window) {
                e->renderer.resize((uint32_t)ANativeWindow_getWidth(app->window),
                                   (uint32_t)ANativeWindow_getHeight(app->window));
            }
            break;
        default: break;
    }
}

static int32_t handleInput(android_app* app, AInputEvent* event) {
    auto* e = (Engine*)app->userData;
    if (!e || !e->client) return 0;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    float x = AMotionEvent_getX(event, 0);
    float y = AMotionEvent_getY(event, 0);
    int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    if (action == AMOTION_EVENT_ACTION_DOWN) {
        e->touching = true; e->dragged = false; e->downX = x; e->downY = y; e->lastY = y;
        return 1;
    }
    if (action == AMOTION_EVENT_ACTION_MOVE && e->touching) {
        float dy = y - e->lastY;
        if (std::abs(x - e->downX) + std::abs(y - e->downY) > 24) e->dragged = true;
        if (e->dragged && dy != 0) e->client->onDrag(0, dy, x, y);
        e->lastY = y;
        return 1;
    }
    if (action == AMOTION_EVENT_ACTION_UP) {
        e->touching = false;
        if (!e->dragged) e->client->onTap(x, y);
        return 1;
    }
    return 0;
}

void android_main(android_app* app) {
    app_dummy(); // wymagane przez native_app_glue
    Engine engine;
    engine.app = app;
    ClientApp client(kServerUrl);
    engine.client = &client;
    client.setPdfDownloader([&](const Order& order, std::string& savedPath) {
        return savePdf(&engine, order, savedPath);
    });

    app->userData = &engine;
    app->onAppCmd = handleCmd;
    app->onInputEvent = handleInput;

    LOGI("AutoSerwis Pro Android, serwer: %s", kServerUrl);

    while (true) {
        int ident, events;
        android_poll_source* source = nullptr;
        int timeout = engine.rendererReady ? 0 : -1;
        while ((ident = ALooper_pollOnce(timeout, nullptr, &events, (void**)&source)) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) {
                if (engine.rendererReady) engine.renderer.shutdown();
                return;
            }
            timeout = 0;
        }
        if (engine.rendererReady && app->window) {
            float w = (float)ANativeWindow_getWidth(app->window);
            float h = (float)ANativeWindow_getHeight(app->window);
            if (w > 0 && h > 0) engine.renderer.render(client.draw(w, h));
        }
    }
}
