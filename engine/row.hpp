#pragma once
/*
 * row.hpp — Row data structure, serialization, and synthetic generation
 *
 * A Row is the fundamental record type: (id, username, email).
 * serialize_row / deserialize_row pack/unpack rows into fixed-size
 * byte buffers within B+ Tree leaf cells and heap slots.
 */


#include "constants.hpp"
#include <cstring>
#include <cstdint>
#include <string>

struct Row {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1]; // +1 for null terminator
    char email[COLUMN_EMAIL_SIZE + 1];

    Row() : id(0) {
        std::memset(username, 0, sizeof(username));
        std::memset(email, 0, sizeof(email));
    }
};

// Pack a Row into a contiguous byte buffer (inside a leaf node cell)
inline void serialize_row(const Row& source, void* destination) {
    char* dest = static_cast<char*>(destination);
    std::memcpy(dest + ID_OFFSET,       &source.id,       COLUMN_ID_SIZE);
    std::memcpy(dest + USERNAME_OFFSET,  source.username,  COLUMN_USERNAME_SIZE);
    std::memcpy(dest + EMAIL_OFFSET,     source.email,     COLUMN_EMAIL_SIZE);
}

// Unpack a Row from a contiguous byte buffer
inline void deserialize_row(const void* source, Row& destination) {
    const char* src = static_cast<const char*>(source);
    std::memcpy(&destination.id,       src + ID_OFFSET,       COLUMN_ID_SIZE);
    std::memcpy(destination.username,  src + USERNAME_OFFSET,  COLUMN_USERNAME_SIZE);
    destination.username[COLUMN_USERNAME_SIZE] = '\0';
    std::memcpy(destination.email,     src + EMAIL_OFFSET,     COLUMN_EMAIL_SIZE);
    destination.email[COLUMN_EMAIL_SIZE] = '\0';
}

// Escape a string for safe JSON inclusion
inline std::string json_escape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
        }
    }
    return result;
}

// Render a Row as a JSON object string
inline std::string row_to_json(const Row& row) {
    return "{\"id\":" + std::to_string(row.id)
         + ",\"username\":\"" + json_escape(row.username) + "\""
         + ",\"email\":\"" + json_escape(row.email) + "\"}";
}

// ============================================================
//  Synthetic Row Generation (for bulk seeding)
// ============================================================

// Simple deterministic name generation from an ID
inline Row generate_synthetic_row(uint32_t id) {
    Row row;
    row.id = id;

    // Generate a plausible username: user_<id>
    std::string uname = "user_" + std::to_string(id);
    std::strncpy(row.username, uname.c_str(), COLUMN_USERNAME_SIZE);
    row.username[COLUMN_USERNAME_SIZE] = '\0';

    // Generate a plausible email: user_<id>@tinysql.io
    std::string em = "user_" + std::to_string(id) + "@tinysql.io";
    std::strncpy(row.email, em.c_str(), COLUMN_EMAIL_SIZE);
    row.email[COLUMN_EMAIL_SIZE] = '\0';

    return row;
}
