
#ifndef LINKLAYER_H
#define LINKLAYER_H

//Function - This headers job is so simulate a channel (temoporarily) for bit flips, and implement the algorithm responsible for coordinating packetizing, encrypting, decrypting, etc. 

#include <cstdint> //for all the uintx_t
#include <vector>//for std::vector
#include <cstddef> // for size_t 
#include <cstring> // for memcpy
#include <iostream> 
#include <chrono>
#include <thread>
#include <filesystem>

#include <sodium.h> // The library we're gonna be using for encryption + authenthication

#include "packet.hpp"
#include "packetizer.hpp"
#include "raw_data_dummy.hpp"
#include "payload_command_struct.hpp"

// below 4 libs are for simulation 
#include <optional>
#include <deque>
#include <mutex>
#include <random>

//Start the global clock which we'll use to check against if 10 seconds has passed - 
//abstract this function to just be "Clock"
using Clock = std::chrono::steady_clock;

class LinkLayer{ 
    private : 
        //Hidden attributes that other classes and main can't see, only the functions within this class- 
        const std::vector<uint8_t>& key_;
        //Setting the direction bit - 0 is for balloon side , while 1 is for ground side.
        static constexpr uint8_t DIR_UPLINK_ = 1; //for ground to balloon 
        static constexpr uint8_t DIR_DOWNLINK_ = 0; //for balloon to ground
        Clock::time_point last_telemetry_ = Clock::now() - std::chrono::seconds(10);
        std::vector<uint8_t> temp_payload_ ; //temporary payload storage -
        std::vector<uint8_t> buffer_temp_ ; //temporary storage for simulating a 255 byte LoRa buffer - 

        //Calling all headers/ structs and classes and initializing them  - 
        Packetizer packetizer_rx_ ;
        Packetizer packetizer_tx_ ;  
        TelemetryPayloadDummy payload_dummy_ ;   


        //Defining the algorithm two structs for the special case logic - 
        AckPayload ackpayload_ ; 
        CommandPayload commandpayload_ ; 

        static constexpr size_t MAX_LORA_BYTES = 255;

        //Defining the Duplication logic variables - 
        uint64_t last_cmd_seq_ = 0 ; 
        bool first_cmd_ = false;

        static constexpr uint8_t BALLOON_ID = 1;// used for comparing to the balloon and ground ID - 
        static constexpr uint8_t GROUND_ID  = 2; 

        // Channel queue: frames "in the air"
        //removed const since that causes a syntax error for some reason - 
        inline static std::mutex g_uplink_mtx;
        inline static std::deque<std::vector<uint8_t>> g_uplink_queue;   // Ground -> Balloon

        inline static std::mutex g_downlink_mtx;
        inline static std::deque<std::vector<uint8_t>> g_downlink_queue; // Balloon -> Ground

        // Removed static and const since that also causes a syntax error, and also doesn't make sense since we want to maintain randomness across different instances - 
        std::mt19937 rng{std::random_device{}()};

        // Error injection (default OFF)
        double g_drop_prob = 0.0;      // e.g. 0.05 = 5% drop
        double g_bitflip_prob = 0.0;   // e.g. 0.02 = 2% frames get 1 random bit flip

    public : 

    explicit LinkLayer(const std::vector<uint8_t>& key) : key_(key) {
    //everything that defines the packet will be appended in the method
    }

    //To easily convert any payload data from string to bytes since we eventually want the data to be bytes std::vector<uint8_t> vec - 
    std::vector<uint8_t> to_bytes(const std::string& s) {
        return {s.begin(), s.end()};
    }

    //SIMULATED CHANNEL - 


    void maybe_corrupt(std::vector<uint8_t>& frame) {
        std::uniform_real_distribution<double> prob(0.0, 1.0);

        // Drop is handled in send; here we do bit flip only.
        if (g_bitflip_prob <= 0.0) return;
        if (prob(rng) > g_bitflip_prob) return;

        if (frame.empty()) return;

        std::uniform_int_distribution<size_t> byte_dist(0, frame.size() - 1);
        std::uniform_int_distribution<int> bit_dist(0, 7);

        size_t i = byte_dist(rng);
        int bit = bit_dist(rng);
        frame[i] ^= static_cast<uint8_t>(1u << bit);  // flip one bit
    }

