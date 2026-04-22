/*
 * server.cpp — TinySQL HTTP REST API Server
 *
 * Self-contained C++ HTTP server with zero external dependencies.
 * Uses custom http_server.hpp (raw Winsock) and manual JSON building.
 *
 * Endpoints:
 *   GET  /              — Serves the dashboard (public/index.html)
 *   GET  /api/health    — Health check
 *   GET  /api/stats     — Database statistics
 *   POST /api/seed      — Bulk insert synthetic rows
 *   GET  /api/search    — Search by ID in both B+ Tree and Heap
 *   POST /api/benchmark — Run comparative benchmark
 *
 * Build:
 *   g++ -std=c++17 -O2 -o tinysql-server.exe server.cpp pager.cpp btree.cpp heap.cpp -lws2_32
 */

#include "http_server.hpp"
#include "constants.hpp"
#include "row.hpp"
#include "pager.hpp"
#include "btree.hpp"
#include "heap.hpp"

#include <iostream>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>

using Clock = std::chrono::high_resolution_clock;

// ============================================================
//  Simple JSON helpers (no external library needed)
// ============================================================

static std::string jstr(const std::string& key, const std::string& val) {
    return "\"" + key + "\":\"" + json_escape(val) + "\"";
}
static std::string jnum(const std::string& key, double val) {
    std::ostringstream oss;
    oss << "\"" << key << "\":" << val;
    return oss.str();
}
static std::string jint(const std::string& key, uint64_t val) {
    return "\"" + key + "\":" + std::to_string(val);
}
static std::string jbool(const std::string& key, bool val) {
    return "\"" + key + "\":" + (val ? "true" : "false");
}

// Simple JSON body parser — extracts "count" from {"count": 12345}
static uint32_t parse_count_from_json(const std::string& body) {
    // Try "count" (with quotes)
    size_t pos = body.find("count");
    if (pos == std::string::npos) return 0;
    pos = body.find(':', pos);
    if (pos == std::string::npos) return 0;
    pos++;
    // Skip whitespace after colon
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' || body[pos] == '\r')) pos++;
    // Parse number
    std::string num_str;
    while (pos < body.size() && body[pos] >= '0' && body[pos] <= '9') {
        num_str += body[pos];
        pos++;
    }
    if (num_str.empty()) return 0;
    return static_cast<uint32_t>(std::stoul(num_str));
}

// ============================================================
//  Global Database State
// ============================================================

struct Database {
    Pager     btree_pager;
    HeapFile  heap;
    uint32_t  root_page_num;

    Database()
        : btree_pager("data.db")
        , heap("heap.db")
        , root_page_num(0)
    {
        if (btree_pager.num_pages() == 0) {
            void* root = btree_pager.get_page(0);
            initialize_leaf_node(root);
            set_node_root(root, true);
            btree_pager.mark_dirty(0);
        }
    }

    ~Database() {
        btree_pager.flush_all();
    }
};

static Database* g_db = nullptr;

// ============================================================
//  Helpers
// ============================================================

