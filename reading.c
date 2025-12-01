#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"

const char* username;

#define MESSAGE_LEN 2048

// Helper function to all the required bytes
size_t read_helper(int fd, void* buf, size_t len) {
  // Bytes read so far
  size_t bytes_read = 0; 
  
  // Keep reading until the end
  while (bytes_read < len) {
    // Try to read the entire remaining message
    ssize_t rc2 = read(fd, buf + bytes_read, len - bytes_read);
    // Catch error
    if (rc2 < 0) return rc2;
    // Update bytes read so far
    bytes_read += rc2;
  }
  
  // All bytes are read
  return bytes_read;
}

