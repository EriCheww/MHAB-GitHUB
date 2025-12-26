#ifndef PACKETISER_H
#define PACKETISER_H


//FUNCTION : This headers job is to input data from the main code and then packetize the payload, and serialize it. This header uses packet.hpp as the packet structure blueprint.  

#include <cstdint> //for all the uintx_t
#include <vector>  //for std::vector 
#include <cstddef> // for size_t 
#include <cstring> // for memcpy

#include <sodium.h> // The library we're gonna be using for encryption + authenthication

#include "packet.hpp"


class Packetizer { 
    private : 
        //Hidden attributes that the outside can't see (encapsulation)
        static constexpr size_t MAX_BUFFER = 255 ; //size_t is 64 bits aka 8 bytes.  
    public : 
    // Meant to run the function / action when the object is created based on the attributes we defined -    
    //The constructor here is also meant to initialize the packet features we defined in the private class to a safe state.   
    Packetizer() {
        //everything that defines the packet will be appended in the method
    } 

    //DEFINITIONS - 
    std::vector<uint8_t> packet; 
    std::vector<uint8_t> header;   
    TelemetryPacketStructure telemetry ; 
    
    //255-35 = 220 bytes for the payload even though in practice all that extra space won't be used. 

    //ENCODING - since we're starting with a big integer already in a big container, e.g. uint64_t x we can split x into 8 bytes and push them into the vector.

    // Conversion to Little endian, and copying the data to the packet - 
    // A bit tedious but this is easier to understand and also done byte-wise- we're encoding into little-endian so format is explicitly defined. 
    

    //works by appending bits to the lsb of (leftmost) portion of the frame and then we append the rest of the bytes by shifting it depending on the data type - 
    bool append_bytes(std::vector<uint8_t>& packet, const std::vector<uint8_t>& bytes_headers) {
        
        if (packet.size()+bytes_headers.size()>(MAX_BUFFER)) {
            return false ; 
        } else {
            packet.insert(packet.end(), bytes_headers.begin(), bytes_headers.end());
        }

        return true ; 
        
    }

