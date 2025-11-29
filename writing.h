#if !defined(WRITING_H)
#define WRITING_H

#include <sys/socket.h>

// Helper function to write all the required bytes
ssize_t write_helper(int fd, char* buf, size_t len) {
  size_t bytes_written = 0;
  char* ptr = buf;

  // write every element in the buffer
  while (bytes_written < len) {
    ssize_t rc = write(fd, ptr + bytes_written, len - bytes_written);
    if (rc < 0) return rc;
    bytes_written += (size_t) rc;
  }
  return (ssize_t) bytes_written;
}

#endif

