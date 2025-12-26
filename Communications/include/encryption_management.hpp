#ifndef ENCRYPTION_MANAGEMENT_H
#define ENCRYPTION_MANAGEMENT_H


//FUNCTION : This headers job is to standardize key management and nonce derivation.  

#include <cstdint>
#include <vector>  
#include <cstddef> 
#include <cstring>

#include <sodium.h>

#include "packet.hpp"
#include "packetizer.hpp"


class EncryptionManagement { 
    private : 
        std::vector<uint8_t> secret_key;
    public : 

    EncryptionManagement() {
        //everything that defines the packet will be appended in the method
    } 

    //DEFINITIONS - 
  
    bool encode_and_encrypt(const std::vector<uint8_t>& payload) { 
        
        return true ; 


    }


} ; 

#endif