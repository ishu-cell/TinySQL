/*
 * main.cpp — TinySQL REPL
 *
 * Usage:
 *   tinysql <database_file>            (interactive REPL)
 *   tinysql <database_file> --json     (machine-readable JSON output)
 *
 * Commands:
 *   .exit           — flush all pages and quit
 *   .btree          — print the B+ Tree structure (debug)
 *   insert <id> <username> <email>
 *   select          — full table scan (leaf traversal)
 *   find <id>       — O(log n) lookup via B+ Tree
 *   count           — total number of rows
 */

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

#include "constants.hpp"
#include "row.hpp"
#include "pager.hpp"
#include "btree.hpp"

// ============================================================
//  Table — ties together the Pager and the root page
// ============================================================
struct Table {
    Pager    pager;
    uint32_t root_page_num;

    Table(const char* filename)
        : pager(filename), root_page_num(0)
    {
        if (pager.num_pages() == 0) {
            // Brand-new database — initialise page 0 as an empty leaf root
            void* root = pager.get_page(0);
            initialize_leaf_node(root);
            set_node_root(root, true);
            pager.mark_dirty(0);
        }
    }
};

// Global flag for JSON output mode
static bool json_mode = false;

// ============================================================
//  Meta-commands (dot-commands)
// ============================================================
enum class MetaCommandResult { SUCCESS, UNRECOGNIZED };

static MetaCommandResult do_meta_command(const std::string& input, Table& table) {
    if (input == ".exit") {
        table.pager.flush_all();
        if (json_mode) {
            std::cout << "{\"status\":\"success\",\"message\":\"Goodbye.\"}" << std::endl;
        }
        std::exit(EXIT_SUCCESS);
    }
    if (input == ".btree") {
        if (!json_mode) {
            std::cout << "Tree:\n";
            print_tree(table.pager, table.root_page_num, 0);
        }
        return MetaCommandResult::SUCCESS;
    }
    return MetaCommandResult::UNRECOGNIZED;
}

// ============================================================
//  Statement parsing
// ============================================================
enum class StatementType { INSERT, SELECT, FIND, COUNT };

struct Statement {
    StatementType type;
    Row           row;       // for INSERT
    uint32_t      find_id;   // for FIND
};

enum class PrepareResult { SUCCESS, SYNTAX_ERROR, UNRECOGNIZED, STRING_TOO_LONG, NEGATIVE_ID };

static PrepareResult prepare_statement(const std::string& input, Statement& stmt) {
    if (input.rfind("insert", 0) == 0) {
        stmt.type = StatementType::INSERT;
        std::istringstream iss(input);
        std::string cmd, username, email;
        int id_signed = 0;

        iss >> cmd >> id_signed >> username >> email;
        if (iss.fail() || username.empty() || email.empty())
            return PrepareResult::SYNTAX_ERROR;
        if (id_signed < 0)
            return PrepareResult::NEGATIVE_ID;
        if (username.size() > COLUMN_USERNAME_SIZE)
            return PrepareResult::STRING_TOO_LONG;
        if (email.size() > COLUMN_EMAIL_SIZE)
            return PrepareResult::STRING_TOO_LONG;

        stmt.row.id = static_cast<uint32_t>(id_signed);
        std::strncpy(stmt.row.username, username.c_str(), COLUMN_USERNAME_SIZE);
        stmt.row.username[COLUMN_USERNAME_SIZE] = '\0';
        std::strncpy(stmt.row.email, email.c_str(), COLUMN_EMAIL_SIZE);
        stmt.row.email[COLUMN_EMAIL_SIZE] = '\0';

        return PrepareResult::SUCCESS;
    }

    if (input == "select") {
        stmt.type = StatementType::SELECT;
        return PrepareResult::SUCCESS;
    }

    if (input.rfind("find", 0) == 0) {
        stmt.type = StatementType::FIND;
        std::istringstream iss(input);
        std::string cmd;
        int id_signed = 0;
        iss >> cmd >> id_signed;
        if (iss.fail())
            return PrepareResult::SYNTAX_ERROR;
        if (id_signed < 0)
            return PrepareResult::NEGATIVE_ID;
        stmt.find_id = static_cast<uint32_t>(id_signed);
        return PrepareResult::SUCCESS;
    }

    if (input == "count") {
        stmt.type = StatementType::COUNT;
        return PrepareResult::SUCCESS;
    }

    return PrepareResult::UNRECOGNIZED;
}

// ============================================================
//  Execution
// ============================================================

