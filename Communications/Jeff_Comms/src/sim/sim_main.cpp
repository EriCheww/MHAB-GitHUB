#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <atomic>

#include <sodium.h>

#include "../../include/packetizer.hpp"
#include "../../include/raw_data_dummy.hpp"

// Direction bit (must match your Packetizer nonce rule)
static constexpr uint8_t DIR_UPLINK   = 1; // ground -> balloon
static constexpr uint8_t DIR_DOWNLINK = 0; // balloon -> ground

// -------------------- utilities --------------------
static std::vector<uint8_t> to_bytes(const std::string& s) {
    return {s.begin(), s.end()};
}
static std::string bytes_to_string(const std::vector<uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

// -------------------- simulated channel (two queues) --------------------
struct SimChannel {
    std::mutex mtx;
    std::deque<std::vector<uint8_t>> downlink_q; // balloon -> ground
    std::deque<std::vector<uint8_t>> uplink_q;   // ground -> balloon (not used yet)

    void send_downlink(const std::vector<uint8_t>& frame) {
        std::lock_guard<std::mutex> lock(mtx);
        downlink_q.push_back(frame);
    }

    std::optional<std::vector<uint8_t>> recv_downlink_nonblocking() {
        std::lock_guard<std::mutex> lock(mtx);
        if (downlink_q.empty()) return std::nullopt;
        auto f = std::move(downlink_q.front());
        downlink_q.pop_front();
        return f;
    }
};

// -------------------- balloon role --------------------
void BalloonLoop(SimChannel& chan,
                 const std::vector<uint8_t>& key,
                 std::atomic<bool>& stop_flag)
{
    Packetizer tx;
    TelemetryPayloadDummy dummy;

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now() - std::chrono::seconds(10);

    while (!stop_flag.load()) {
        auto now = Clock::now();

        // For testing: send every 1 second (instead of 10s)
        if (now - last >= std::chrono::seconds(1)) {
            // Build telemetry payload
            std::vector<uint8_t> payload = to_bytes(dummy.dummy_Data);

            // Packet fields
            tx.telemetry.record_Type = 1;        // telemetry
            tx.telemetry.sequence_Number += 1;   // increment seq

            // Encode + encrypt (balloon sending downlink => DIR_DOWNLINK)
            if (!tx.encode_and_encrypt(payload, key, DIR_DOWNLINK)) {
                std::cerr << "[balloon] encode_and_encrypt failed\n";
            } else {
                // "Transmit" into simulated channel
                chan.send_downlink(tx.packet);
                std::cout << "[balloon] sent telemetry seq=" << tx.telemetry.sequence_Number
                          << " bytes=" << tx.packet.size() << "\n";
            }

            last = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// -------------------- ground role --------------------
void GroundLoop(SimChannel& chan,
                const std::vector<uint8_t>& key,
                std::atomic<bool>& stop_flag)
{
    Packetizer rx;
    int received = 0;

    while (!stop_flag.load()) {
        auto frame_opt = chan.recv_downlink_nonblocking();
        if (!frame_opt.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const auto& frame = *frame_opt;

        // Decode downlink => MUST use DIR_DOWNLINK
        if (!rx.Decode(frame, DIR_DOWNLINK, key)) {
            std::cerr << "[ground] Decode failed (corrupt/auth/format)\n";
            continue;
        }

        if (rx.telemetry.record_Type == 1) {
            std::string msg = bytes_to_string(rx.telemetry.payload_buffer);
            std::cout << "[ground] got telemetry seq=" << rx.telemetry.sequence_Number
                      << " payload=\"" << msg << "\"\n";

            received++;
            if (received >= 5) { // stop after 5 packets (test done)
                stop_flag.store(true);
            }
        } else {
            std::cout << "[ground] got non-telemetry record_type="
                      << int(rx.telemetry.record_Type) << "\n";
        }
    }
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    // Create key
    std::vector<uint8_t> key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);

    // For now: generate random key for simulation (simplest & always works)
    randombytes_buf(key.data(), key.size());
    std::cout << "[sim] using random key (single-process sim is fine)\n";

    SimChannel chan;
    std::atomic<bool> stop_flag{false};

    std::thread t_balloon(BalloonLoop, std::ref(chan), std::cref(key), std::ref(stop_flag));
    std::thread t_ground (GroundLoop,  std::ref(chan), std::cref(key), std::ref(stop_flag));

    t_balloon.join();
    t_ground.join();

    std::cout << "[sim] done\n";
    return 0;
}
