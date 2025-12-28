// Communications/src/ground/ground_main.cpp


#include <vector>
#include <iostream>
#include <cstdint>
#include <thread>
#include <chrono>
#include <fstream>
#include <optional>
#include <deque>
#include <mutex>
#include <random>

#include <sodium.h>

#include "../../include/packetizer.hpp"
#include "../../include/packet.hpp"



static constexpr size_t MAX_LORA_BYTES = 255;

// For now: a single queue placeholder (will be replaced)
static std::mutex g_chan_mtx;
static std::deque<std::vector<uint8_t>> g_chan_queue;

static double g_drop_prob = 0.0;
static double g_bitflip_prob = 0.0;
static std::mt19937 rng{std::random_device{}()};

static void maybe_corrupt(std::vector<uint8_t>& frame) {
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    if (g_bitflip_prob <= 0.0) return;
    if (prob(rng) > g_bitflip_prob) return;
    if (frame.empty()) return;

    std::uniform_int_distribution<size_t> byte_dist(0, frame.size() - 1);
    std::uniform_int_distribution<int> bit_dist(0, 7);
    size_t i = byte_dist(rng);
    int bit = bit_dist(rng);
    frame[i] ^= static_cast<uint8_t>(1u << bit);
}

static bool simulated_send_over_channel(const std::vector<uint8_t>& frame_in) {
    if (frame_in.size() > MAX_LORA_BYTES) {
        std::cerr << "[sim] frame too large: " << frame_in.size() << "\n";
        return false;
    }

    std::uniform_real_distribution<double> prob(0.0, 1.0);
    if (g_drop_prob > 0.0 && prob(rng) < g_drop_prob) {
        std::cerr << "[sim] dropped a frame\n";
        return true;
    }

    std::vector<uint8_t> frame = frame_in;
    maybe_corrupt(frame);

    {
        std::lock_guard<std::mutex> lock(g_chan_mtx);
        g_chan_queue.push_back(std::move(frame));
    }
    return true;
}

static std::optional<std::vector<uint8_t>> simulated_receive_over_channel() {
    std::lock_guard<std::mutex> lock(g_chan_mtx);
    if (g_chan_queue.empty()) return std::nullopt;

    std::vector<uint8_t> frame = std::move(g_chan_queue.front());
    g_chan_queue.pop_front();
    return frame;
}


// Direction bits (must match balloon side)

static constexpr uint8_t DIR_UPLINK   = 1; // ground -> balloon
static constexpr uint8_t DIR_DOWNLINK = 0; // balloon -> ground



// Ground-side state (Algorithm 6 will extend this later)

using Clock = std::chrono::steady_clock;

struct PendingCommand {
    uint64_t cmd_seq = 0;                   // command sequence (for matching ACK)
    std::vector<uint8_t> frame;             // encoded+encrypted frame bytes
    enum class Status { NEW, WAITING_ACK, DONE } status = Status::NEW;
    enum class Result { NONE, OK, ERROR, TIMEOUT } result = Result::NONE;

    Clock::time_point last_tx_time{};
    int retries_left = 0;
};


// Globals (keep simple for now)
static std::vector<uint8_t> g_key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);

static Packetizer packetizer_rx;
static Packetizer packetizer_tx;

static std::vector<PendingCommand> g_pending_commands;



static void ReceiveDownlinkStep();
static void PollGuiAndQueueCommands();
static void SendPendingCommandsStep();
static void UpdateUI();


// Helpers
static bool load_key_file(const std::string& path, std::vector<uint8_t>& key) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return false;

    input.seekg(0, std::ios::end);
    std::streamsize fsize = input.tellg();
    input.seekg(0, std::ios::beg);

    if (fsize != static_cast<std::streamsize>(key.size())) {
        std::cerr << "[key] expected " << key.size() << " bytes but got " << fsize << "\n";
        return false;
    }

    input.read(reinterpret_cast<char*>(key.data()), fsize);
    return true;
}


// ReceiveDownlinkStep()
// (non-blocking, handle TELEMETRY and ACK)
static void ReceiveDownlinkStep() {
    auto frame_opt = simulated_receive_over_channel();
    if (!frame_opt.has_value()) return; // non-blocking: nothing received

    const std::vector<uint8_t>& rx_frame = *frame_opt;

    // Downlink means balloon->ground, so DIR_DOWNLINK
    if (!packetizer_rx.Decode(rx_frame, DIR_DOWNLINK, g_key)) {
        std::cerr << "[rx] drop frame (decode/auth failed)\n";
        return;
    }

    // Basic sanity
    if (packetizer_rx.telemetry.protocol_Version != 1) return;
    if (packetizer_rx.telemetry.sync0 != 67) return;
    if (packetizer_rx.telemetry.sync1 != 8)  return;

    const uint8_t rt = packetizer_rx.telemetry.record_Type;

    if (rt == 1) {
        // TELEMETRY
        std::cout << "[telemetry] bytes=" << packetizer_rx.telemetry.payload_buffer.size()
                  << " seq=" << packetizer_rx.telemetry.sequence_Number << "\n";
        // TODO: decode telemetry payload into fields + log to disk
        return;
    }

    if (rt == 4) {

        std::cout << "[ack] payload_bytes=" << packetizer_rx.telemetry.payload_buffer.size()
                  << " seq=" << packetizer_rx.telemetry.sequence_Number << "\n";
        return;
    }

    // unknown record types ignored
}


// Algorithm 4: PollGuiAndQueueCommands()
// For now: CLI stub (later replace with GUI)
static void PollGuiAndQueueCommands() {

}


static void SendPendingCommandsStep() {
    // Placeholder:
    // Later implement NEW -> WAITING_ACK, retry/timeouts, DONE states.

    for (auto& cmd : g_pending_commands) {
        if (cmd.status == PendingCommand::Status::DONE) continue;

        if (cmd.status == PendingCommand::Status::NEW) {
            // Send once
            simulated_send_over_channel(cmd.frame);
            cmd.last_tx_time = Clock::now();
            cmd.status = PendingCommand::Status::WAITING_ACK;
            std::cout << "[tx] sent command seq=" << cmd.cmd_seq << "\n";
        }
    }
}



static void UpdateUI() {
    // Placeholder for GUI refresh:
    // - link quality, last telemetry time, pending commands list, etc.
}



// Main = Algorithm 4: GROUNDMAINLOOP

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

   
    if (!load_key_file("../xchacha_keygen.bin", g_key)) {
        std::cerr << "Failed to load key file: ../xchacha_keygen.bin\n";
        return 1;
    }

    while (true) {
        ReceiveDownlinkStep();      // non-blocking
        PollGuiAndQueueCommands();  // GUI/CLI input -> queue
        SendPendingCommandsStep();  // send queued commands (later: retry/timeout)
        UpdateUI();                 // UI refresh
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
