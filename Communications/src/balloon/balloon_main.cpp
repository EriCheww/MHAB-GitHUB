


//FUNCTION : This is the main balloon side loop that brings all the headers together and gathers all the telemetry, packetizes, it, encodes/ decodes  it, then sends it. 

#include <vector>  
#include <iostream>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

#include <sodium.h>

#include "../../include/packetizer.hpp"
#include "../../include/packet.hpp"
#include "../../include/raw_data_dummy.hpp"

namespace fs = std::filesystem;

//Start the global clock which we'll use to check against if 10 seconds has passed - 
using Clock = std::chrono::steady_clock;
static Clock::time_point last_telemetry = Clock::now() - std::chrono::seconds(10);
std::vector<uint8_t> temp_payload ; //temporary payload storage -
std::vector<uint8_t> buffer_temp ; //temporary storage for simulating a 255 byte LoRa buffer - 

//Setting the direction bit - 0 is for balloon side , while 1 is for ground side.
std::vector<uint8_t> key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES); 
uint8_t DIR_UPLINK = 1; //for ground to balloon 
uint8_t DIR_DOWNLINK = 0; //for balloon to ground

//Calling all headers/ structs and classes and initializing them  - 
Packetizer packetizer_rx ;
Packetizer packetizer_tx ;  
TelemetryPayloadDummy payload_dummy ; 

//abstract this function to just be "Clock"

//To easily convert any payload data from string to bytes since we eventually want the data to be bytes std::vector<uint8_t> vec - 
std::vector<uint8_t> to_bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

//This function simulates sending a frame by creating a buffer of 255 bytes to simulate a LoRa buffer, and then simulates bit flips / errors to simulate sending
//Over the air - 
void simulated_send_over_channel () {

}

void simulated_receive_over_channel() {

}

void SendCommandACK() {

}


void TelemetryStep() {
    //Getting the last time we requested for telemetry to be sent -  
    //Sending telemetry data - 
    std::chrono::steady_clock::time_point  current_time = Clock::now();

    if (current_time-last_telemetry >= std::chrono::seconds(10)) {
        // send telemetry
        //Convert from string to bytes (temporary fix)
        temp_payload = to_bytes(payload_dummy.dummy_Data) ; 
        //Append sequence number - 
        packetizer_tx.telemetry.sequence_Number++ ; 
        //Set record type (1 is for telemetry) - 
        packetizer_tx.telemetry.record_Type = 1 ; 
        //Encrypt and encode - 
        if (!packetizer_tx.encode_and_encrypt(temp_payload, key, DIR_DOWNLINK)) return ;  // I changed DIR_UPLINK to DIR_DOWNLINK since this is balloon side sending data downlink
        simulated_send_over_channel() ; 
        last_telemetry = current_time;
    }
}


bool ReceiveCommandStep () {
    //Pseudocode basically but I'll get back to this - 
    if (simulated_receive_over_channel==nullptr) {
        return false ; 
    } else {
        //Since this is a packet from ground we should use the opposite direction - 
        if (!packetizer_rx.Decode(temp_payload, DIR_UPLINK, key)) return false ; 
    }

    // Our packetizer function already does integrity checks so should automatically drop the packet upon detection of bit flips - 
    //Same with length checks and this also feeds into the fact that if there were length changes it would fail ythe integrity check anyways - we also do length checks during 
    // ... encoding so that base is covered too -  

    //skipping command checks for whether it's valid or invalid since we don't have any actual commands to test with - 
    // Assumption is that teammate will cover the ground side here - 

    if (packetizer_rx.telemetry.protocol_Version != 1) return false;

    if (packetizer_rx.telemetry.sync0!=67) {
        return false ; 
    } 

    if (packetizer_rx.telemetry.sync1!=8) {
        return false ; 
    } 

    //pseudocode-ish since we don't have any actual data yet - 
    //keep these empty fields they'll be used later - 
    //1 is for payload data , 4 is for ACK's and 6 is for commands -
    if (packetizer_rx.telemetry.record_Type==1) {} else if (packetizer_rx.telemetry.record_Type==4){
    } else if (packetizer_rx.telemetry.record_Type==6) {
        if (packetizer_rx.telemetry.payload_buffer.size() < 2) return false;
        //If record type 6 do this action - 
        //Example commands - prints out in the console since we don't have any hardware to forward commands to - 
        //Using SunByte's model for recognizing commands - 
        if (packetizer_rx.telemetry.payload_buffer[0]==0xa2 && packetizer_rx.telemetry.payload_buffer[1]==0x08) {
            std::cout << "Recalibrating the telescope" << std::endl ;  
        } else if (packetizer_rx.telemetry.payload_buffer[0]==0xa3 && packetizer_rx.telemetry.payload_buffer[1]==0x09) {
            std::cout << "Prepare for shutdown" << std::endl ;  
        }  else if (packetizer_rx.telemetry.payload_buffer[0]==0xa4 && packetizer_rx.telemetry.payload_buffer[1]==0x10) {
            std::cout << "Reset the stepper motor" << std::endl ;  
        }   else  {
            std::cout << "Invalid Command" << std::endl ;
            //here we need to send an ACK back down now - 
            SendCommandACK() ; 
        }    
    } else {
        return false ; 
    }

}

int main() {

    //Standard-check if libsodium initializes correctly-
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl; 
        return 1;
    }

    size_t key_size = key.size() ; 

    //This entire segment is for finding the key, opening, reading it , and then using it for packetizing and encryption/ decryption. 
    std::ifstream input( "../xchacha_keygen.bin", std::ios::binary );
        
    if (input.is_open()) {
            input.seekg(0, std::ios::end);
            std::streamsize fsize = input.tellg();
            input.seekg(0, std::ios::beg);
            if (fsize < 0) {
                std::cerr << "tellg() failed\n";
                return 1;
            }
            if(fsize!=key_size) {
                std::cerr<< "Key buffer isn't equal to the key, exiting the program, please check xchacha_keygen.bin \n" << std::endl ; 
                return 1 ; 
            }
            std::vector<uint8_t> vec(fsize);
            input.read(reinterpret_cast<char *>(std::data(vec)), fsize);
            key = vec ; 
    } else {
            std::cerr <<"ERROR"<< std::endl;
            return 1 ; 
    }
    
    input.close();
    //Done with file i/o for the key - Start the main loop now - 

    while(true) {

        ReceiveCommandStep() ; 

        TelemetryStep() ; 

        //Sleep for 10ms - 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));


    }    

    return 0 ; 

}