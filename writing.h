#if !defined(WRITING_H)
#define WRITING_H

#include <sys/socket.h>

// Helper function to write all the required bytes
ssize_t write_helper(int fd, char* buf, size_t len);

#endif
