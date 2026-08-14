#include "mjpeg_client.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
    #define INVALID_SOCKET -1
#endif

MjpegClient::MjpegClient() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

MjpegClient::~MjpegClient() {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

void MjpegClient::parse_host_port(const std::string& input, std::string& host_out, int& port_out) {
    std::string clean = input;
    size_t prefix_pos = clean.find("://");
    if (prefix_pos != std::string::npos) {
        clean = clean.substr(prefix_pos + 3);
    }
    size_t slash_pos = clean.find('/');
    if (slash_pos != std::string::npos) {
        clean = clean.substr(0, slash_pos);
    }
    size_t colon_pos = clean.find(':');
    if (colon_pos != std::string::npos) {
        host_out = clean.substr(0, colon_pos);
        try {
            port_out = std::stoi(clean.substr(colon_pos + 1));
        } catch (...) {
            port_out = 80;
        }
    } else {
        host_out = clean;
        port_out = 80;
    }
}

bool MjpegClient::connect_stream(const std::string& host_port, const std::string& path) {
    disconnect();

    std::string host;
    int port;
    parse_host_port(host_port, host, port);

    m_running.store(true);
    m_thread = std::thread(&MjpegClient::worker_thread, this, host, port, path);
    return true;
}

void MjpegClient::disconnect() {
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_connected.store(false);
}

bool MjpegClient::get_latest_jpeg(std::vector<uint8_t>& jpeg_out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_latest_jpeg.empty()) return false;
    jpeg_out = m_latest_jpeg;
    return true;
}

std::string MjpegClient::get_status() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status_msg;
}

void MjpegClient::worker_thread(std::string host, int port, std::string path) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status_msg = "Connecting to " + host + ":" + std::to_string(port) + "...";
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status_msg = "DNS resolution failed for " + host;
        return;
    }

    socket_t sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status_msg = "Failed to create socket.";
        return;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) < 0) {
        CLOSE_SOCKET(sock);
        freeaddrinfo(res);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status_msg = "Failed to connect to " + host + ":" + std::to_string(port);
        return;
    }
    freeaddrinfo(res);

#ifdef _WIN32
    DWORD timeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    std::ostringstream req;
    req << "GET " << path << " HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "User-Agent: ESP32CamCppStreamer/1.0\r\n"
        << "Connection: keep-alive\r\n\r\n";

    std::string req_str = req.str();
    if (send(sock, req_str.c_str(), (int)req_str.size(), 0) < 0) {
        CLOSE_SOCKET(sock);
        return;
    }

    m_connected.store(true);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status_msg = "Connected and streaming.";
    }

    std::vector<uint8_t> buffer;
    buffer.reserve(65536);
    char chunk[8192];

    while (m_running.load()) {
        int bytes_read = recv(sock, chunk, sizeof(chunk), 0);
        if (bytes_read <= 0) break;

        buffer.insert(buffer.end(), chunk, chunk + bytes_read);

        static const uint8_t SOI[2] = {0xFF, 0xD8};
        static const uint8_t EOI[2] = {0xFF, 0xD9};

        // Scan for JPEG SOI (0xFF, 0xD8) and EOI (0xFF, 0xD9)
        while (true) {
            auto soi = std::search(buffer.begin(), buffer.end(), SOI, SOI + 2);
            if (soi == buffer.end()) break;

            auto eoi = std::search(soi + 2, buffer.end(), EOI, EOI + 2);
            if (eoi == buffer.end()) break;

            eoi += 2; // Include 0xFF 0xD9

            std::vector<uint8_t> jpeg_bytes(soi, eoi);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_latest_jpeg = std::move(jpeg_bytes);
            }

            buffer.erase(buffer.begin(), eoi);
        }

        if (buffer.size() > 1000000) {
            buffer.clear();
        }
    }

    CLOSE_SOCKET(sock);
    m_connected.store(false);
}