    // Balloon TX (Telemetry + ACK): push to DOWNLINK
    bool simulated_send_downlink(const std::vector<uint8_t>& frame_in) {
        if (frame_in.size() > MAX_LORA_BYTES) {
            std::cerr << "[sim] downlink frame too large: "
                    << frame_in.size() << " > " << MAX_LORA_BYTES << "\n";
            return false;
        }

        std::uniform_real_distribution<double> prob(0.0, 1.0);
        if (g_drop_prob > 0.0 && prob(rng) < g_drop_prob) {
            std::cerr << "[sim] dropped a downlink frame\n";
            return true;
        }

        std::vector<uint8_t> frame = frame_in;
        maybe_corrupt(frame);

        {
            std::lock_guard<std::mutex> lock(g_downlink_mtx);
            g_downlink_queue.push_back(std::move(frame));
        }
        return true;
    }

    // Balloon RX (Commands): pop from UPLINK
    std::optional<std::vector<uint8_t>> simulated_receive_uplink() {
        std::lock_guard<std::mutex> lock(g_uplink_mtx);
        if (g_uplink_queue.empty()) return std::nullopt;

        auto frame = std::move(g_uplink_queue.front());
        g_uplink_queue.pop_front();
        return frame;
    }

    // Ground injects uplink frames to balloon
    static void SimGroundInjectUplink(std::vector<uint8_t> frame) {
        std::lock_guard<std::mutex> lock(g_uplink_mtx);
        g_uplink_queue.push_back(std::move(frame));
    }

    // Ground reads what balloon transmitted
    static std::optional<std::vector<uint8_t>> SimGroundReadDownlink() {
        std::lock_guard<std::mutex> lock(g_downlink_mtx);
        if (g_downlink_queue.empty()) return std::nullopt;
        auto frame = std::move(g_downlink_queue.front());
        g_downlink_queue.pop_front();
        return frame;
    }

    // Pseudocode from the PDR implemented here - Balloon side: 
    //Send command ack simulates command sending the packet over the air downlink - 
    void SendCommandACK(uint64_t cmd_seq, AckStatus status) {
        std::vector<uint8_t> ack_payload;
        ack_payload.reserve(9);

        for (int i = 0; i < 8; ++i) {
            ack_payload.push_back(static_cast<uint8_t>((cmd_seq >> (8*i)) & 0xFF));
        }

        ack_payload.push_back(static_cast<uint8_t>(status));

        packetizer_tx_.telemetry.record_Type = 4;
        packetizer_tx_.telemetry.sequence_Number++;

        if (!packetizer_tx_.encode_and_encrypt(ack_payload, key_, DIR_DOWNLINK_)) return;
        simulated_send_downlink(packetizer_tx_.packet);
    }

    //Input is the previous cmd_seq- 
    bool IsDuplicateSeq (uint64_t last_seq) {
        //one way this works is by comparing the new sequence number that hasn't overwritten cmd_seq yet with the previous id (i.e. unupdated cmd_seq)
        
        if (!first_cmd_) {
            // This is the first command ever. It can't be a duplicate.
            return false;
        }
        
        if (last_seq==ackpayload_.cmd_seq) {
            return true ; //yes there are duplicates
        }
        return false ; // no there isn't any duplicates  
    }

    void TelemetryStep() {
        //Getting the last time we requested for telemetry to be sent -  
        //Sending telemetry data - 
        std::chrono::steady_clock::time_point  current_time = Clock::now();

        if (current_time-last_telemetry_ >= std::chrono::seconds(10)) {
            // send telemetry
            //Convert from string to bytes (temporary fix)
            temp_payload_= to_bytes(payload_dummy_.dummy_Data) ; 
            //Append sequence number - 
            packetizer_tx_.telemetry.sequence_Number++ ; 
            //Set record type (1 is for telemetry) - 
            packetizer_tx_.telemetry.record_Type = 1 ; 
            //Encrypt and encode - 
            if (!packetizer_tx_.encode_and_encrypt(temp_payload_, key_, DIR_DOWNLINK_)) return ;  // I changed DIR_UPLINK to DIR_DOWNLINK since this is balloon side sending data downlink
            simulated_send_downlink(packetizer_tx_.packet);
            last_telemetry_ = current_time;
        }
    }

    bool valid_cmd(uint8_t c0, uint8_t c1) {
        return (c0 == 0xA2 && c1 == 0x08) ||
            (c0 == 0xA3 && c1 == 0x09) ||
            (c0 == 0xA4 && c1 == 0x10);
    }


