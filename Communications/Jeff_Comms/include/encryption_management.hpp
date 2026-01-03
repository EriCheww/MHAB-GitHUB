#ifndef PAYLOAD_DATA_H
#define PAYLOAD_DATA_H


//FUNCTION : This headers job is to standardize the payload data we're gonna be accessing -  

#include <cstdint>
#include <vector>  
#include <cstddef> 


struct PayloadData { 
    uint8_t sync0 = 67 ; 
    uint8_t sync1 = 8 ; 
    uint8_t protocol_Version=1 ;
    uint8_t record_Type=0 ; 
    uint8_t source_Id = 0 ; 
    uint8_t destination_Id  = 0 ; 
    uint64_t sequence_Number = 0   ; 
    uint32_t unix_Timestamp=0  ; 
    uint8_t payload_Length = 0 ; 
    std::vector<uint8_t>payload_buffer ;
    std::vector<uint8_t>tag_auth  ;  

} ; 

#endif