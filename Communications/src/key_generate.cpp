
//FUNCTION : This C++ file's job is to standardize key management and store it somewhere that can be accessed.   

#include <vector>  
#include <iostream>
#include <cstdint>
#include <fstream>
#include <filesystem>

#include <sodium.h>

namespace fs = std::filesystem;

int main() {

    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium" << std::endl; 
        return 1;
    }

    if (fs::exists("xchacha_keygen.bin")) {
        std::cerr << "Key file already exists \n";
        return 1;
    }
    
    std::vector<uint8_t> key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);

    crypto_aead_xchacha20poly1305_ietf_keygen(key.data());

    std::cout << "Key generated successfully\n";

    std::ofstream out;
    out.open("xchacha_keygen.bin", std::ios::binary);
    
    if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(key.data()), key.size());
    } else {
        std::cout <<"ERROR"<< std::endl;
        return 1 ; 
    }

    if (!out.good()) {
        std::cerr << "Write failed \n";
        return 1;
    }
 
    out.close();

    return 0 ; 
} 