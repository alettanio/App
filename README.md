# AutoSerwis Pro — platforma zleceń samochodowych

Własna platforma dla **komputera, telefonu i przeglądarki**:

| Platforma | Technologia | Rola |
|---|---|---|
| **Komputer-serwer** (Windows/Linux) | C++17, serwer HTTP, własny generator PDF | **Jedyne miejsce przechowywania danych i tworzenia zleceń** (`data/orders.json`), generuje PDF, serwuje aplikację web |
| **Przeglądarka** (telefon/komputer) | HTML + CSS + JavaScript | Pełny interfejs: lista, wyszukiwanie, szczegóły, podgląd i pobieranie PDF; tworzenie/edycja **tylko z komputera-serwera** (localhost) |
| **Windows desktop** | C++17 + **Vulkan** + GLFW | Klient tylko-do-odczytu: lista i szczegóły zleceń, pobieranie/otwieranie PDF |
| **Android** | C++17 + **Vulkan** (NDK, NativeActivity) | Klient mobilny tylko-do-odczytu: lista i szczegóły zleceń, zapis PDF do katalogu aplikacji |

> Zasada bezpieczeństwa: serwer odrzuca `POST/PUT/DELETE` z innego adresu niż localhost
> (chyba że uruchomisz go świadomie z `--allow-remote-writes`). Telefon i drugi komputer
> mogą więc tylko **przeglądać zlecenia i pobierać PDF-y** — dane powstają wyłącznie
> na komputerze-serwerze.

## Co zawiera zlecenie i PDF

Zlecenie: klient (nazwa, telefon, e-mail, adres), pojazd (marka, model, rok, silnik, paliwo,
nr rejestracyjny, VIN, przebieg), daty przyjęcia i planowanego oddania, status, priorytet,
mechanik, zgłoszenie klienta, diagnoza serwisu, uwagi, gwarancja oraz lista napraw/części
(nazwa, kod, ilość, cena netto, VAT) z rabatem i zaliczką.

Zaawansowany generator PDF (czysty C++, bez bibliotek) tworzy czytelny dokument A4:
nagłówek firmy, sekcje Klient/Pojazd, status, zgłoszenie i diagnozę, uporządkowaną tabelę
napraw z VAT, podsumowanie kosztów (netto → VAT → brutto → rabat → zaliczka → **do zapłaty**),
gwarancję, miejsce na podpisy, kod kreskowy z numerem zlecenia, stopkę z numeracją stron
oraz poprawne polskie znaki (kodowanie + ToUnicode — tekst da się wyszukiwać i kopiować).
Długie listy napraw automatycznie dzielą się na kolejne strony.

## Struktura projektu

```
shared/                 Wspólny model C++: Order/RepairItem, obliczenia, JSON (bez zależności)
server/
  src/                  Serwer C++: HTTP, REST API, magazyn JSON, generator PDF
  web/                  Aplikacja przeglądarkowa (index.html, style.css, app.js)
clients/
  shared/               Wspólny klient natywny: HTTP, logika UI, renderer Vulkan, font 5x7
  shaders/              Shadery UI Vulkan (GLSL -> SPIR-V w CMake)
  cmake/                Kompilacja shaderów (glslangValidator z Vulkan SDK)
desktop/                Windows: Vulkan + GLFW (klient tylko-do-odczytu)
android/                Android: Gradle + NDK + Vulkan (NativeActivity)
data/orders.json        Dane zleceń (tworzone automatycznie przy pierwszym starcie)
```

## 1. Serwer (komputer-serwer) — start

### Linux / macOS (szybko, bez CMake)

```bash
make run
# albo ręcznie:
g++ -std=c++17 -O2 -pthread -I. -o build/autoservis-server \
  server/src/main.cpp server/src/http_server.cpp server/src/api.cpp server/src/store.cpp server/src/pdf.cpp
./build/autoservis-server --host 0.0.0.0 --port 8080 --data data/orders.json --web server/web
```

### Windows / Linux (CMake)

Wymaga CMake 3.16+ i kompilatora C++17 (Visual Studio 2022 / g++ / clang++).

```powershell
cmake -S . -B build -DAUTOSERVIS_BUILD_SERVER=ON
cmake --build build --config Release
.\build\server\Release\autoservis-server.exe --host 0.0.0.0 --port 8080
```

Po starcie:

- Przeglądarka (na tym komputerze): **http://localhost:8080/**
- Z telefonu w tej samej sieci Wi-Fi: `http://<IP-komputera>:8080/` (np. `http://192.168.1.20:8080/`)
- Dane zapisują się do `data/orders.json` (zapis atomowy; przy pierwszym starcie serwer tworzy 3 przykładowe zlecenia).

