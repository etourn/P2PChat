#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "ui.h"

// Keep the username in a global so we can access it from the callback
const char* username;

#define CAPACITY 1000
#define MESSAGE_LEN 2048
pthread_mutex_t peers_lock = PTHREAD_MUTEX_INITIALIZER;

// Data structure to store all peers
typedef struct{
  int unique_fd;
  // char* username;
}peer_identifier_t;

int peers[CAPACITY];
int num_peers=0;

// TODO: need to implement write_helper to write all bytes (similar to read_helper)

// TODO: fix this (fixed?)
// Broadcast message
void broadcast(int unique_fd, char* username, char* message){
  size_t ulen = strlen(username);
  size_t mlen = strlen(message);

  pthread_mutex_lock(&peers_lock);
  for (int i = 0; i < num_peers; i++) {
    int fd = peers[i];
    if (fd == unique_fd) {
      continue; // don't send back to sender
    } 
    if (write_helper(fd, &ulen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
      continue;
    }
    if (write_helper(fd, username, ulen) != (ssize_t)ulen) {
      continue;
    }
    if (write_helper(fd, &mlen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
      continue;
    }
    if (write_helper(fd, message, mlen) != (ssize_t)mlen) {
      continue;
    }
  }
  pthread_mutex_unlock(&peers_lock);
}

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

// Thread for reading username and messages 
void* peer_read_thread(void* arg) {
  int peer_fd = *(int*)arg;
  free(arg);

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
    // broadcast(peer_fd, username, message);

    free(username);
    free(message);
  }
  return NULL;
}

// Thread for accepting incoming connection thread
void* accept_thread(void* arg) {
  int server_fd = *(int*) arg;
  free(arg);
  // Continuously accepting thread as long as the server is running
  while(1) {
    // Get the socket file descriptor 
    int peer_fd = server_socket_accept(server_fd);
    // Skip if accept return an error
    if (peer_fd < 0)continue;
    
    // Add new peers to the global peer list
    pthread_mutex_lock(&peers_lock);
    if (num_peers < CAPACITY) {
      // store peers
      peers[num_peers++] = peer_fd;
    } else {
      // too many peers
      close(peer_fd);
      pthread_mutex_unlock(&peers_lock);
      continue;
    }
    pthread_mutex_unlock(&peers_lock);

    pthread_t t;
    // Allocate memory to store the peer's socket fd
    int *fd_ptr = malloc(sizeof(int));
    if (!fd_ptr) { close(peer_fd); continue; }
    *fd_ptr = peer_fd;
    // Create a read thread for each peer
    pthread_create(&t, NULL, peer_read_thread, fd_ptr);
  }
  return NULL;
}

    


// // Set up server to listen for incoming peers
// void* listenForConnection (void* arg){
//   int server_socket_fd = (int**) arg;
//   int client_socket_fd;
//   while((client_socket_fd = server_socket_accept(server_socket_fd)) != -1){
//     peers[num_peers] = client_socket_fd; // potential lock
//     num_peers++;

//     // create a thread to handle connection
//   }
// }

// This function is run whenever the user hits enter after typing a message
void input_callback(const char* message) {
  if (strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0) {
    ui_exit();
  } else {
    ui_display(username, message);
  } 
  // TODO: construct a message and send it to everyone else
}

int main(int argc, char** argv) {
  // Make sure the arguments include a username
  if (argc != 2 && argc != 4) {
    fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
    exit(1);
  }

  // Save the username in a global
  username = argv[1];

  // TODO: Set up a server socket to accept incoming connections
  unsigned short port = 0;
  intptr_t server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1) {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // listening on our server
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, accept_thread, (void*) server_socket_fd);

  // Did the user specify a peer we should connect to?
  if (argc == 4) {
    // Unpack arguments
    char* peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);

    // TODO: Connect to another peer in the chat network
    if (socket_connect(peer_hostname, peer_port) == -1) {
      perror("Connection fail");
      exit(EXIT_FAILURE);
    }

    // Call listening thread
    // Send message out
  }

  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.
  ui_init(input_callback);

  // Once the UI is running, you can use it to display log messages
  ui_display("INFO", "This is a handy log message.");

  // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
  ui_run();

  return 0;
}
