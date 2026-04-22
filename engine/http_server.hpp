#pragma once
/*
 * http_server.hpp — Minimal single-threaded HTTP server using Winsock2
 *
 * Zero external dependencies. Provides simple routing for GET/POST
 * with JSON request/response support. Designed for TinySQL's REST API.
 *
 * Usage:
 *   HttpServer svr;
 *   svr.Get("/api/health", [](const HttpRequest& req, HttpResponse& res) {
 *       res.json("{\"status\":\"ok\"}");
 *   });
 *   svr.listen("0.0.0.0", 8080);
 */

#ifdef _WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0601
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int SOCKET;
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #define closesocket close
#endif

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>

// ============================================================
//  HTTP Request
// ============================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::string body;
    std::unordered_map<std::string, std::string> params; // query params
    std::unordered_map<std::string, std::string> headers;

    std::string get_param(const std::string& key, const std::string& def = "") const {
        auto it = params.find(key);
        return it != params.end() ? it->second : def;
    }

    bool has_param(const std::string& key) const {
        return params.find(key) != params.end();
    }
};

// ============================================================
//  HTTP Response
// ============================================================
struct HttpResponse {
    int status_code = 200;
    std::string content_type = "text/plain";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    void json(const std::string& j) {
        content_type = "application/json";
        body = j;
    }

    void html(const std::string& h) {
        content_type = "text/html; charset=utf-8";
        body = h;
    }

    void set_header(const std::string& key, const std::string& val) {
        headers.push_back({key, val});
    }

    std::string build() const {
        std::string status_text = "OK";
        if (status_code == 400) status_text = "Bad Request";
        else if (status_code == 404) status_text = "Not Found";
        else if (status_code == 204) status_text = "No Content";
        else if (status_code == 500) status_text = "Internal Server Error";

        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        oss << "Content-Type: " << content_type << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        oss << "Access-Control-Allow-Headers: Content-Type\r\n";
        oss << "Connection: close\r\n";
        for (auto& h : headers) {
            oss << h.first << ": " << h.second << "\r\n";
        }
        oss << "\r\n";
        oss << body;
        return oss.str();
    }
};

// ============================================================
//  Route handler type
// ============================================================
using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// ============================================================
//  HTTP Server
// ============================================================
class HttpServer {
public:
    HttpServer() = default;

    void Get(const std::string& path, RouteHandler handler) {
        routes_["GET:" + path] = handler;
    }

    void Post(const std::string& path, RouteHandler handler) {
        routes_["POST:" + path] = handler;
    }

    bool listen(const std::string& host, int port) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::cerr << "[HTTP] WSAStartup failed" << std::endl;
            return false;
        }
#endif
        SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == INVALID_SOCKET) {
            std::cerr << "[HTTP] Failed to create socket" << std::endl;
            return false;
        }

        // Allow port reuse
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(port));

        if (host == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            addr.sin_addr.s_addr = inet_addr(host.c_str());
        }

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "[HTTP] Bind failed on port " << port << std::endl;
            closesocket(server_fd);
            return false;
        }

        if (::listen(server_fd, 128) == SOCKET_ERROR) {
            std::cerr << "[HTTP] Listen failed" << std::endl;
            closesocket(server_fd);
            return false;
        }

        std::cout << "[HTTP] Listening on " << host << ":" << port << std::endl;

        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            SOCKET client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd == INVALID_SOCKET) continue;

            handle_connection(client_fd);
            closesocket(client_fd);
        }

        closesocket(server_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return true;
    }

private:
    std::unordered_map<std::string, RouteHandler> routes_;

    void handle_connection(SOCKET fd) {
        // Read the full request (up to 64KB)
        std::string raw;
        raw.resize(65536);
        int total = 0;
        int bytes;

        // Read headers first
        while (total < (int)raw.size() - 1) {
            bytes = recv(fd, &raw[total], (int)raw.size() - total - 1, 0);
            if (bytes <= 0) break;
            total += bytes;
            raw[total] = '\0';

            // Check if we have the full headers
            if (raw.find("\r\n\r\n") != std::string::npos) {
                // Check Content-Length for body
                size_t header_end = raw.find("\r\n\r\n") + 4;
                int content_length = 0;

                std::string lower = raw.substr(0, header_end);
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                size_t cl_pos = lower.find("content-length:");
                if (cl_pos != std::string::npos) {
                    content_length = std::atoi(raw.c_str() + cl_pos + 15);
                }

                int body_received = total - (int)header_end;
                // Read remaining body if needed
                while (body_received < content_length) {
                    bytes = recv(fd, &raw[total], std::min((int)raw.size() - total - 1, content_length - body_received), 0);
                    if (bytes <= 0) break;
                    total += bytes;
                    body_received += bytes;
                }
                break;
            }
        }
        raw.resize(total);

        if (raw.empty()) return;

        // Parse request
        HttpRequest req = parse_request(raw);

        // Handle OPTIONS (CORS preflight)
        if (req.method == "OPTIONS") {
            HttpResponse res;
            res.status_code = 204;
            std::string resp = res.build();
            send(fd, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        // Find route handler
        std::string route_key = req.method + ":" + req.path;
        HttpResponse res;

        auto it = routes_.find(route_key);
        if (it != routes_.end()) {
            try {
                it->second(req, res);
            } catch (const std::exception& e) {
                res.status_code = 500;
                res.json("{\"status\":\"error\",\"message\":\"Internal error: " + std::string(e.what()) + "\"}");
            }
        } else {
            res.status_code = 404;
            res.json("{\"status\":\"error\",\"message\":\"Not found: " + req.path + "\"}");
        }

        std::string resp = res.build();
        // Send in chunks (large responses)
        int sent = 0;
        while (sent < (int)resp.size()) {
            int chunk = send(fd, resp.c_str() + sent, (int)resp.size() - sent, 0);
            if (chunk <= 0) break;
            sent += chunk;
        }
    }

    HttpRequest parse_request(const std::string& raw) {
        HttpRequest req;

        // First line: METHOD /path?query HTTP/1.1
        size_t line_end = raw.find("\r\n");
        if (line_end == std::string::npos) return req;

        std::string first_line = raw.substr(0, line_end);
        std::istringstream iss(first_line);
        std::string version;
        std::string full_path;
        iss >> req.method >> full_path >> version;

        // Split path and query string
        size_t qpos = full_path.find('?');
        if (qpos != std::string::npos) {
            req.path = full_path.substr(0, qpos);
            req.query_string = full_path.substr(qpos + 1);
            parse_query_params(req.query_string, req.params);
        } else {
            req.path = full_path;
        }

        // Parse headers
        size_t header_start = line_end + 2;
        size_t header_end = raw.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string header_section = raw.substr(header_start, header_end - header_start);
            std::istringstream hss(header_section);
            std::string line;
            while (std::getline(hss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string val = line.substr(colon + 1);
                    // trim leading space
                    if (!val.empty() && val[0] == ' ') val = val.substr(1);
                    req.headers[key] = val;
                }
            }

            // Body
            req.body = raw.substr(header_end + 4);
        }

        return req;
    }

    void parse_query_params(const std::string& qs, std::unordered_map<std::string, std::string>& params) {
        std::istringstream iss(qs);
        std::string pair;
        while (std::getline(iss, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != std::string::npos) {
                params[pair.substr(0, eq)] = pair.substr(eq + 1);
            } else {
                params[pair] = "";
            }
        }
    }
};
