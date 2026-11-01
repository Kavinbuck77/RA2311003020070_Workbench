#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iostream>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
  #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
  #define CLOSESOCKET(s) closesocket(s)
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <netdb.h>
  typedef int socket_t;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define ISVALIDSOCKET(s) ((s) >= 0)
  #define CLOSESOCKET(s) close(s)
#endif

namespace Workbench {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string get_param(const std::string& key, const std::string& default_val = "") const {
        auto it = query_params.find(key);
        return (it != query_params.end()) ? it->second : default_val;
    }
};

struct HttpResponse {
    int status_code = 200;
    std::string content_type = "text/html; charset=utf-8";
    std::map<std::string, std::string> headers;
    std::string body;

    static HttpResponse Json(const std::string& json_str, int code = 200) {
        HttpResponse res;
        res.status_code = code;
        res.content_type = "application/json; charset=utf-8";
        res.body = json_str;
        return res;
    }

    static HttpResponse Html(const std::string& html_str, int code = 200) {
        HttpResponse res;
        res.status_code = code;
        res.content_type = "text/html; charset=utf-8";
        res.body = html_str;
        return res;
    }

    static HttpResponse Csv(const std::string& csv_str, const std::string& filename = "export.csv") {
        HttpResponse res;
        res.status_code = 200;
        res.content_type = "text/csv; charset=utf-8";
        res.headers["Content-Disposition"] = "attachment; filename=\"" + filename + "\"";
        res.body = csv_str;
        return res;
    }

    static HttpResponse NotFound(const std::string& msg = "Not Found") {
        HttpResponse res;
        res.status_code = 404;
        res.content_type = "text/plain";
        res.body = msg;
        return res;
    }
};

typedef std::function<HttpResponse(const HttpRequest&)> HttpHandler;

class SimpleHttpServer {
public:
    SimpleHttpServer(int port = 8080)
        : port_(port), running_(false), server_fd_(INVALID_SOCKET) {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    ~SimpleHttpServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void Get(const std::string& path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(routes_mutex_);
        get_routes_[path] = handler;
    }

    void Post(const std::string& path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(routes_mutex_);
        post_routes_[path] = handler;
    }

    bool start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!ISVALIDSOCKET(server_fd_)) {
            std::cerr << "[HTTP Server] Error creating socket." << std::endl;
            return false;
        }

        // Enable reuse address
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(static_cast<u_short>(port_));

        if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[HTTP Server] Bind failed on port " << port_ << std::endl;
            CLOSESOCKET(server_fd_);
            return false;
        }

        if (listen(server_fd_, 64) == SOCKET_ERROR) {
            std::cerr << "[HTTP Server] Listen failed." << std::endl;
            CLOSESOCKET(server_fd_);
            return false;
        }

        running_ = true;
        std::cout << "=================================================================\n";
        std::cout << "  UNIFIED ENGINEERING WORKBENCH HTTP SERVER ACTIVE\n";
        std::cout << "  Candidate: Kavin (Principal Systems & Scientific Software Engineer)\n";
        std::cout << "  Listening on: http://localhost:" << port_ << " (or http://127.0.0.1:" << port_ << ")\n";
        std::cout << "=================================================================\n" << std::endl;

        accept_thread_ = std::thread(&SimpleHttpServer::accept_loop, this);
        return true;
    }

    void wait() {
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
    }

    void stop() {
        if (running_) {
            running_ = false;
            if (ISVALIDSOCKET(server_fd_)) {
                CLOSESOCKET(server_fd_);
                server_fd_ = INVALID_SOCKET;
            }
        }
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
    }

