#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "writing.h"
#include "ui.h"
#include "reading.h"

#define CAPACITY 1000
#define MESSAGE_LEN 2048

pthread_mutex_t peers_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t seen_lock = PTHREAD_MUTEX_INITIALIZER;

// Keep the username in a global so we can access it from the callback
const char* username;
// keep a count of messages sent for id
int count = 0;
// seen array
char* seen[CAPACITY]; 

// List of peers
intptr_t peers[CAPACITY];
int num_peers = 0;

// Broadcast message
void broadcast(const char* username, const char* message, const char* message_id){

  // get lengths of our data
  size_t ulen = strlen(username);
  size_t mlen = strlen(message);
  size_t milen = strlen(message_id);

  // Write to all peers
  for (int i = 0; i < num_peers; i++) {    
    if (write_helper(peers[i], (char*) &milen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
      fprintf(stderr, "Error transmitting over network\n");
      break;
    }
    if (write_helper(peers[i], message_id, milen) != (ssize_t)milen) {
      fprintf(stderr, "Error transmitting over network\n");
      break;
    }
    if (write_helper(peers[i], (char*) &ulen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
      fprintf(stderr, "Error transmitting over network\n");
      break;
    }
    if (write_helper(peers[i], username, ulen) != (ssize_t)ulen) {
      fprintf(stderr, "Error transmitting over network\n");
      break;
    }
    if (write_helper(peers[i], (char*) &mlen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
      fprintf(stderr, "Error transmitting over network\n");
      break;
    }
    if (write_helper(peers[i], message, mlen) != (ssize_t)mlen) {
      fprintf(stderr, "Error transmitting over network\n");
      break;
    }
  }
}

// Thread for accepting incoming connection thread
void *accept_thread(void *arg)
{
  intptr_t server_fd = (intptr_t)arg;
  // Continuously accepting thread as long as the server is running
  while (1)
  {
    // Get the socket file descriptor
    intptr_t peer_fd = server_socket_accept(server_fd);
    // Skip if accept return an error
    if (peer_fd < 0)
      continue;

    // Add new peers to the global peer list
    pthread_mutex_lock(&peers_lock);
    if (num_peers < CAPACITY)
    {
      // store peers
      peers[num_peers++] = peer_fd;
    }
    else
    {
      // too many peers
      close(peer_fd);
      pthread_mutex_unlock(&peers_lock);
      continue;
    }
    pthread_mutex_unlock(&peers_lock);

    // Create a read thread for each peer
    pthread_t t;
    // create struct peer to pass in args
    peer* p = malloc(sizeof(*p));
    p->peer_fd = peer_fd;
    p->seen = seen;
    pthread_create(&t, NULL, peer_read_thread, (void*) p);
  }
  return NULL;
}

// This function is run whenever the user hits enter after typing a message
void input_callback(const char *message)
{
  if (strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0)
  {
    ui_exit();
  }
  else
  {
    ui_display(username, message);
  } 

  // Create message id
  // increment count
  count++;
  // create message id
  char message_id[64];
  snprintf(message_id, sizeof(message_id), "%s%d", username, count);
  // add to our own seen set
  pthread_mutex_lock(&seen_lock);
  for (int i=0; i<CAPACITY; i++){
    if (seen[i] == NULL) {
      // cite: https://man7.org/linux/man-pages/man3/strdup.3.html
      seen[i] = strdup(message_id);  
      break;
    }
  }
  pthread_mutex_unlock(&seen_lock);
  // Valid message broadcast to network
  broadcast(username, message, message_id);
}

int main(int argc, char **argv)
{
  // Make sure the arguments include a username
  if (argc != 2 && argc != 4)
  {
    fprintf(stderr, "Usage: %s <username> [<peer> <port number>]\n", argv[0]);
    exit(1);
  }

  // Save the username in a global
  username = argv[1];

  // TODO: Set up a server socket to accept incoming connections
  unsigned short port = 0;
  intptr_t server_socket_fd = server_socket_open(&port);
  if (server_socket_fd == -1)
  {
    perror("Server socket was not opened");
    exit(EXIT_FAILURE);
  }

  // start listening on our server
  // cite: https://man7.org/linux/man-pages/man2/listen.2.html
  if (listen(server_socket_fd, SOMAXCONN) == -1) {
  perror("listen");
  exit(EXIT_FAILURE);
}

  // create thread to wait for connections
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, accept_thread, (void *)server_socket_fd);

  // Did the user specify a peer we should connect to?
  if (argc == 4)
  {
    // Unpack arguments
    char *peer_hostname = argv[2];
    unsigned short peer_port = atoi(argv[3]);

    // TODO: Connect to another peer in the chat network
    intptr_t peer_fd;
    if ((peer_fd = socket_connect(peer_hostname, peer_port)) == -1)
    {
      perror("Connection fail");
      exit(EXIT_FAILURE);
    }

    // add to peer list
    pthread_mutex_lock(&peers_lock); // lock when altering shared array
    peers[num_peers++] = peer_fd;
    pthread_mutex_unlock(&peers_lock); 

    // Call reading thread
    pthread_t t;
    // Create peer struct
    peer* p = malloc(sizeof(*p));
    p->peer_fd = peer_fd;
    p->seen = seen;
    pthread_create(&t, NULL, peer_read_thread, (void*)p);
  }

  // Set up the user interface. The input_callback function will be called
  // each time the user hits enter to send a message.
  ui_init(input_callback);

  // Display the port the server is listening on
  char port_msg[64];
  snprintf(port_msg, sizeof(port_msg), "Listening on port %d", port);
  ui_display("INFO", port_msg);

  // Once the UI is running, you can use it to display log messages
  ui_display("INFO", "This is a handy log message.");

  // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
  ui_run();

  return 0;
}
