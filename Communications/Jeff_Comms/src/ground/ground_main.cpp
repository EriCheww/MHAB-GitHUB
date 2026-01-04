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







static constexpr int MAX_CMD_RETRIES = 3;  // number of re-transmits after the first send
static constexpr auto CMD_ACK_TIMEOUT = std::chrono::milliseconds(800); // wait before retry

static void mark_command_timeout(PendingCommand& cmd) {
    cmd.status = PendingCommand::Status::DONE;
    cmd.result = PendingCommand::Result::TIMEOUT;
    std::cout << "[cmd] TIMEOUT cmd_seq=" << cmd.cmd_seq << "\n";
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



// ReceiveDownlinkStep()
// (non-blocking, handle TELEMETRY and ACK frames from balloon -> ground)
static void ReceiveDownlinkStep() {
    auto frame_opt = simulated_receive_over_channel();
    if (!frame_opt.has_value()) return; // non-blocking: nothing received

    const std::vector<uint8_t>& rx_frame = *frame_opt;

    // Downlink direction: balloon -> ground
    if (!packetizer_rx.Decode(rx_frame, DIR_DOWNLINK, g_key)) {
        g_link_stats.drop_count++;
        std::cerr << "[rx] drop frame (decode/auth failed)\n";
        return;
    }

    // Sanity checks (keep cheap & strict)
    if (packetizer_rx.telemetry.protocol_Version != 1) return;
    if (packetizer_rx.telemetry.sync0 != 67) return;
    if (packetizer_rx.telemetry.sync1 != 8)  return;

    g_link_stats.last_rx_time = Clock::now();
    g_link_stats.last_rx_seq  = packetizer_rx.telemetry.sequence_Number;

    const uint8_t rt = packetizer_rx.telemetry.record_Type;

    // ---------------- Telemetry ----------------
    if (rt == 1) {
        g_link_stats.telemetry_count++;
        g_link_stats.last_telemetry_time = Clock::now();

        // For now we treat telemetry payload as a string (your dummy payload is a string)
        const std::string payload_str = bytes_to_string(packetizer_rx.telemetry.payload_buffer);

        std::cout << "[telemetry] seq=" << packetizer_rx.telemetry.sequence_Number
                  << " bytes=" << packetizer_rx.telemetry.payload_buffer.size()
                  << " payload=\"" << payload_str << "\"\n";

        // TODO (later):
        // - parse telemetry fields (GPS, alt, battery...)
        // - log to file (CSV/JSON)
        // - update GUI plots
        return;
    }

    //ACK
    if (rt == 4) {
        g_link_stats.ack_count++;

        uint64_t cmd_seq = 0;
        AckStatus st = AckStatus::ERROR;

        if (!parse_ack_payload(packetizer_rx.telemetry.payload_buffer, cmd_seq, st)) {
            std::cerr << "[ack] invalid payload format (bytes="
                      << packetizer_rx.telemetry.payload_buffer.size() << ")\n";
            return;
        }

        std::cout << "[ack] downlink_seq=" << packetizer_rx.telemetry.sequence_Number
                  << " cmd_seq=" << cmd_seq
                  << " status=" << (st == AckStatus::OK ? "OK" : "ERROR")
                  << "\n";

        // Update pending command state machine (Algorithm 6 will extend retries/timeouts)
        mark_command_done(cmd_seq, st);
        return;
    }

    // Unknown record type 
    std::cout << "[rx] ignore unsupported record_Type=" << static_cast<int>(rt)
              << " seq=" << packetizer_rx.telemetry.sequence_Number << "\n";
}

static void PollGuiAndQueueCommands();
static void SendPendingCommandsStep() {
    const auto now = Clock::now();

    for (auto& cmd : g_pending_commands) {

        // DONE: do nothing (ACK already received OR timed out)
        if (cmd.status == PendingCommand::Status::DONE) {
            continue;
        }

        // NEW: send immediately once
        if (cmd.status == PendingCommand::Status::NEW) {

            // If retries_left wasn't initialized by queue logic, set it here safely
            if (cmd.retries_left <= 0) {
                cmd.retries_left = MAX_CMD_RETRIES;
            }

            if (!simulated_send_over_channel(cmd.frame)) {
                std::cerr << "[tx] failed to send command cmd_seq=" << cmd.cmd_seq << "\n";
                // You can choose to keep it NEW or mark as timeout/error; we keep it NEW for now.
                continue;
            }

            cmd.last_tx_time = now;
            cmd.status = PendingCommand::Status::WAITING_ACK;

            std::cout << "[tx] sent command cmd_seq=" << cmd.cmd_seq
                      << " (retries_left=" << cmd.retries_left << ")\n";
            continue;
        }

        // WAITING_ACK: check timeout
        if (cmd.status == PendingCommand::Status::WAITING_ACK) {

            const auto elapsed = now - cmd.last_tx_time;

            // Not timed out yet: keep waiting
            if (elapsed < CMD_ACK_TIMEOUT) {
                continue;
            }

            // Timed out: either retry or finish as TIMEOUT
            if (cmd.retries_left > 0) {
                cmd.retries_left--;

                if (!simulated_send_over_channel(cmd.frame)) {
                    std::cerr << "[tx] retry send failed cmd_seq=" << cmd.cmd_seq << "\n";
                    // If send fails, you could choose to not decrement retries, but we already did.
                    // Keeping logic simple: we still wait again.
                } else {
                    std::cout << "[tx] RETRY cmd_seq=" << cmd.cmd_seq
                              << " (retries_left=" << cmd.retries_left << ")\n";
                }

                cmd.last_tx_time = now;          // reset the timer regardless
                cmd.status = PendingCommand::Status::WAITING_ACK;
            } else {
                // no retries left => DONE/TIMEOUT
                mark_command_timeout(cmd);
            }
        }
    }
}
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
