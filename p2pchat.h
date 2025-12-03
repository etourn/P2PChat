#if !defined(P2PCHAT_H)
#define P2PCHAT_H

#include <sys/socket.h>

// Helper function to write all the required bytes
void broadcast(const char* username, const char* message);

#endif