    bool append_16_bit(std::vector<uint8_t>& packet, uint16_t headers) {
        if (packet.size() + 2 > MAX_BUFFER) return false;
        packet.push_back(static_cast<uint8_t>( headers& 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>8) & 0xFF));
        return true ; 
    }

    bool append_8_bit(std::vector<uint8_t>& packet, uint8_t headers) {
        if (packet.size() + 1 > MAX_BUFFER) return false;
        packet.push_back(static_cast<uint8_t>(headers& 0xFF));
        return true ; 
    }

    bool append_32_bit(std::vector<uint8_t>& packet, uint32_t headers) {
        if (packet.size() + 4 > MAX_BUFFER) return false;
        packet.push_back(static_cast<uint8_t>( headers& 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>8) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>24) & 0xFF));
        return true ; 
    }

    bool append_64_bit(std::vector<uint8_t>& packet, uint64_t headers) {
        if (packet.size() + 8 > MAX_BUFFER) return false;
        packet.push_back(static_cast<uint8_t>( headers& 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>8) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>24) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>32) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>40) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>48) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>56) & 0xFF));
        return true ; 
    }

    //A function made specially for appending the sequence number to the nonce along with the direction bit 
    void derive_append_nonce(std::vector<uint8_t>& nonce, uint64_t sequence_number, uint8_t dir) {
        
        nonce[0] = dir ; 
        int counter = 0 ; 

        for(int i=0; i<sizeof(sequence_number); i++) {
            nonce[i+1] = ((sequence_number>>counter) & 0xFF) ; 
            counter = counter + 8 ; 
        }
    }


    // note : the payload is passed by value (copied). Encode() only reads it, therefore that's ok - however in future, if we need to be able to modify the variable - 
    //it should use void Encode(std::vector<uint8_t>& payload) 
    bool encode_and_encrypt(const std::vector<uint8_t>& payload, const std::vector<uint8_t> &key, uint8_t dir) { 
        //We'll load the key from a file in main.cpp and then use that key as the input for encode and encrypt - 
        //clear the entire holding packet before re-encoding - 
        packet.clear();
        header.clear() ; 
        packet.reserve(255) ; 
        header.reserve(35);
        size_t payload_size = payload.size() ; 

        //Key handling and nonce derivation here - 
        std::vector<uint8_t> Nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) ; 
        derive_append_nonce(Nonce, telemetry.sequence_Number, dir) ; 

        telemetry.tag_auth.resize(crypto_aead_xchacha20poly1305_ietf_ABYTES); // 16 

        if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) return false;
        if (Nonce.size() != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) return false;

        //Define more sizes  - 
        const size_t tag_len = crypto_aead_xchacha20poly1305_ietf_ABYTES;


        telemetry.payload_buffer.resize(payload_size) ; 

        telemetry.payload_Length = static_cast<uint8_t>(payload_size);

        //APPENDING TO HEADER FIELD AND THE MAIN PACKET (The header field is used for AAD)

        if (!append_8_bit(packet,telemetry.sync0)) return false ; // two bytes will be validated separately to prevent any accidents 
        if (!append_8_bit(header,telemetry.sync0)) return false ;

        if (!append_8_bit(packet,telemetry.sync1)) return false ;
        if (!append_8_bit(header,telemetry.sync1)) return false ;

        if (!append_8_bit(packet,telemetry.protocol_Version)) return false; 
        if (!append_8_bit(header,telemetry.protocol_Version)) return false ;

        if (!append_8_bit(packet, telemetry.record_Type)) return false ; // important since we need a rule for handling different data types
        //for example maybe we can do record_type = 0x01 for "GPS" and so on and then record_Type = 0x02 For "ACK", etc. 
        //And then maybe we can define byte's to determine the data type (string, uint?) for example 0x21 means 2 is a string and 1 is for telescope. 
        if (!append_8_bit(header,telemetry.record_Type)) return false ;

        if (!append_8_bit(packet, telemetry.source_Id))return false ; 
        if (!append_8_bit(header,telemetry.source_Id))return false ;

        if (!append_8_bit(packet, telemetry.destination_Id)) return false ;
        if (!append_8_bit(header,telemetry.destination_Id)) return false ;

        if (!append_64_bit(packet, telemetry.sequence_Number)) return false ;  
        if (!append_64_bit(header,telemetry.sequence_Number)) return false ;

        if (!append_32_bit(packet, telemetry.unix_Timestamp)) return false ;
        if (!append_32_bit(header,telemetry.unix_Timestamp)) return false  ;

        
        //Using plaintext length assuming that plaintext length = encrypted_length 
        if (!append_8_bit(packet, static_cast<uint8_t>(payload_size))) return false ; //cast to uint8_t 
        if (!append_8_bit(header, static_cast<uint8_t>(payload_size))) return false ; 

        size_t header_size = header.size() ; 

        if (payload_size+header_size+tag_len>MAX_BUFFER) return false ; 

        //ENCRYPTION + AUTHENTHICATION FROM LIBSODIUM USING XCHACHA20-POLY1305 - 

        int ret = crypto_aead_xchacha20poly1305_ietf_encrypt_detached(telemetry.payload_buffer.data(), telemetry.tag_auth.data(), nullptr , payload.data(),payload.size(),header.data(), header.size(), nullptr,Nonce.data(),key.data());

        if (ret!= 0) {
            return false;
        }

        size_t encrypted_payload_size =  telemetry.payload_buffer.size() ; 

        if (encrypted_payload_size+header_size>(MAX_BUFFER)) {
            return false ; 
        } else  {
            //using the struct as a blueprint. 
            if(!append_bytes(packet, telemetry.payload_buffer)) return false ;
            if (!append_bytes(packet, telemetry.tag_auth)) return false  ; 
        }

        return true ; 


    }

    //DECODE PACKET LOGIC HERE - 
    // This time we start with bytes in a vector, e.g. frame[offset + k] which is uint8_t.
    // Goal: rebuild the big integer by placing each byte into the correct position.
    //start with bytes in the frame vector and then we'll move along using an offset
    // For multi-byte fields, REBUILD the integer by reading bytes at frame[offset+k], casting to the destination width
    // OR works because each shifted byte takes up a different slot in the frame function once we've casted it .... 

    bool decode_8_bit(const std::vector<uint8_t>& frame, size_t& offset, uint8_t &header) {
        if (offset + 1 > frame.size()) {
            return false ;
        }            
        header = static_cast<uint8_t>(frame[offset]) ;
        offset = offset + 1;
        return true ; 
    }

    bool decode_16_bit(const std::vector<uint8_t>& frame, size_t& offset, uint16_t &header) {
        if (offset + 2 > frame.size()) {
            return false ;
        }       
        header = static_cast<uint16_t>(frame[offset]) | (static_cast<uint16_t>(frame[offset + 1]) << 8);
        offset = offset + 2;
        return true ; 
    }

    bool decode_32_bit(const std::vector<uint8_t>& frame, size_t& offset, uint32_t &header) {
        if (offset + 4 > frame.size()) {
            return false ;
        }       
        header = static_cast<uint32_t>(frame[offset]) | (static_cast<uint32_t>(frame[offset + 1]) << 8) | (static_cast<uint32_t>(frame[offset+2])<<16) | (static_cast<uint32_t>(frame[offset+3])<<24); 
        offset = offset + 4 ;
        return true ; 
    }

    bool decode_64_bit(const std::vector<uint8_t>& frame, size_t& offset, uint64_t &header) {
        if (offset + 8 > frame.size()) {
            return false ;
        }       
        header = static_cast<uint64_t>(frame[offset]) | (static_cast<uint64_t>(frame[offset + 1]) << 8) | (static_cast<uint64_t>(frame[offset+2])<<16) | (static_cast<uint64_t>(frame[offset+3])<<24)| (static_cast<uint64_t>(frame[offset+4])<<32)| (static_cast<uint64_t>(frame[offset+5])<<40)| (static_cast<uint64_t>(frame[offset+6])<<48)| (static_cast<uint64_t>(frame[offset+7])<<56) ;
        offset = offset + 8 ;
        return true ; 
    }

    bool decode_bytes(const std::vector<uint8_t>& frame, size_t& offset, std::vector<uint8_t> &data, uint8_t payload_Length_) {
        const size_t n = static_cast<size_t>(payload_Length_) ; 
        if (offset + n > frame.size()) {
            return false ;
        }       
        data.assign(frame.begin()+offset, frame.begin()+offset+ n ) ; 
        offset = offset + n ; 
        return true ; 
    }

    bool Decode(const std::vector<uint8_t>& frame, uint8_t dir, const std::vector<uint8_t> &key) {  // use the real values by doing const & not their copies. 
        //a packet has entered the wire from air - now we must decode it - 
        size_t offset = 0 ; 
        std::vector<uint8_t> ciphertext ;  
        static const size_t tag_length = 16 ; 

        // Size check of the frame to ensure that if the size is incorrect it will auto-reject the corrupted portion to prevent any errors further down the line -
        //Using a bool to return either true or false  

        //Decoding to struct and header fields - 
        if (!decode_8_bit(frame, offset, telemetry.sync0)) {
            return false;
        }

        if (!decode_8_bit(frame, offset, telemetry.sync1)) {
            return false;
        }

        if (!decode_8_bit(frame, offset, telemetry.protocol_Version)) {
            return false ; 
        }

        if (!decode_8_bit(frame, offset, telemetry.record_Type)) {
            return false ; 
        }

        if (!decode_8_bit(frame, offset, telemetry.source_Id)) {
            return false ; 
        }

        if (!decode_8_bit(frame, offset, telemetry.destination_Id)){
            return false ; 
        }

        if (!decode_64_bit(frame, offset, telemetry.sequence_Number)){
            return false ; 
        }

        if (!decode_32_bit(frame,offset, telemetry.unix_Timestamp )) {
            return false ; 
        } 

        if (!decode_8_bit(frame, offset, telemetry.payload_Length)){
            return false ; 
        }
        
        //Key handling and nonce derivation here - 
        std::vector<uint8_t> Nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) ; 
        derive_append_nonce(Nonce, telemetry.sequence_Number, dir) ; 

        telemetry.tag_auth.resize(crypto_aead_xchacha20poly1305_ietf_ABYTES); // 16 

        if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) return false;
        if (Nonce.size() != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) return false;


        size_t header_end = offset;

        //Temporary header used to input into the decrypt function since we want to take a slice of the entire frame (just the header data)
        std::vector<uint8_t> temp_header(frame.begin(), frame.begin() + header_end);

        if (!decode_bytes(frame, offset, ciphertext, telemetry.payload_Length)) {
            return false ; 
        }

        if (!decode_bytes(frame,offset, telemetry.tag_auth, tag_length)) {
            return false ; 
        } 

        //DECRYPTION AND AUTHENTICATE - 
        
        telemetry.payload_buffer.resize(telemetry.payload_Length) ; 


        int ret = crypto_aead_xchacha20poly1305_ietf_decrypt_detached(telemetry.payload_buffer.data(), nullptr, ciphertext.data() , 
            telemetry.payload_Length,telemetry.tag_auth.data(),temp_header.data(), temp_header.size(), Nonce.data(),key.data()
        );

        // Used in libsodium, used to check if == 0 then it's authenticated otherwise drop the message if -1. 
        if (ret!= 0) {
            return false;
        }

        if (telemetry.sync0!=67) {
            return false ; 
        } 

        if (telemetry.sync1!=8) {
            return false ; 
        } 

        if (telemetry.protocol_Version!=1) {
            return false ; 
        } 

        //pseudocode-ish since we don't have any actual data yet - 
        //keep these empty fields they'll be used later - 
        if (telemetry.record_Type==1) {} else if (telemetry.record_Type==4){} else if (telemetry.record_Type==6) {} else {
            return false ; 
        }

        return true ; 


    } 

    // Deriving the nonce using the sequence number deterministically but still safely - 
    //The direction bit will be given to the balloon and ground side specially for this, so that nonce, key pair can never repeat - 

} ; 

#endif