private:
    int port_;
    std::atomic<bool> running_;
    socket_t server_fd_;
    std::thread accept_thread_;
    std::mutex routes_mutex_;
    std::map<std::string, HttpHandler> get_routes_;
    std::map<std::string, HttpHandler> post_routes_;

    void accept_loop() {
        while (running_) {
            sockaddr_in client_addr{};
            int client_len = sizeof(client_addr);
#ifdef _WIN32
            socket_t client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
#else
            socket_t client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
#endif
            if (!ISVALIDSOCKET(client_fd)) {
                if (!running_) break;
                continue;
            }

            // Dispatch to detached worker thread
            std::thread([this, client_fd]() {
                handle_client(client_fd);
            }).detach();
        }
    }

    static std::string url_decode(const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (in[i] == '%') {
                if (i + 2 < in.size()) {
                    int hex_val = 0;
                    std::istringstream hex_stream(in.substr(i + 1, 2));
                    if (hex_stream >> std::hex >> hex_val) {
                        out += static_cast<char>(hex_val);
                        i += 2;
                    } else {
                        out += '%';
                    }
                } else {
                    out += '%';
                }
            } else if (in[i] == '+') {
                out += ' ';
            } else {
                out += in[i];
            }
        }
        return out;
    }

    void handle_client(socket_t client_fd) {
        char buffer[8192];
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            CLOSESOCKET(client_fd);
            return;
        }
        buffer[bytes_received] = '\0';

        std::string raw_request(buffer, bytes_received);
        std::istringstream req_stream(raw_request);
        std::string req_line;
        if (!std::getline(req_stream, req_line)) {
            CLOSESOCKET(client_fd);
            return;
        }

        // Clean trailing carriage returns
        if (!req_line.empty() && req_line.back() == '\r') req_line.pop_back();

        std::istringstream line_parts(req_line);
        std::string method, full_path, protocol;
        line_parts >> method >> full_path >> protocol;

        HttpRequest req;
        req.method = method;

        // Split path and query parameters
        size_t query_pos = full_path.find('?');
        if (query_pos != std::string::npos) {
            req.path = full_path.substr(0, query_pos);
            req.query_string = full_path.substr(query_pos + 1);

            // Parse key=value query string
            std::istringstream q_stream(req.query_string);
            std::string pair;
            while (std::getline(q_stream, pair, '&')) {
                size_t eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string k = url_decode(pair.substr(0, eq_pos));
                    std::string v = url_decode(pair.substr(eq_pos + 1));
                    req.query_params[k] = v;
                } else if (!pair.empty()) {
                    req.query_params[url_decode(pair)] = "";
                }
            }
        } else {
            req.path = full_path;
        }

        // Parse headers
        std::string header_line;
        while (std::getline(req_stream, header_line) && header_line != "\r" && !header_line.empty()) {
            if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
            size_t colon = header_line.find(':');
            if (colon != std::string::npos) {
                std::string hk = header_line.substr(0, colon);
                std::string hv = header_line.substr(colon + 1);
                // trim
                while (!hv.empty() && (hv.front() == ' ' || hv.front() == '\t')) hv.erase(0, 1);
                req.headers[hk] = hv;
            }
        }

        // Read remaining body if any
        std::stringstream body_ss;
        body_ss << req_stream.rdbuf();
        req.body = body_ss.str();

        HttpResponse res;

        // Handle CORS Pre-flight OPTIONS
        if (method == "OPTIONS") {
            res.status_code = 204;
            res.headers["Access-Control-Allow-Origin"] = "*";
            res.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
            res.headers["Access-Control-Allow-Headers"] = "Content-Type";
            send_response(client_fd, res);
            CLOSESOCKET(client_fd);
            return;
        }

        // Route lookup
        HttpHandler handler = nullptr;
        {
            std::lock_guard<std::mutex> lock(routes_mutex_);
            if (method == "GET") {
                auto it = get_routes_.find(req.path);
                if (it != get_routes_.end()) handler = it->second;
            } else if (method == "POST") {
                auto it = post_routes_.find(req.path);
                if (it != post_routes_.end()) handler = it->second;
            }
        }

        if (handler) {
            try {
                res = handler(req);
            } catch (const std::exception& e) {
                res.status_code = 500;
                res.content_type = "application/json";
                res.body = "{\"error\":\"Internal Server Error: " + std::string(e.what()) + "\"}";
            }
        } else {
            res = HttpResponse::NotFound("Endpoint not found: " + req.path);
        }

        // Always inject CORS
        res.headers["Access-Control-Allow-Origin"] = "*";
        res.headers["Connection"] = "close";

        send_response(client_fd, res);
        CLOSESOCKET(client_fd);
    }

    void send_response(socket_t client_fd, const HttpResponse& res) {
        std::ostringstream ss;
        std::string status_msg = "OK";
        if (res.status_code == 404) status_msg = "Not Found";
        else if (res.status_code == 500) status_msg = "Internal Server Error";
        else if (res.status_code == 204) status_msg = "No Content";

        ss << "HTTP/1.1 " << res.status_code << " " << status_msg << "\r\n";
        ss << "Content-Type: " << res.content_type << "\r\n";
        ss << "Content-Length: " << res.body.size() << "\r\n";
        for (const auto& h : res.headers) {
            ss << h.first << ": " << h.second << "\r\n";
        }
        ss << "\r\n";
        ss << res.body;

        std::string full_response = ss.str();
        send(client_fd, full_response.c_str(), static_cast<int>(full_response.size()), 0);
    }
};

} // namespace Workbench

#endif // HTTP_SERVER_HPP
