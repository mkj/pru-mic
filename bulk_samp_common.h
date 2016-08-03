/* Shared by Linux and PRU code */

#ifndef BULK_SAMP_H
#define BULK_SAMP_H

#define RPMSG_CHAN_NAME                 "bulksamp-pru"

// message types
// cpu to device
#define BULK_SAMP_MSG_BUFFERS 1
#define BULK_SAMP_MSG_START 2
#define BULK_SAMP_MSG_STOP 3
// XXX pin setup? rising/falling trigger?
// device to cpu
#define BULK_SAMP_MSG_READY 20
// debug
#define BULK_SAMP_MSG_CONFIRM 40
#define BULK_SAMP_MSG_DEBUG 41

#define BULK_SAMP_MAX_NUM_BUFFERS 30

struct 
__attribute__((__packed__))
bulk_samp_msg_buffers {
    uint8_t type;
    uint32_t buffer_count;
    uint32_t buffer_size;
    uint32_t buffers[BULK_SAMP_MAX_NUM_BUFFERS];
};

struct 
__attribute__((__packed__))
bulk_samp_msg_confirm {
    uint8_t type;
    uint8_t confirm_type;
};

// start sampling continuously
struct 
__attribute__((__packed__))
bulk_samp_msg_start {
    uint8_t type;
    uint32_t clock_hz;
};

struct 
__attribute__((__packed__))
bulk_samp_msg_stop {
    uint8_t type;
};

// Sent when the buffer is full/finished
struct 
__attribute__((__packed__))
bulk_samp_msg_ready {
    uint8_t type;
    uint8_t buffer_index;
    uint32_t size; // XXX needed? probably always just buffer_size
    uint32_t checksum; // checksum including magic
    uint32_t magic; // written to start/end of buffer? or just included in checksum?
};

struct 
__attribute__((__packed__))
bulk_samp_msg_debug {
    uint8_t type;
    char str1[32];
    char str2[32];
    uint32_t num1;
    uint32_t num2;
    uint32_t num3;
};

#endif // BULK_SAMP_H
