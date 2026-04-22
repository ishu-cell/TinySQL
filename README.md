# ⚡ TinySQL — High-Performance Database Engine

A full-stack database system built from scratch in C++ with **B+ Tree indexing**, **Heap File storage**, **LRU Buffer Pool**, a built-in HTTP REST API, and a professional web dashboard — all with **zero external dependencies**.

## Architecture

```
┌──────────────────────────────────────████████████████████████████████████████████┐
│                            TinySQL v2.0 — Self-Contained                        │
│                                                                                 │
│  ┌──────────────────┐         HTTP/JSON         ┌──────────────────┐            │
│  │   Web Dashboard  │ ◄──────────────────────► │  C++ HTTP Server │            │
│  │   (Vanilla HTML) │    localhost:8080          │  (Winsock raw)   │            │
│  └──────────────────┘                           └────────┬─────────┘            │
│                                                          │                      │
│                              ┌────────────────────────────┤                      │
│                              │                            │                      │
│                    ┌─────────▼─────────┐      ┌──────────▼──────────┐           │
│                    │   B+ Tree Index   │      │   Heap File Store   │           │
│                    │   O(log n) find   │      │   O(1) insert       │           │
│                    │   data.db         │      │   O(n) scan         │           │
│                    └─────────┬─────────┘      │   heap.db           │           │
│                              │                └──────────┬──────────┘           │
│                    ┌─────────▼────────────────────────────▼──────────┐           │
│                    │         LRU Buffer Pool (1024 frames)          │           │
│                    │         4KB pages · evict-on-full              │           │
│                    └────────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## Features

### Core Engine (C++)
- **LRU Buffer Pool**: Hash map + doubly-linked list, max 1024 in-memory pages, supports 1M+ disk pages
- **B+ Tree Index**: O(log n) insert/find/delete with leaf splitting, internal node splitting, and root promotion
- **Heap File**: O(1) append insert, O(n) linear scan, tombstone deletion
- **Row Format**: `(uint32_t id, char[32] username, char[255] email)` — 291 bytes serialized
- **Persistence**: Both data.db and heap.db survive restarts via file-backed pages
- **Zero Dependencies**: Custom HTTP server using raw Winsock, manual JSON — no libraries needed

### REST API
| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/` | Dashboard UI |
| `GET` | `/api/health` | Health check |
| `GET` | `/api/stats` | Database statistics (rows, file sizes, buffer pool) |
| `POST` | `/api/seed` | Bulk insert synthetic rows `{"count": 10000}` |
| `GET` | `/api/search?id=X` | Search both B+ Tree and Heap, return timing |
| `POST` | `/api/benchmark` | Run 100 random insert/find/delete ops, return timing report |

### Web Dashboard
- **Stats Panel**: Live row counts, file sizes, page counts, buffer pool occupancy
- **Seed Control**: Bulk-insert up to 2M rows with progress and throughput display
- **Search**: Find by ID with side-by-side B+ Tree vs Heap timing
- **Performance Visualizer**: Animated bar charts comparing execution times
- **Benchmark Suite**: Comprehensive insert/find/delete comparison with detailed report

## Quick Start

### Prerequisites
- **g++** (MinGW on Windows) — C++17 compatible

### Build & Run
```bash
cd engine
g++ -std=c++17 -O2 -o tinysql-server.exe server.cpp pager.cpp btree.cpp heap.cpp -lws2_32
.\tinysql-server.exe
```

Open **http://localhost:8080** in your browser.

### Using the Makefile
```bash
cd engine
make server   # Build the HTTP server
make run      # Build and run
make repl     # Build the legacy REPL
make clean    # Remove binaries
```

## Project Structure

```
TinySQL/
├── engine/                      # C++ core — everything self-contained
│   ├── constants.hpp            # Page sizes, node layouts, heap constants
│   ├── row.hpp                  # Row struct, serialization, synthetic generation
│   ├── pager.hpp/cpp            # LRU Buffer Pool (1024 frames, hash map + LRU list)
│   ├── btree.hpp/cpp            # B+ Tree with internal splitting + lazy deletion
│   ├── heap.hpp/cpp             # Heap File with tombstone deletion
│   ├── http_server.hpp          # Custom HTTP server (raw Winsock, zero deps)
│   ├── server.cpp               # REST API server (primary entry point)
│   ├── main.cpp                 # Legacy REPL entry point
│   ├── public/
│   │   └── index.html           # Professional dashboard (vanilla HTML/JS/CSS)
│   └── Makefile
├── api/                         # (Legacy) Node.js API bridge
├── frontend/                    # (Legacy) React + Vite frontend
└── README.md
```

## B+ Tree Design

### Node Layout (per page = 4096 bytes)

**Leaf Node:**
```
[type:1B][root:1B][parent:4B][num_cells:4B][next_leaf:4B]
[key:4B][row:291B] × 13 cells max
```

**Internal Node:**
```
[type:1B][root:1B][parent:4B][num_keys:4B][right_child:4B]
[child:4B][key:4B] × 510 keys max
```

### Capacity
- **Leaf**: 13 rows per node
- **Internal**: 510 children per node
- **Tree depth 3**: supports ~3.4 million rows
- **Internal splitting**: supports arbitrary depth for unlimited scale

## Heap File Design

### Page Layout (4096 bytes)
```
[num_rows:4B]
[tombstone:1B][row:291B] × 14 slots max
```

- **Insert**: O(1) — append to last page
- **Find**: O(n) — linear scan all pages
- **Delete**: O(n) — scan + tombstone flag

## Performance Characteristics

| Operation | B+ Tree | Heap File |
|-----------|---------|-----------|
| Insert | O(log n) | O(1) |
| Find | O(log n) | O(n) |
| Delete | O(log n) | O(n) |

At 10,000 rows, B+ Tree find is typically **8-30x faster** than heap scan.

## License

MIT
