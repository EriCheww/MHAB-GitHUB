#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <vector> 
#include <cstddef>

//FUNCTION : This header is a template for the packet structure - it serves so functional purpose. 

// Functions are a block of code that executes a job 
// CLasses are objects and the literal object 
//CR of 4/5, BW of 125 kHz, SF of 12. 

struct TelemetryPacketStructure { 
    uint8_t sync0 = 67 ; //Used to inform when the packet starts. 
    uint8_t sync1 = 8 ; 
    uint8_t protocol_Version=1 ; //This is used to tell the receiver to interpret the bytes that follow this specific packet layout and rules.
    uint8_t record_Type=0 ; //1 is for payload data , 4 is for ACK's and 6 is for commands  -
    uint8_t source_Id = 0 ; 
    uint8_t destination_Id  = 0 ; 
    uint64_t sequence_Number = 0   ; 
    uint32_t unix_Timestamp=0  ; 
    uint8_t payload_Length = 0 ; 
    std::vector<uint8_t>payload_buffer ;
    std::vector<uint8_t>tag_auth  ;  

} ; 

#endif