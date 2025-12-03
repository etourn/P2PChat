#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"
#include "p2pchat.h"
#include "reading.h"

#define MESSAGE_LEN 2048
#define CAPACITY 1000

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
  peer* p = (peer*) arg;
  intptr_t peer_fd = p->peer_fd;
  char** seen = p->seen;

  // Keep reading information from this peer
  while(1) {

    // Read the message id
    size_t milen;
    if (read(peer_fd, &milen, sizeof(size_t)) != sizeof(size_t)) {
      break; // Stop reading if there's an error
    }
    // Allocate memory for the message id
    char *message_id = malloc(milen+1);
    message_id[milen] = '\0';

    // Read the message_id
    read_helper(peer_fd, message_id, milen);

    // Flag for if i should display/broadcast
    int flag = 1;
    // Check if in set
    int i = 0;
    while (seen[i] != NULL){
      if (strcmp(seen[i], message_id) == 0) {
        flag = 0;
        break;
      }
      i++;
    }

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
    
    // check flag
    if (flag) {
      ui_display(username, message);
      /* Broadcast to all other peers */
      broadcast(username, message, message_id);

      // store message id
      // find a free slot and store
      for (int i = 0; i < CAPACITY; i++) {
          if (seen[i] == NULL) {
              seen[i] = strdup(message_id);  
              break;
          }
      }
    }

    free(username);
    free(message);
    free(message_id);
  }
  return NULL;
}