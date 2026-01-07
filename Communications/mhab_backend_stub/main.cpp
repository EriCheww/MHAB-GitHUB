#include <iostream>
#include <string>
#include <atomic>
#include "httplib.h"

static std::atomic<int> g_cmd_seq{0};

static std::string detect_cmd(const std::string& body) {
    // Minimal “parsing” (good enough for stub testing)
    if (body.find("\"cmd\"") != std::string::npos && body.find("shutdown") != std::string::npos) return "shutdown";
    if (body.find("\"cmd\"") != std::string::npos && body.find("recalibrate") != std::string::npos) return "recalibrate";
    return "unknown";
}

int main() {
    httplib::Server svr;

    // POST /api/command  (what Node-RED will call)
    svr.Post("/api/command", [](const httplib::Request& req, httplib::Response& res) {
        int seq = ++g_cmd_seq;
        std::string cmd = detect_cmd(req.body);

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


    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("pong", "text/plain");
        res.status = 200;
    });

    std::cout << "Backend stub running on http://127.0.0.1:5000\n";
    svr.listen("0.0.0.0", 5000);
    return 0;
}
