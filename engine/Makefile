CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
REPL_SRCS  = main.cpp pager.cpp btree.cpp
SERVER_SRCS = server.cpp pager.cpp btree.cpp heap.cpp
HEADERS  = constants.hpp row.hpp pager.hpp btree.hpp heap.hpp http_server.hpp

ifeq ($(OS),Windows_NT)
    EXE_EXT  = .exe
    RM_CMD   = del /Q
    LIBS     = -lws2_32
else
    EXE_EXT  =
    RM_CMD   = rm -f
    LIBS     =
endif

# Default: build the HTTP server
all: server

# ── HTTP Server (primary) ──
server: tinysql-server$(EXE_EXT)

tinysql-server$(EXE_EXT): $(SERVER_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_SRCS) $(LIBS)

# ── REPL (legacy) ──
repl: tinysql$(EXE_EXT)

tinysql$(EXE_EXT): $(REPL_SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(REPL_SRCS)

# ── Run ──
run: server
	./tinysql-server$(EXE_EXT)

# ── Clean ──
clean:
	$(RM_CMD) tinysql$(EXE_EXT) tinysql-server$(EXE_EXT)

.PHONY: all server repl run clean
