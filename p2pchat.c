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

// Keep the username in a global so we can access it from the callback
const char *username;

#define CAPACITY 1000
#define MESSAGE_LEN 2048

pthread_mutex_t peers_lock = PTHREAD_MUTEX_INITIALIZER;

intptr_t peers[CAPACITY]; // holds the socket fd of each connected peer
int num_peers = 0;

// Broadcast message
void broadcast(int unique_fd, char *username, char *message) {
    // get lengths of our data
    size_t ulen = strlen(username);
    size_t mlen = strlen(message);
    // Write to all peers
    for (int i = 0; i < num_peers; i++) {
        if (peers[i] == unique_fd)
            continue;

        if (write_helper(peers[i], (char *)&ulen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
            fprintf(stderr, "Error transmitting over network\n");
            continue;
        }
        if (write_helper(peers[i], username, ulen) != (ssize_t)ulen) {
            fprintf(stderr, "Error transmitting over network\n");
            continue;
        }
        if (write_helper(peers[i], (char *)&mlen, sizeof(size_t)) != (ssize_t)sizeof(size_t)) {
            fprintf(stderr, "Error transmitting over network\n");
            continue;
        }
        if (write_helper(peers[i], message, mlen) != (ssize_t)mlen) {
            fprintf(stderr, "Error transmitting over network\n");
            continue;
        }
    }
}

// Thread to read both username and message. Receive lengths first and then contents
void* peer_read_thread(void* arg) {
    intptr_t peer_fd = (intptr_t) arg;

    // Keep reading information from this peer
    while (1) {
        // Reading the username's length 
        size_t username_len;
        if (read(peer_fd, &username_len, sizeof(size_t)) != sizeof(size_t)) {
            break; // Stop reading if there's an error
        }

        // Check if size is appropriate 
        if (username_len > MESSAGE_LEN)
            break;

        // Allocate memory for the username
        char *recv_username = malloc(username_len + 1);
        if (!recv_username) break;
        recv_username[username_len] = '\0';

        // Read the username
        if (read_helper(peer_fd, recv_username, username_len) != (ssize_t)username_len) {
            free(recv_username);
            break;
        }

        // Get message length
        size_t message_len;
        if (read_helper(peer_fd, &message_len, sizeof(size_t)) != sizeof(size_t)) {
            free(recv_username);
            break;
        }

        // Check size
        if (message_len > MESSAGE_LEN) {
            free(recv_username);
            break;
        }

        char *recv_message = malloc(message_len + 1);
        if (!recv_message) {
            free(recv_username);
            break;
        }
        recv_message[message_len] = '\0';

        // Read the full message
        if (read_helper(peer_fd, recv_message, message_len) != (ssize_t)message_len) {
            free(recv_username);
            free(recv_message);
            break;
        }

        ui_display(recv_username, recv_message);

        // Broadcast to all peer 
        broadcast(peer_fd, recv_username, recv_message);

        free(recv_username);
        free(recv_message);
    }
    return NULL;
}

// Thread for accepting incoming connection thread
void* accept_thread(void* arg) {
    intptr_t server_fd = (intptr_t)arg;

    // Continuously accepting thread as long as the server is running
    while (1) {
        // Get the socket file descriptor
        intptr_t peer_fd = server_socket_accept(server_fd);

        // Skip if accept return an error
        if (peer_fd < 0)
            continue;

        // Add new peers to the global peer list
        pthread_mutex_lock(&peers_lock);
        if (num_peers < CAPACITY) {
            // store peers
            peers[num_peers++] = peer_fd;
            pthread_mutex_unlock(&peers_lock);

            // Create a read thread for each peer
            pthread_t t;
            pthread_create(&t, NULL, peer_read_thread, (void *)peer_fd);
        } else {
            // too many peers
            pthread_mutex_unlock(&peers_lock);
            close(peer_fd);
        }
    }
    return NULL;
}

// This function is run whenever the user hits enter after typing a message
void input_callback(const char *message) {
    if (strcmp(message, ":quit") == 0 || strcmp(message, ":q") == 0) {
        ui_exit();
    } else {
        ui_display(username, message);

        // Send to all peers
        pthread_mutex_lock(&peers_lock);
        broadcast(-1, (char*)username, (char*)message);
        pthread_mutex_unlock(&peers_lock);
    }
}

int main(int argc, char **argv) {
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

    // Print the port number
    printf("Listening on port: %hu\n", port);
    fflush(stdout);

    // listening on our server
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, accept_thread, (void *)server_socket_fd);

    // Did the user specify a peer we should connect to?
    if (argc == 4) {
        // Unpack arguments
        char *peer_hostname = argv[2];
        unsigned short peer_port = atoi(argv[3]);

        // TODO: Connect to another peer in the chat network
        intptr_t peer_fd = socket_connect(peer_hostname, peer_port);
        if (peer_fd < 0) {
            perror("Connection fail");
        } else {
            // Add peer to list
            pthread_mutex_lock(&peers_lock);
            peers[num_peers++] = peer_fd;
            pthread_mutex_unlock(&peers_lock);

            // Start thread to read from this peer
            pthread_t t;
            pthread_create(&t, NULL, peer_read_thread, (void*)peer_fd);
        }
    }

    // Set up the user interface. The input_callback function will be called
    // each time the user hits enter to send a message.
    ui_init(input_callback);

    // Once the UI is running, you can use it to display log messages
    char port_msg[64];
    snprintf(port_msg, sizeof(port_msg), "Listening on port: %hu", port);
    ui_display("INFO", port_msg);

    // Run the UI loop. This function only returns once we call ui_stop() somewhere in the program.
    ui_run();

    close(server_socket_fd);
    return 0;
}
