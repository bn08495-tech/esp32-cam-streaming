#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>

#include "mjpeg_client.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace fs = std::filesystem;

class RawTerminal {
public:
    RawTerminal() {
        m_is_tty = isatty(STDIN_FILENO);
        if (m_is_tty) {
            tcgetattr(STDIN_FILENO, &old_tio);
            new_tio = old_tio;
            new_tio.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
        }
        old_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);
    }

    ~RawTerminal() {
        if (m_is_tty) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
        }
        fcntl(STDIN_FILENO, F_SETFL, old_flags);
    }

    int read_key() {
        unsigned char ch = 0;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            return ch;
        }
        return -1;
    }

private:
    bool m_is_tty{false};
    struct termios old_tio{}, new_tio{};
    int old_flags{0};
};

std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S_")
       << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

bool save_snapshot(const std::vector<uint8_t>& jpeg_data, std::string& saved_path) {
    if (jpeg_data.empty()) return false;

    fs::path target_dir = "/home/computer/pictures";
    std::error_code ec;
    fs::create_directories(target_dir, ec);

    std::string filename = "ESP32_CAM_CPP_" + get_current_timestamp() + ".jpg";
    fs::path full_path = target_dir / filename;

    std::ofstream ofs(full_path, std::ios::binary);
    if (!ofs) return false;

    ofs.write(reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size());
    saved_path = full_path.string();
    return true;
}

void toggle_remote_flash(const std::string& host_target) {
    std::string host = host_target;
    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos) {
        host = host.substr(0, colon_pos);
    }
    size_t prefix_pos = host.find("://");
    if (prefix_pos != std::string::npos) {
        host = host.substr(prefix_pos + 3);
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), "80", &hints, &res) == 0 && res) {
        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock >= 0) {
            struct timeval tv{1, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            if (connect(sock, res->ai_addr, res->ai_addrlen) == 0) {
                std::string req = "GET /flash HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
                send(sock, req.c_str(), req.size(), 0);
            }
            close(sock);
        }
        freeaddrinfo(res);
    }
}

int main(int argc, char** argv) {
    std::string target_ip = "127.0.0.1:8080";
    std::string stream_path = "/stream";

    if (argc > 1) {
        target_ip = argv[1];
    }
    if (argc > 2) {
        stream_path = argv[2];
    }

    std::cout << "========================================================\n";
    std::cout << "  ESP32-CAM C++ Streamer (mDNS / OTA / Flash Control)   \n";
    std::cout << "========================================================\n";
    std::cout << "[+] Target IP / Host : " << target_ip << "\n";
    std::cout << "[+] Stream Path     : " << stream_path << "\n";
    std::cout << "[+] mDNS Address    : http://esp32-cam.local:81/stream\n";
    std::cout << "[+] Snapshot Dir    : /home/computer/pictures\n";
    std::cout << "[+] Controls        : Press [Ctrl+X] to Capture Snapshot\n";
    std::cout << "                    : Press 'f' to Toggle Flash LED\n";
    std::cout << "                    : Press 'q' to Quit\n";
    std::cout << "--------------------------------------------------------\n";

    MjpegClient client;
    if (!client.connect_stream(target_ip, stream_path)) {
        std::cerr << "[-] Error initializing stream connection.\n";
        return 1;
    }

    RawTerminal term;
    std::cout << "[+] Connecting to stream...\n";

    int frame_count = 0;
    auto last_fps_time = std::chrono::steady_clock::now();

    while (true) {
        int key = term.read_key();
        if (key != -1) {
            // ASCII 0x18 (24) is Ctrl + X
            if (key == 24 || key == 24 - 64) {
                std::vector<uint8_t> frame;
                if (client.get_latest_jpeg(frame)) {
                    std::string saved_file;
                    if (save_snapshot(frame, saved_file)) {
                        std::cout << "\n[📸 SNAPSHOT CAPTURED] Saved to: " << saved_file << "\n" << std::flush;
                    } else {
                        std::cout << "\n[❌ SNAPSHOT FAILED] Could not write image file.\n" << std::flush;
                    }
                } else {
                    std::cout << "\n[⚠️ WARNING] No frame available to capture yet.\n" << std::flush;
                }
            } else if (key == 'f' || key == 'F') {
                std::cout << "\n[⚡ FLASH LED] Sending toggle command...\n" << std::flush;
                toggle_remote_flash(target_ip);
            } else if (key == 'q' || key == 'Q' || key == 3) { // 'q' or Ctrl+C
                std::cout << "\n[-] Exiting streamer...\n";
                break;
            }
        }

        std::vector<uint8_t> current_jpeg;
        if (client.get_latest_jpeg(current_jpeg)) {
            frame_count++;
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_fps_time).count();
            if (elapsed >= 2.0) {
                double fps = frame_count / elapsed;
                std::cout << "\r[+] Streaming Live | FPS: " << std::fixed << std::setprecision(1) << fps
                          << " | Frames Received: " << frame_count << " | [Ctrl+X]: Snapshot | 'f': Flash" << std::flush;
                frame_count = 0;
                last_fps_time = now;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    client.disconnect();
    std::cout << "\n[+] Goodbye!\n";
    return 0;
}