    bool ReceiveCommandStep() {
        // Non-blocking receive
        auto frame_opt = simulated_receive_uplink();
        if (!frame_opt.has_value()) {
            return false; // no frame this cycle
        }
        const std::vector<uint8_t>& rx_frame = *frame_opt;

        // Decode + authenticate (uplink: ground -> balloon)
        if (!packetizer_rx_.Decode(rx_frame, DIR_UPLINK_, key_)) {
            return false; // corrupted / auth failed / malformed
        }

        // Basic protocol sanity checks
        if (packetizer_rx_.telemetry.protocol_Version != 1) return false;
        if (packetizer_rx_.telemetry.sync0 != 67) return false;
        if (packetizer_rx_.telemetry.sync1 != 8)  return false;
    
        // return true: valid COMMAND received and handled
        // return false: no frame / invalid frame / or valid non-command (telemetry/ack)
        const uint8_t rt = packetizer_rx_.telemetry.record_Type;

        if (rt == 1) {
            // Telemetry (not expected on balloon uplink, but ignore safely)
            return false;
        }

        if (rt == 4) {
            // ACK (not a command)
            return false;
        }

        if (rt != 6) {
            // Unknown/unsupported record type
            return false;
        }

        if (packetizer_rx_.telemetry.destination_Id != BALLOON_ID) return false;
        if (packetizer_rx_.telemetry.source_Id != GROUND_ID) return false;

        // rt == 6 : Command
        // NOTE: telemetry.payload_buffer holds decrypted payload after Decode()
        //Assuming this is the case where the packet is successfully decoded but the inputted command is incorrect case - 
        if (packetizer_rx_.telemetry.payload_buffer.size() < 10) {
            // invalid command payload
            //Since there isn't even enough bytes to decode we should send the frame sequence number back ... 
            SendCommandACK(packetizer_rx_.telemetry.sequence_Number, AckStatus::ERROR);
            return true; // we did receive a command-type packet and handled it (error path)
        } else {
            int i =2 ; 
            //Else if all checks pass remember the sequence number - 
            ackpayload_.cmd_seq = static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i]) | 
            (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i + 1]) << 8) | 
            (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i+2])<<16) | 
            (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i+3])<<24)| 
            (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i+4])<<32)| (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i+5])<<40)| 
            (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i+6])<<48)| (static_cast<uint64_t>(packetizer_rx_.telemetry.payload_buffer[i+7])<<56) ;
        }

        //Assuming that the payload for the special case will be assembled as follow 
        // [byte_hi][byte_lo][cmd_seq]
        //Where bytes are one byte each, cmd_seq is uint64_t (8 bytes), and cmd_id is one byte. 

        //Once all the checks have been successfully gotten through, we can now actually decode the commands ... 
        // ... (here we don't need to store the cmd_seq since that's only for bookkeeping)
        const uint8_t c0 = packetizer_rx_.telemetry.payload_buffer[0];
        const uint8_t c1 = packetizer_rx_.telemetry.payload_buffer[1];

        //Check for duplicate commands just before the command execution to prevent re-executing the command incorrectly -
        //If not valid automatically send invalid command, if it is continue the code as normal -  
        if (!valid_cmd(c0, c1)) {
            std::cout << "Invalid Command\n";
            SendCommandACK(ackpayload_.cmd_seq, AckStatus::ERROR);
            return true;
        }

        // 2) only for valid commands, apply duplicate suppression
        if (IsDuplicateSeq(last_cmd_seq_)) {
            SendCommandACK(ackpayload_.cmd_seq, AckStatus::OK);
            return true;
        }

        last_cmd_seq_ = ackpayload_.cmd_seq ; // adding the last sequence number to the variable before we overwrite it 
        first_cmd_= true; // Mark that we have history now

        // Example commands (placeholder)
        if (c0 == 0xA2 && c1 == 0x08) {
            std::cout << "Recalibrating the telescope" << std::endl;
            SendCommandACK(ackpayload_.cmd_seq, AckStatus::OK);
            return true;
        } else if (c0 == 0xA3 && c1 == 0x09) {
            std::cout << "Prepare for shutdown" << std::endl;
            SendCommandACK(ackpayload_.cmd_seq, AckStatus::OK);
            return true;
        } else if (c0 == 0xA4 && c1 == 0x10) {
            std::cout << "Reset the stepper motor" << std::endl;
            SendCommandACK(ackpayload_.cmd_seq, AckStatus::OK);
            return true;
        } 

    }
} ; 

#endif