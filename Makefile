# Szybkie budowanie serwera bez CMake (Linux/macOS, g++):
#   make            -> build/autoservis-server
#   make run        -> buduje i uruchamia na porcie 8080
#   make clean
# Na Windows użyj CMake (patrz README) albo Visual Studio.

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

SRC := server/src/main.cpp server/src/http_server.cpp server/src/api.cpp server/src/store.cpp server/src/pdf.cpp
BIN := build/autoservis-server

.PHONY: all run clean
all: $(BIN)

$(BIN): $(SRC) shared/json.hpp shared/order_model.hpp server/src/*.hpp
	mkdir -p build data
	$(CXX) $(CXXFLAGS) -I. -o $(BIN) $(SRC)

run: $(BIN)
	./$(BIN) --host 0.0.0.0 --port 8080 --data data/orders.json --web server/web

clean:
	rm -rf build