Opcje serwera: `--host`, `--port`, `--data <plik>`, `--web <katalog>`,
`--allow-remote-writes` (uwaga: pozwala tworzyć zlecenia z sieci LAN — domyślnie wyłączone).
Dane firmy na PDF edytujesz w `server/src/pdf.hpp` (`CompanyInfo`: nazwa, adres, NIP, telefon, e-mail, konto).

## 2. Przeglądarka (HTML/CSS/JS)

Nie wymaga budowania — serwer serwuje pliki z `server/web/`:

- lista zleceń z wyszukiwarką, filtrem statusu i sortowaniem, statystyki,
- formularz nowego/edycji zlecenia z dynamiczną tabelą napraw i podglądem kosztów na żywo,
- widok szczegółów, pobieranie PDF (`/api/orders/<id>/pdf`), drukowanie.

Na telefonie ikony tworzenia/edycji też są widoczne, ale serwer zwróci błąd 403 —
to celowa ochrona danych (tworzenie tylko na komputerze-serwerze).

## 3. Windows desktop (Vulkan)

Wymaga: **Vulkan SDK** (https://vulkan.lunarg.com — zawiera `glslangValidator`, nagłówki i loader)
oraz GLFW (zainstalowane albo automatycznie pobrane przez CMake FetchContent z GitHuba).

```powershell
cmake -S . -B build -DAUTOSERVIS_BUILD_SERVER=ON -DAUTOSERVIS_BUILD_DESKTOP=ON
cmake --build build --config Release
.\build\desktop\Release\autoservis-desktop.exe --server http://127.0.0.1:8080
```

Opcje: `--server <url>` (adres komputera-serwera), `--pdf-dir <katalog>` (gdzie zapisywać PDF-y;
domyślnie `./zlecenia-pdf`). Sterowanie: mysz (klik = wybór, przeciąganie/kółko = przewijanie),
`R`/`F5` = odśwież, `Esc` = wyjście. Kreślenie interfejsu robi własny renderer Vulkan
(`clients/shared/vulkan_ui.cpp`) — prostokąty i tekst bitmapowy rysowane jako geometria,
bez zewnętrznych bibliotek UI.

## 4. Android (Vulkan)

Wymaga: Android Studio (SDK 34, NDK, CMake 3.22.1) oraz Vulkan SDK na komputerze
(`glslangValidator` do kompilacji shaderów).

1. Uruchom serwer na komputerze i sprawdź jego IP w sieci LAN (np. `192.168.1.20`).
2. W `android/gradle.properties` ustaw:
   `autoservis.serverUrl=http://<IP-komputera>:8080`
   (dla emulatora zostaw `http://10.0.2.2:8080`).
3. Otwórz katalog `android/` w Android Studio → Run na telefonie (Android 7.0+, GPU z Vulkanem)
   **połączonym z tą samą siecią Wi-Fi** co serwer.
4. Aplikacja pokazuje zlecenia; przycisk **POBIERZ PDF** zapisuje plik do wewnętrznego
   katalogu aplikacji (ścieżka pojawia się na pasku statusu).

Budowanie z linii poleceń (po wygenerowaniu wrappera przez Android Studio):

```bash
cd android && ./gradlew assembleDebug
```

## API (serwer)

| Metoda | Ścieżka | Opis |
|---|---|---|
| GET | `/api/health` | Stan serwera |
| GET | `/api/stats` | Statystyki: liczba, aktywne, przychód, wg statusu |
| GET | `/api/orders?status=&search=` | Lista zleceń |
| GET | `/api/orders/<id>` | Jedno zlecenie |
| POST | `/api/orders` | Nowe zlecenie — **tylko localhost** |
| PUT | `/api/orders/<id>` | Edycja — **tylko localhost** |
| DELETE | `/api/orders/<id>` | Usunięcie — **tylko localhost** |
| GET | `/api/orders/<id>/pdf` | Dokument PDF zlecenia (dostępny też z telefonu) |

## Uwagi

- Brak zewnętrznych zależności w serwerze i kliencie HTTP: własny JSON (`shared/json.hpp`),
  własne sockety (Winsock/POSIX) i własny generator PDF.
- Aplikacje natywne (Vulkan) celowo nie mają formularza tworzenia zleceń — jeden wspólny
  model danych w `shared/order_model.hpp` jest używany przez serwer i klientów.
- Sieć LAN bez HTTPS: wystawiaj serwer tylko w zaufanej sieci lokalnej warsztatu.
