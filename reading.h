#if !defined(READING_H)
#define READING_H

#include <sys/socket.h>

// Helper function to read all the required bytes
size_t read_helper(int fd, void* buf, size_t len);

// Thread to read both username and message. Receive lengths first and then contents
void* peer_read_thread(void* arg);

// Struct to pass into peer_read_thread
typedef struct {
    intptr_t peer_fd;
    char** seen;
} peer;

#endif
