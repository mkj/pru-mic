/* Shared by Linux and PRU code */

#ifndef BULK_SAMP_H
#define BULK_SAMP_H

#define BULK_SAMP_NUM_BUFFERS          (2)
#define BULK_SAMP_BUFFER_SIZE          (4*1024*1024)

// message types
// cpu to device
#define BULK_SAMP_MSG_BUFFERS 1
#define BULK_SAMP_MSG_START 2
#define BULK_SAMP_MSG_STOP 3
// XXX pin setup? rising/falling trigger?
// device to cpu
#define BULK_SAMP_MSG_DATA 20

struct bulk_samp_msg_buffers {
    char type;
    uint32_t buffers[BULK_SAMP_NUM_BUFFERS];
};

// start sampling continuously
struct bulk_samp_msg_start {
    char type;
    uint32_t clock_hz;
};

struct bulk_samp_msg_stop {
    char type;
};

// Sent when the buffer is full/finished
struct bulk_samp_msg_ready {
    char type;
    char buffer_index;
    uint32_t size; // XXX needed? probably always just buffer_size
    uint32_t checksum; // checksum including magic
    uint32_t magic; // written to start/end of buffer? or just included in checksum?
};

#endif // BULK_SAMP_H
