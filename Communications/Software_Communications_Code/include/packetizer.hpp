#ifndef PACKETISER_H
#define PACKETISER_H


//FUNCTION : This headers job is to input data from the main code and then packetize the payload, and serialize it. This header also defines the packet structure. 

#include <cstdint>
#include <vector> 
#include <cstddef>
#include <iostream>
#include <cstring>

#include "packet.hpp"

class Packetizer { 
    private : 
        //Hidden attributes that the outside can't see (encapsulation)
        static constexpr size_t MAX_BUFFER = 255 ; //size_t is 64 bits aka 8 bytes.  
        static constexpr size_t HEADER_SIZE = 19 ; 
    public : 
    // Meant to run the function / action when the object is created based on the attributes we defined -    
    //The constructor here is also meant to initialize the packet features we defined in the private class to a safe state.   
    Packetizer() {
        //everything that defines the packet will be appended in the method
    } 

    std::vector<uint8_t> packet;  
    TelemetryPacketStructure telemetry ; 
    //255-35 = 220 bytes for the payload even though in practice all that extra space won't be used. 


    //ENCODING - since we're starting with a big integer already in a big container, e.g. uint64_t x we can split x into 8 bytes and push them into the vector.

    // Conversion to Little endian, and copying the data to the packet - 
    // A bit tedious but this is easier to understand and also done byte-wise- we're encoding into little-endian so format is explicitly defined. 
    

    //works by appending bits to the lsb of (leftmost) portion of the frame and then we append the rest of the bytes by shifting it depending on the data type - 
    void append_bytes(std::vector<uint8_t>& packet, const std::vector<uint8_t>& bytes_headers) {
        packet.insert(packet.end(), bytes_headers.begin(), bytes_headers.end());
    }

