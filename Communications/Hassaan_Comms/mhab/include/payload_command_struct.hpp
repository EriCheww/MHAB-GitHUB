#ifndef SEQUENCE_COMMAND_H
#define SEQUENCE_COMMAND_H

#include <cstdint>
#include <vector> 
#include <cstddef>
#include <cstring>

//FUNCTION : This header is for the payload structure for the special case when a command is sent uplink it should use this struct - 

enum class AckStatus : uint8_t { OK = 0, ERROR = 1 };

struct AckPayload {
    uint64_t cmd_seq = 0 ; //match command payload width of the seq
    AckStatus status; 
};


struct CommandPayload {
    uint16_t cmd_id = 0 ; // e.g. 0xA208
    // This can be either "OK" or "ERROR"
    uint64_t cmd_seq = 0 ; // from byte 0-7 in the payload we define a separate sequence number specifically for tracking 
    // the command id - this is so that we indirectly maintain nonce uniqueness while appending the frame sequence number, 
    // This is a special cmd_seq for specifically ACK's and duplication logic
};


#endif