static void execute_insert(Statement& stmt, Table& table) {
    uint32_t key = stmt.row.id;
    Cursor cursor = btree_find(table.pager, table.root_page_num, key);

    void* node = table.pager.get_page(cursor.page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    // Check for duplicate key
    if (cursor.cell_num < num_cells &&
        *leaf_node_key(node, cursor.cell_num) == key) {
        if (json_mode) {
            std::cout << "{\"status\":\"error\",\"message\":\"Duplicate key: "
                      << key << "\"}" << std::endl;
        } else {
            std::cout << "Error: Duplicate key " << key << "." << std::endl;
        }
        return;
    }

    leaf_node_insert(cursor, key, stmt.row);

    if (json_mode) {
        std::cout << "{\"status\":\"success\",\"message\":\"Row inserted.\"}" << std::endl;
    } else {
        std::cout << "Executed." << std::endl;
    }
}

static void execute_select(Table& table) {
    Cursor cursor = table_start(table.pager, table.root_page_num);

    if (json_mode) {
        std::cout << "{\"status\":\"success\",\"rows\":[";
        bool first = true;
        while (!cursor.end_of_table) {
            Row row;
            void* node = table.pager.get_page(cursor.page_num);
            deserialize_row(leaf_node_value(node, cursor.cell_num), row);
            if (!first) std::cout << ",";
            std::cout << row_to_json(row);
            first = false;
            cursor_advance(cursor);
        }
        std::cout << "]}" << std::endl;
    } else {
        while (!cursor.end_of_table) {
            Row row;
            void* node = table.pager.get_page(cursor.page_num);
            deserialize_row(leaf_node_value(node, cursor.cell_num), row);
            std::cout << "(" << row.id << ", "
                      << row.username << ", "
                      << row.email << ")" << std::endl;
            cursor_advance(cursor);
        }
    }
}

static void execute_find(Statement& stmt, Table& table) {
    uint32_t key = stmt.find_id;
    Cursor cursor = btree_find(table.pager, table.root_page_num, key);

    void* node = table.pager.get_page(cursor.page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (cursor.cell_num < num_cells &&
        *leaf_node_key(node, cursor.cell_num) == key) {
        Row row;
        deserialize_row(leaf_node_value(node, cursor.cell_num), row);
        if (json_mode) {
            std::cout << "{\"status\":\"success\",\"row\":"
                      << row_to_json(row) << "}" << std::endl;
        } else {
            std::cout << "(" << row.id << ", "
                      << row.username << ", "
                      << row.email << ")" << std::endl;
        }
    } else {
        if (json_mode) {
            std::cout << "{\"status\":\"error\",\"message\":\"Key not found: "
                      << key << "\"}" << std::endl;
        } else {
            std::cout << "Error: Key " << key << " not found." << std::endl;
        }
    }
}

static void execute_count(Table& table) {
    uint32_t count = 0;
    Cursor cursor = table_start(table.pager, table.root_page_num);
    while (!cursor.end_of_table) {
        count++;
        cursor_advance(cursor);
    }
    if (json_mode) {
        std::cout << "{\"status\":\"success\",\"count\":" << count << "}" << std::endl;
    } else {
        std::cout << "Count: " << count << std::endl;
    }
}

static void execute_statement(Statement& stmt, Table& table) {
    switch (stmt.type) {
    case StatementType::INSERT: execute_insert(stmt, table); break;
    case StatementType::SELECT: execute_select(table);       break;
    case StatementType::FIND:   execute_find(stmt, table);   break;
    case StatementType::COUNT:  execute_count(table);        break;
    }
}

// ============================================================
//  Main REPL
// ============================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: tinysql <database_file> [--json]" << std::endl;
        return EXIT_FAILURE;
    }

    const char* filename = argv[1];

    // Check for --json flag
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--json") {
            json_mode = true;
        }
    }

    Table table(filename);

    // Signal readiness
    if (json_mode) {
        std::cout << "{\"status\":\"ready\",\"message\":\"TinySQL engine started.\"}" << std::endl;
    }

    std::string input;
    while (true) {
        if (!json_mode) {
            std::cout << "tinysql> ";
            std::cout.flush();
        }

        if (!std::getline(std::cin, input)) {
            // EOF — exit gracefully
            table.pager.flush_all();
            break;
        }

        // Trim trailing \r (Windows line endings)
        if (!input.empty() && input.back() == '\r') {
            input.pop_back();
        }

        // Skip empty lines
        if (input.empty()) {
            if (json_mode) {
                std::cout << "{\"status\":\"error\",\"message\":\"Empty command.\"}" << std::endl;
            }
            continue;
        }

        // Meta-commands start with '.'
        if (input[0] == '.') {
            if (do_meta_command(input, table) == MetaCommandResult::UNRECOGNIZED) {
                if (json_mode) {
                    std::cout << "{\"status\":\"error\",\"message\":\"Unrecognized command: "
                              << json_escape(input) << "\"}" << std::endl;
                } else {
                    std::cout << "Unrecognized command: " << input << std::endl;
                }
            }
            continue;
        }

        // Parse and execute SQL-like statements
        Statement stmt{};
        switch (prepare_statement(input, stmt)) {
        case PrepareResult::SUCCESS:
            execute_statement(stmt, table);
            break;
        case PrepareResult::SYNTAX_ERROR:
            if (json_mode) {
                std::cout << "{\"status\":\"error\",\"message\":\"Syntax error in: "
                          << json_escape(input) << "\"}" << std::endl;
            } else {
                std::cout << "Syntax error: could not parse '" << input << "'." << std::endl;
            }
            break;
        case PrepareResult::STRING_TOO_LONG:
            if (json_mode) {
                std::cout << "{\"status\":\"error\",\"message\":\"String is too long.\"}" << std::endl;
            } else {
                std::cout << "Error: String is too long." << std::endl;
            }
            break;
        case PrepareResult::NEGATIVE_ID:
            if (json_mode) {
                std::cout << "{\"status\":\"error\",\"message\":\"ID must be non-negative.\"}" << std::endl;
            } else {
                std::cout << "Error: ID must be non-negative." << std::endl;
            }
            break;
        case PrepareResult::UNRECOGNIZED:
            if (json_mode) {
                std::cout << "{\"status\":\"error\",\"message\":\"Unrecognized keyword: "
                          << json_escape(input) << "\"}" << std::endl;
            } else {
                std::cout << "Unrecognized keyword at start of '" << input << "'." << std::endl;
            }
            break;
        }
    }

    return EXIT_SUCCESS;
}
