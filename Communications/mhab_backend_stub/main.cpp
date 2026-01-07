
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <deque>
#include <iostream>
#include <string>
#include <atomic>
#include "httplib.h"

static std::atomic<int> g_cmd_seq{0};
struct PendingCommand {
    int cmd_seq;
    std::string cmd;
    int retries_left;
    std::chrono::steady_clock::time_point last_tx;
    int timeout_s;
    std::string state; // WAITING_ACK / RETRYING / TIMEOUT
};

static std::mutex g_mtx;
static std::vector<PendingCommand> g_pending;

static bool g_radio_connected = false;
static std::string g_last_tx_time = "";
static std::string g_last_rx_time = "";



static std::string now_hhmmss() {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

static std::string detect_cmd(const std::string& body) {
    // Minimal “parsing” (good enough for stub testing)
    if (body.find("\"cmd\"") != std::string::npos && body.find("shutdown") != std::string::npos) return "shutdown";
    if (body.find("\"cmd\"") != std::string::npos && body.find("recalibrate") != std::string::npos) return "recalibrate";
    return "unknown";
}







struct RadioMetrics {
    bool connected = false;

    // "last known" link metrics (updated on each RX in real system)
    int rssi_dbm = -112;
    double snr_db = 7.5;

    // last RX time (for UI display)
    std::chrono::steady_clock::time_point last_rx_tp{};
    std::string last_rx_hhmmss = "";

    // packet timestamps for "packets per minute"
    std::deque<std::chrono::steady_clock::time_point> rx_times;
};

static RadioMetrics g_radio;

static void radio_simulate_rx(int rssi_dbm, double snr_db) {
    using namespace std::chrono;
    auto now = steady_clock::now();

    g_radio.rssi_dbm = rssi_dbm;
    g_radio.snr_db = snr_db;

    g_radio.last_rx_tp = now;
    g_radio.last_rx_hhmmss = now_hhmmss();  // reuse your helper

    g_radio.rx_times.push_back(now);

    // keep only last 60s
    while (!g_radio.rx_times.empty()) {
        auto age = duration_cast<seconds>(now - g_radio.rx_times.front()).count();
        if (age > 60) g_radio.rx_times.pop_front();
        else break;
    }
}

static int radio_pkts_last_min() {
    using namespace std::chrono;
    auto now = steady_clock::now();

    // purge old
    while (!g_radio.rx_times.empty()) {
        auto age = duration_cast<seconds>(now - g_radio.rx_times.front()).count();
        if (age > 60) g_radio.rx_times.pop_front();
        else break;
    }
    return (int)g_radio.rx_times.size();
}




















int main() {
    httplib::Server svr;

    svr.Get("/api/radio/metrics", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);

        int pkts = radio_pkts_last_min();

        // If never received anything yet, last_rx_time can be empty string
        std::ostringstream oss;
        oss << "{"
            << "\"ok\":true,"
            << "\"connected\":" << (g_radio.connected ? "true" : "false") << ","
            << "\"rssi_dbm\":" << g_radio.rssi_dbm << ","
            << "\"snr_db\":" << g_radio.snr_db << ","
            << "\"last_rx_time\":\"" << g_radio.last_rx_hhmmss << "\","
            << "\"pkts_last_min\":" << pkts
            << "}";

        res.set_content(oss.str(), "application/json");
        res.status = 200;
        });

    
  
    svr.Post("/api/radio/sim_rx", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);

        // very lightweight parse (since this is stub)
        int rssi = g_radio.rssi_dbm;
        double snr = g_radio.snr_db;

        // naive parse
        auto pos_rssi = req.body.find("rssi_dbm");
        if (pos_rssi != std::string::npos) {
            auto colon = req.body.find(":", pos_rssi);
            if (colon != std::string::npos) rssi = std::stoi(req.body.substr(colon + 1));
        }
        auto pos_snr = req.body.find("snr_db");
        if (pos_snr != std::string::npos) {
            auto colon = req.body.find(":", pos_snr);
            if (colon != std::string::npos) snr = std::stod(req.body.substr(colon + 1));
        }

        
        if (!g_radio.connected) {
            res.set_content("{\"ok\":false,\"error\":\"radio not connected\"}", "application/json");
            res.status = 400;
            return;
        }

        radio_simulate_rx(rssi, snr);

        std::ostringstream oss;
        oss << "{"
            << "\"ok\":true,"
            << "\"rssi_dbm\":" << g_radio.rssi_dbm << ","
            << "\"snr_db\":" << g_radio.snr_db << ","
            << "\"last_rx_time\":\"" << g_radio.last_rx_hhmmss << "\","
            << "\"pkts_last_min\":" << radio_pkts_last_min()
            << "}";

        res.set_content(oss.str(), "application/json");
        res.status = 200;
        });





    // POST /api/command  (what Node-RED will call)
    svr.Post("/api/command", [](const httplib::Request& req, httplib::Response& res) {
        int seq = ++g_cmd_seq;
        std::string cmd = detect_cmd(req.body);

        //  Add to pending list when we receive a command
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_pending.push_back(PendingCommand{
                seq,                                // cmd_seq
                cmd,                                // cmd
                3,                                  // retries_left
                std::chrono::steady_clock::now(),    // last_tx
                5,                                  // timeout_s
                "WAITING_ACK"
                });
            g_last_tx_time = now_hhmmss();

        }

        std::string json =
            "{"
            "\"ok\": true, "
            "\"cmd_seq\": " + std::to_string(seq) + ", "
            "\"status\": \"QUEUED\", "
            "\"received_cmd\": \"" + cmd + "\""
            "}";

        res.set_content(json, "application/json");
        res.status = 200;
        });



    svr.Get("/api/pending", [](const httplib::Request&, httplib::Response& res) {
        using namespace std::chrono;
        auto now = steady_clock::now();

        std::lock_guard<std::mutex> lk(g_mtx);

        // Update states (simulate timeout + retry)
        for (auto& p : g_pending) {
            if (p.state == "TIMEOUT") continue;

            int elapsed_s = (int)duration_cast<seconds>(now - p.last_tx).count();
            if (elapsed_s >= p.timeout_s) {
                if (p.retries_left > 0) {
                    p.retries_left--;
                    p.last_tx = now; // "re-send now"
                    p.state = "RETRYING";
                }
                else {
                    p.state = "TIMEOUT";
                }
            }
            else {
                // After a retry, go back to waiting
                if (p.state == "RETRYING") p.state = "WAITING_ACK";
            }
        }

        // Build JSON array
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < g_pending.size(); i++) {
            const auto& p = g_pending[i];
            int elapsed_s = (int)duration_cast<seconds>(now - p.last_tx).count();
            int remaining = std::max(0, p.timeout_s - elapsed_s);

            oss << "{"
                << "\"cmd_seq\":" << p.cmd_seq << ","
                << "\"cmd\":\"" << p.cmd << "\","
                << "\"retries_left\":" << p.retries_left << ","
                << "\"last_tx_time\":\"" << now_hhmmss() << "\","
                << "\"timeout_remaining_s\":" << remaining << ","
                << "\"timeout_s\":" << p.timeout_s << ","
                << "\"state\":\"" << p.state << "\""
                << "}";
            if (i + 1 < g_pending.size()) oss << ",";
        }
        oss << "]";

        res.set_content(oss.str(), "application/json");
        res.status = 200;
        });

    std::cout << "Backend stub running on http://127.0.0.1:5000\n";


    svr.Post("/api/reset", [](const httplib::Request&, httplib::Response& res) {
        size_t cleared = 0;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            cleared = g_pending.size();
            g_pending.clear();
        }

  
        g_cmd_seq.store(0);

        std::string json =
            "{"
            "\"ok\": true, "
            "\"cleared_pending\": " + std::to_string(cleared) +
            "}";

        res.set_content(json, "application/json");
        res.status = 200;
        });


    

    svr.Post("/api/radio/connect", [](const httplib::Request&, httplib::Response& res) {
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_radio_connected = true;
            g_radio.connected = true;

        }
        res.set_content("{\"ok\":true,\"connected\":true}", "application/json");
        res.status = 200;
        });

    svr.Post("/api/radio/disconnect", [](const httplib::Request&, httplib::Response& res) {
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_radio_connected = false;
            g_radio.connected = false;

        }
        res.set_content("{\"ok\":true,\"connected\":false}", "application/json");
        res.status = 200;
        });

    svr.Get("/api/radio/status", [](const httplib::Request&, httplib::Response& res) {
        bool connected;
        std::string last_tx, last_rx;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            connected = g_radio_connected;
            last_tx = g_last_tx_time;
            last_rx = g_last_rx_time;
        }

        std::string json =
            std::string("{")
            + "\"ok\":true,"
            + "\"connected\":" + (connected ? "true" : "false") + ","
            + "\"last_tx_time\":\"" + last_tx + "\","
            + "\"last_rx_time\":\"" + last_rx + "\""
            + "}";

        res.set_content(json, "application/json");
        res.status = 200;
        });


    svr.listen("0.0.0.0", 5000);
    return 0;
}
