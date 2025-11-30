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

// Thread to read both username and message. Recieve lengths first and then contents
void* peer_read_thread(void* arg) {
  intptr_t peer_fd = (intptr_t) arg;

  // Keep reading information from this peer
  while(1) {

    // Reading the username's length 
    size_t username_len;
    if (read(peer_fd, &username_len, sizeof(size_t)) != sizeof(size_t)) {
      break; // Stop reading if there's an error
    }

    // Check if size is appropriate 
    if (username_len > MESSAGE_LEN) break;

    // Allocate memory for the username
    char *username = malloc(username_len+1);
    username[username_len] = '\0';

    // Read the username
    read_helper(peer_fd, username, username_len);

    // Get message length
    size_t message_len;
    read(peer_fd, &message_len, sizeof(size_t));

    // Check size
    if (message_len > MESSAGE_LEN) break;
    char *message = malloc(message_len+1);
    message[message_len] = '\0';

    // Read the full message
    read_helper(peer_fd, message, message_len);
    
    ui_display(username, message);

    /* Broadcast to all other peers */

    free(username);
    free(message);
  }
  return NULL;
}