static std::string read_file_to_string(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// High-precision timer using QueryPerformanceCounter on Windows
#ifdef _WIN32
#include <windows.h>
template<typename Func>
static double measure_us(Func fn) {
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    fn();
    QueryPerformanceCounter(&t1);
    return static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000000.0 / static_cast<double>(freq.QuadPart);
}
#else
template<typename Func>
static double measure_us(Func fn) {
    auto t0 = Clock::now();
    fn();
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
}
#endif

// ============================================================
//  Main
// ============================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  TinySQL v2.0 — Database Engine Server" << std::endl;
    std::cout << "========================================" << std::endl;

    g_db = new Database();
    std::cout << "[TinySQL] Database initialized. B+ Tree pages: "
              << g_db->btree_pager.num_pages() << std::endl;

    HttpServer svr;

    // ── Serve static dashboard ──
    svr.Get("/", [](const HttpRequest&, HttpResponse& res) {
        std::string html = read_file_to_string("public/index.html");
        if (html.empty()) {
            res.status_code = 404;
            res.json("{\"status\":\"error\",\"message\":\"public/index.html not found\"}");
            return;
        }
        res.html(html);
    });

    // ── Health ──
    svr.Get("/api/health", [](const HttpRequest&, HttpResponse& res) {
        res.json("{" + jstr("status", "ok") + "," + jstr("engine", "TinySQL v2.0") + "}");
    });

    // ── Stats ──
    svr.Get("/api/stats", [](const HttpRequest&, HttpResponse& res) {
        uint32_t btree_rows = btree_count(g_db->btree_pager, g_db->root_page_num);
        uint32_t heap_rows  = g_db->heap.count();

        std::string j = "{"
            + jint("btree_rows", btree_rows) + ","
            + jint("heap_rows", heap_rows) + ","
            + jint("btree_file_size", g_db->btree_pager.file_size()) + ","
            + jint("heap_file_size", g_db->heap.file_size()) + ","
            + jint("btree_pages", g_db->btree_pager.num_pages()) + ","
            + jint("heap_pages", g_db->heap.pager().num_pages()) + ","
            + jint("buffer_pool", g_db->btree_pager.pool_occupancy())
            + "}";
        res.json(j);
    });

    // ── Seed ──
    svr.Post("/api/seed", [](const HttpRequest& req, HttpResponse& res) {
        uint32_t count = parse_count_from_json(req.body);
        if (count == 0 || count > 2000000) {
            res.status_code = 400;
            res.json("{" + jstr("status", "error") + "," + jstr("message", "count must be 1..2000000") + "}");
            return;
        }

        auto t0 = Clock::now();

        uint32_t inserted_btree = 0, inserted_heap = 0, skipped = 0;

        for (uint32_t i = 1; i <= count; i++) {
            Row row = generate_synthetic_row(i);

            // B+ Tree insert (skip duplicates)
            Cursor cursor = btree_find(g_db->btree_pager, g_db->root_page_num, row.id);
            void* node = g_db->btree_pager.get_page(cursor.page_num);
            uint32_t nc = *leaf_node_num_cells(node);
            if (cursor.cell_num < nc && *leaf_node_key(node, cursor.cell_num) == row.id) {
                skipped++;
            } else {
                leaf_node_insert(cursor, row.id, row);
                inserted_btree++;
            }

            // Heap insert (always appends)
            g_db->heap.insert(row);
            inserted_heap++;

            // Periodic flush to avoid huge dirty page accumulation
            if (i % 10000 == 0) {
                g_db->btree_pager.flush_all();
                g_db->heap.pager().flush_all();
            }
        }

        g_db->btree_pager.flush_all();
        g_db->heap.pager().flush_all();

        auto t1 = Clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double rows_per_sec = (count / (elapsed_ms / 1000.0));

        std::string j = "{"
            + jstr("status", "success") + ","
            + jint("inserted_btree", inserted_btree) + ","
            + jint("inserted_heap", inserted_heap) + ","
            + jint("skipped", skipped) + ","
            + jnum("elapsed_ms", elapsed_ms) + ","
            + jnum("rows_per_sec", rows_per_sec)
            + "}";
        res.json(j);
    });

    // ── Search ──
    svr.Get("/api/search", [](const HttpRequest& req, HttpResponse& res) {
        if (!req.has_param("id")) {
            res.status_code = 400;
            res.json("{" + jstr("status", "error") + "," + jstr("message", "Missing query param: id") + "}");
            return;
        }

        uint32_t id = static_cast<uint32_t>(std::atoi(req.get_param("id").c_str()));

        // --- B+ Tree search ---
        std::string btree_result;
        double btree_us = measure_us([&]() {
            Cursor cursor = btree_find(g_db->btree_pager, g_db->root_page_num, id);
            void* node = g_db->btree_pager.get_page(cursor.page_num);
            uint32_t nc = *leaf_node_num_cells(node);

            if (cursor.cell_num < nc && *leaf_node_key(node, cursor.cell_num) == id) {
                Row row;
                deserialize_row(leaf_node_value(node, cursor.cell_num), row);
                btree_result = "{"
                    + jbool("found", true) + ","
                    + "\"row\":{" + jint("id", row.id) + ","
                    + jstr("username", row.username) + ","
                    + jstr("email", row.email) + "}"
                    + "}";
            } else {
                btree_result = "{" + jbool("found", false) + "}";
            }
        });

        // Inject time_us into btree_result
        btree_result = btree_result.substr(0, btree_result.size() - 1) + "," + jnum("time_us", btree_us) + "}";

        // --- Heap search ---
        std::string heap_result;
        double heap_us = measure_us([&]() {
            Row row;
            if (g_db->heap.find(id, row)) {
                heap_result = "{"
                    + jbool("found", true) + ","
                    + "\"row\":{" + jint("id", row.id) + ","
                    + jstr("username", row.username) + ","
                    + jstr("email", row.email) + "}"
                    + "}";
            } else {
                heap_result = "{" + jbool("found", false) + "}";
            }
        });

        heap_result = heap_result.substr(0, heap_result.size() - 1) + "," + jnum("time_us", heap_us) + "}";

        double speedup = (heap_us > 0 && btree_us > 0) ? (heap_us / btree_us) : 0.0;

        std::string j = "{"
            + jstr("status", "success") + ","
            + jint("id", id) + ","
            + "\"btree\":" + btree_result + ","
            + "\"heap\":" + heap_result + ","
            + jnum("speedup", speedup)
            + "}";
        res.json(j);
    });

    // ── Benchmark ──
    svr.Post("/api/benchmark", [](const HttpRequest&, HttpResponse& res) {
        std::srand(42);

        uint32_t max_id = btree_count(g_db->btree_pager, g_db->root_page_num);
        if (max_id < 10) max_id = 1000;

        const int NUM_OPS = 100;

        // Generate random IDs
        std::vector<uint32_t> test_ids(NUM_OPS);
        for (int i = 0; i < NUM_OPS; i++) {
            test_ids[i] = 1 + (std::rand() % max_id);
        }

        // --- INSERT benchmark ---
        double btree_insert_us = 0, heap_insert_us = 0;
        uint32_t base_id = max_id + 1000000;
        for (int i = 0; i < NUM_OPS; i++) {
            Row row = generate_synthetic_row(base_id + i);

            btree_insert_us += measure_us([&]() {
                Cursor c = btree_find(g_db->btree_pager, g_db->root_page_num, row.id);
                leaf_node_insert(c, row.id, row);
            });

            heap_insert_us += measure_us([&]() {
                g_db->heap.insert(row);
            });
        }

        // --- FIND benchmark ---
        double btree_find_us = 0, heap_find_us = 0;
        for (int i = 0; i < NUM_OPS; i++) {
            uint32_t search_id = test_ids[i];

            btree_find_us += measure_us([&]() {
                Cursor c = btree_find(g_db->btree_pager, g_db->root_page_num, search_id);
                void* node = g_db->btree_pager.get_page(c.page_num);
                (void)node;
            });

            heap_find_us += measure_us([&]() {
                Row r;
                g_db->heap.find(search_id, r);
            });
        }

        // --- DELETE benchmark ---
        double btree_delete_us = 0, heap_delete_us = 0;
        for (int i = 0; i < NUM_OPS; i++) {
            uint32_t del_id = base_id + i;

            btree_delete_us += measure_us([&]() {
                btree_delete(g_db->btree_pager, g_db->root_page_num, del_id);
            });

            heap_delete_us += measure_us([&]() {
                g_db->heap.remove(del_id);
            });
        }

        g_db->btree_pager.flush_all();
        g_db->heap.pager().flush_all();

        // Build JSON response
        std::string insert_json = "{"
            + jnum("btree_total_us", btree_insert_us) + ","
            + jnum("heap_total_us", heap_insert_us) + ","
            + jnum("btree_avg_us", btree_insert_us / NUM_OPS) + ","
            + jnum("heap_avg_us", heap_insert_us / NUM_OPS)
            + "}";

        std::string find_json = "{"
            + jnum("btree_total_us", btree_find_us) + ","
            + jnum("heap_total_us", heap_find_us) + ","
            + jnum("btree_avg_us", btree_find_us / NUM_OPS) + ","
            + jnum("heap_avg_us", heap_find_us / NUM_OPS)
            + "}";

        std::string delete_json = "{"
            + jnum("btree_total_us", btree_delete_us) + ","
            + jnum("heap_total_us", heap_delete_us) + ","
            + jnum("btree_avg_us", btree_delete_us / NUM_OPS) + ","
            + jnum("heap_avg_us", heap_delete_us / NUM_OPS)
            + "}";

        std::string j = "{"
            + jstr("status", "success") + ","
            + jint("operations", NUM_OPS) + ","
            + "\"insert\":" + insert_json + ","
            + "\"find\":" + find_json + ","
            + "\"delete\":" + delete_json
            + "}";
        res.json(j);
    });

    // ── Start ──
    const int PORT = 8080;
    std::cout << "[TinySQL] Dashboard: http://localhost:" << PORT << "/" << std::endl;
    std::cout << "[TinySQL] API Base:  http://localhost:" << PORT << "/api/" << std::endl;
    std::cout << "========================================" << std::endl;

    if (!svr.listen("0.0.0.0", PORT)) {
        std::cerr << "[TinySQL] Failed to start server on port " << PORT << std::endl;
        delete g_db;
        return 1;
    }

    delete g_db;
    return 0;
}