    void append_16_bit(std::vector<uint8_t>& packet, uint16_t headers) {
        packet.push_back(static_cast<uint8_t>( headers& 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>8) & 0xFF));
    }

    void append_8_bit(std::vector<uint8_t>& packet, uint8_t headers) {
        packet.push_back(static_cast<uint8_t>(headers& 0xFF));
    }

    void append_32_bit(std::vector<uint8_t>& packet, uint32_t headers) {
        packet.push_back(static_cast<uint8_t>( headers& 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>8) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>24) & 0xFF));
    }

    void append_64_bit(std::vector<uint8_t>& packet, uint64_t headers) {
        packet.push_back(static_cast<uint8_t>( headers& 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>8) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>24) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>32) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>40) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>48) & 0xFF));
        packet.push_back(static_cast<uint8_t>((headers>>56) & 0xFF));
    }

    // note : the payload is passed by value (copied). Encode() only reads it, therefore that's ok - however in future, if we need to be able to modify the variable - 
    //it should use void Encode(std::vector<uint8_t>& payload) 
    void Encode(const std::vector<uint8_t>& payload) { 
        //clear the entire packet before re-encoding - 
        packet.clear();
        packet.reserve(255) ; 
        size_t payload_size = payload.size() ;   

        if (payload_size>(MAX_BUFFER-HEADER_SIZE)) {
            std::cerr << "The payload  size exceeds the buffer limit" << std::endl;
            return; 
        } else if (payload_size<=MAX_BUFFER-HEADER_SIZE) {
            //using the struct as a blueprint. 
            append_16_bit(packet,telemetry.sync) ;
            append_8_bit(packet,telemetry.protocol_Version) ; 
            append_8_bit(packet, telemetry.record_Type) ; // important since we need a rule for handling different data types
            //for example maybe we can do record_type = 0x01 for "GPS" and so on and then record_Type = 0x02 For "ACK", etc. 
            //And then maybe we can define byte's to determine the data type (string, uint?) for example 0x21 means 2 is a string and 1 is for telescope. 
            append_8_bit(packet, telemetry.source_Id) ; 
            append_8_bit(packet, telemetry.destination_Id) ;
            append_64_bit(packet, telemetry.sequence_Number) ;  
            append_8_bit(packet, static_cast<uint8_t>(payload_size)) ; //cast to uint8_t 
            append_32_bit(packet, telemetry.unix_Timestamp) ;
            append_bytes(packet, payload) ;
        }

    }

    //DECODE PACKET LOGIC HERE - 
    // This time we start with bytes in a vector, e.g. frame[offset + k] which is uint8_t.
    // Goal: rebuild the big integer by placing each byte into the correct position.
    //start with bytes in the frame vector and then we'll move along using an offset
    // For multi-byte fields, REBUILD the integer by reading bytes at frame[offset+k], casting to the destination width
    // OR works because each shifted byte takes up a different slot in the frame function once we've casted it .... 

    void decode_8_bit(const std::vector<uint8_t>& frame, size_t& offset, uint8_t &header) {
        header = static_cast<uint8_t>(frame[offset]) ;
        offset = offset + 1;
    }

    void decode_16_bit(const std::vector<uint8_t>& frame, size_t& offset, uint16_t &header) {
        header = static_cast<uint16_t>(frame[offset]) | (static_cast<uint16_t>(frame[offset + 1]) << 8);
        offset = offset + 2;
    }

    void decode_32_bit(const std::vector<uint8_t>& frame, size_t& offset, uint32_t &header) {
        header = static_cast<uint32_t>(frame[offset]) | (static_cast<uint32_t>(frame[offset + 1]) << 8) | (static_cast<uint32_t>(frame[offset+2])<<16) | (static_cast<uint32_t>(frame[offset+3])<<24); 
        offset = offset + 4 ;
    }

    void decode_64_bit(const std::vector<uint8_t>& frame, size_t& offset, uint64_t &header) {
        header = static_cast<uint64_t>(frame[offset]) | (static_cast<uint64_t>(frame[offset + 1]) << 8) | (static_cast<uint64_t>(frame[offset+2])<<16) | (static_cast<uint64_t>(frame[offset+3])<<24)| (static_cast<uint64_t>(frame[offset+4])<<32)| (static_cast<uint64_t>(frame[offset+5])<<40)| (static_cast<uint64_t>(frame[offset+6])<<48)| (static_cast<uint64_t>(frame[offset+7])<<56) ;
        offset = offset + 8 ;
    }

    void decode_bytes(const std::vector<uint8_t>& frame, size_t& offset, std::vector<uint8_t> &data, uint8_t payload_Length_) {
        const size_t n = static_cast<size_t>(payload_Length_) ; 
        data.assign(frame.begin()+offset, frame.begin()+offset+ n ) ; 
        offset = offset + n ; 
    }

    void Decode(const std::vector<uint8_t>& frame) { 
        //a packet has entered the wire from air - now we must decode it - 
        size_t offset = 0 ; 
        size_t payload_Size = 0 ; 

        decode_16_bit(frame, offset, telemetry.sync);
        decode_8_bit(frame, offset, telemetry.protocol_Version);
        decode_8_bit(frame, offset, telemetry.record_Type);
        decode_8_bit(frame, offset, telemetry.source_Id);
        decode_8_bit(frame, offset, telemetry.destination_Id);
        decode_64_bit(frame, offset, telemetry.sequence_Number);
        decode_8_bit(frame, offset, telemetry.payload_Length) ; 
        decode_32_bit(frame,offset, telemetry.unix_Timestamp ) ; 

        decode_bytes(frame, offset, telemetry.payload, telemetry.payload_Length) ;  

        if (telemetry.sync!=67) {
            return ; 
        } else if (telemetry.sync==67) {
            std::cout << "Sync number successfully received : " << telemetry.sync << std::endl ;  
        }

        if (telemetry.protocol_Version!=1) {
            return ; 
        } else if (telemetry.protocol_Version==1) {
            std::cout << "Protocol version : " << telemetry.protocol_Version << std::endl ;  
        }

        //pseudocode-ish since we don't have any actual data yet - 
        if (telemetry.record_Type==1) {
            std::cout << "ACK" << telemetry.record_Type << std::endl;   
        } else if (telemetry.record_Type==4){
            std ::cout << "Dummy data received" << std::endl ; 
        } else if (telemetry.record_Type==6) {
            std::cout << " GPS coordinates received : " << telemetry.record_Type << std::endl ;  
        } else {
            std::cout << "Invalid Record_Type" << std::endl ; 
            return ; 
        }




    }


} ; 

#endif