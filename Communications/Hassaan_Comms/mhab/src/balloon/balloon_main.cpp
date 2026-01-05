


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
#include "../../include/link_layer.hpp"

namespace fs = std::filesystem;

int main() {

    //Standard-check if libsodium initializes correctly-
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl; 
        return 1;
    }

    std::vector<uint8_t> key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES); 
    //Using size for comparing to file to see if there is a match (if not reject)
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

    //Passing key as the class input since we use the key as the input for packetizing - 
    LinkLayer link_layer(key);

    while(true) {

        link_layer.ReceiveCommandStep() ; 

        link_layer.TelemetryStep() ; 

        //Sleep for 10ms - 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));


    }    

    return 0 ; 

}