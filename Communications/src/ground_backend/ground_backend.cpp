#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

#include "../../external/httplib.h"

#include "../../external/nlohmann/json.hpp"
using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

// --------------------------- Shared State ---------------------------
struct LinkStatus {
    // null = unknown, false = disconnected, true = connected
    bool connected = false;
    std::string last_tx_time = "-";
    std::string last_rx_time = "-";
    double rssi_dbm = 0.0;
    double snr_db = 0.0;
};

struct PendingRow {
    uint64_t cmd_seq = 0;
    std::string command = "";
    std::string state = "NEW"; // NEW / WAITING_ACK / DONE
    int retries_left = 0;
    int timeout_remain_s = 0;
    int timeout_s = 0;
    std::string last_tx = "-";
};

struct HistoryRow {
    std::string time = "";
    std::string command = "";
    uint64_t cmd_seq = 0;
    std::string status = "QUEUED"; // QUEUED / WAITING_ACK / DONE
    std::string result = "NONE";   // NONE / OK / ERROR / TIMEOUT
};

// This is the single source of truth for Node-RED to poll
static std::mutex g_mtx;
static LinkStatus g_link;
static std::vector<PendingRow> g_pending;
static std::vector<HistoryRow> g_history;

// incrementing cmd seq
static std::atomic<uint64_t> g_cmd_seq{ 1 };

// --------------------------- Helpers ---------------------------
static std::string now_hhmmss() {
    // simple local time string for UI (you can replace later with ISO)
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

// --------------------------- Comms Engine (placeholder loop) ---------------------------
// Later: put Algorithm 4–6 real logic here (ReceiveDownlinkStep / SendPendingCommandsStep).
static std::atomic<bool> g_run{ true };

static void comms_loop() {
    while (g_run.load()) {
        // TODO: ReceiveDownlinkStep();  -> update g_link.last_rx_time, telemetry, ACKs
        // TODO: SendPendingCommandsStep(); -> update g_link.last_tx_time, pending retries/timeouts

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// --------------------------- HTTP API ---------------------------
int main() {
    // Start comms loop thread
    std::thread t(comms_loop);

    httplib::Server svr;

    // health check (useful for debugging)
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("OK", "text/plain");
        });

    // Link status (Node-RED polls)
    svr.Get("/api/link/status", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);
        json j;
        j["ok"] = true;
        j["connected"] = g_link.connected;              // bool
        j["last_tx_time"] = g_link.last_tx_time;        // string or "-"
        j["last_rx_time"] = g_link.last_rx_time;        // string or "-"
        j["rssi_dbm"] = g_link.rssi_dbm;
        j["snr_db"] = g_link.snr_db;
        res.set_content(j.dump(), "application/json");
        });

    // Connect / Disconnect
    svr.Post("/api/link/connect", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_link.connected = true;
        // optional: treat connect as a TX event
        g_link.last_tx_time = now_hhmmss();
        json j = { {"ok", true}, {"connected", true} };
        res.set_content(j.dump(), "application/json");
        });

    svr.Post("/api/link/disconnect", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_link.connected = false;
        g_link.last_tx_time = now_hhmmss();
        json j = { {"ok", true}, {"connected", false} };
        res.set_content(j.dump(), "application/json");
        });

    // Pending table (Node-RED polls)
    svr.Get("/api/commands/pending", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);
        json j;
        j["ok"] = true;
        j["items"] = json::array();
        for (auto& r : g_pending) {
            j["items"].push_back({
                {"cmd_seq", r.cmd_seq},
                {"command", r.command},
                {"state", r.state},
                {"retries_left", r.retries_left},
                {"timeout_remain_s", r.timeout_remain_s},
                {"timeout_s", r.timeout_s},
                {"last_tx", r.last_tx}
                });
        }
        res.set_content(j.dump(), "application/json");
        });

    // History table (Node-RED polls)
    svr.Get("/api/commands/history", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lk(g_mtx);
        json j;
        j["ok"] = true;
        j["items"] = json::array();
        for (auto& r : g_history) {
            j["items"].push_back({
                {"time", r.time},
                {"command", r.command},
                {"cmd_seq", r.cmd_seq},
                {"status", r.status},
                {"result", r.result}
                });
        }
        res.set_content(j.dump(), "application/json");
        });

    // Command enqueue (Node-RED button -> POST)
    // Expected body example: {"command":"shutdown","args":{}}
    svr.Post("/api/command", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        }
        catch (...) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"bad json"})", "application/json");
            return;
        }

        const std::string cmd = body.value("command", "");
        if (cmd.empty()) {
            res.status = 400;
            res.set_content(R"({"ok":false,"error":"missing command"})", "application/json");
            return;
        }

        const uint64_t seq = g_cmd_seq.fetch_add(1);

        {
            std::lock_guard<std::mutex> lk(g_mtx);

            // Add to pending
            PendingRow p;
            p.cmd_seq = seq;
            p.command = cmd;
            p.state = "WAITING_ACK";
            p.retries_left = 3;
            p.timeout_remain_s = 5;
            p.timeout_s = 5;
            p.last_tx = now_hhmmss();
            g_pending.push_back(p);

            // Add to history
            HistoryRow h;
            h.time = now_hhmmss();
            h.command = cmd;
            h.cmd_seq = seq;
            h.status = "QUEUED";
            h.result = "NONE";
            g_history.push_back(h);

            // Update link TX timestamp
            g_link.last_tx_time = p.last_tx;
        }

        json reply = {
            {"ok", true},
            {"cmd_seq", seq},
            {"status", "QUEUED"},
            {"command", cmd}
        };
        res.set_content(reply.dump(), "application/json");
        });

    // Start server
    // NOTE: if you want only local, use "127.0.0.1"
    // If you want other PCs to access, use "0.0.0.0"
    // Sanity: root
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ground_backend alive", "text/plain");
        });

    // Sanity: status
    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["ok"] = true;
        j["service"] = "ground_backend";
        res.set_content(j.dump(), "application/json");
        });

    svr.listen("0.0.0.0", 5000);

    // Shutdown
    g_run.store(false);
    t.join();
    return 0;
}
