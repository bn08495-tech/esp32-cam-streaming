#ifndef MJPEG_CLIENT_HPP
#define MJPEG_CLIENT_HPP

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

class MjpegClient {
public:
    MjpegClient();
    ~MjpegClient();

    bool connect_stream(const std::string& host_port, const std::string& path = "/stream");
    void disconnect();
    
    bool is_connected() const { return m_connected.load(); }
    bool get_latest_jpeg(std::vector<uint8_t>& jpeg_out);
    
    std::string get_status() const;

private:
    void worker_thread(std::string host, int port, std::string path);
    static void parse_host_port(const std::string& input, std::string& host_out, int& port_out);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::thread m_thread;
    
    mutable std::mutex m_mutex;
    std::vector<uint8_t> m_latest_jpeg;
    std::string m_status_msg;
};

#endif // MJPEG_CLIENT_HPP
