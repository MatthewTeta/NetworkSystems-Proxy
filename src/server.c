/**
 * @file server.c
 *
 * @brief Calls socket, bind, listen, and accept
 */

#include "server.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Global variables
server_t server;

/**
 * @brief Initialize the server
 *
 * @param port Port to listen on
 * @param verbose Verbose mode
 * @param handle_client Request handler
 */
void server_init(int port, int verbose,
                 void (*handle_client)(int clientfd)) {
    server.port           = port;
    server.verbose        = verbose;
    server.handle_client = handle_client;
}

/**
 * @brief Start the server
 *
 */
void server_start() {
    // Create the socket
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        fprintf(stderr, "Error: Failed to create socket\n");
        exit(1);
    }
    // Set the socket options to reuse the address
    int opt = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        fprintf(stderr, "Error: Failed to set socket options\n");
        exit(1);
    }
    // Bind the socket to the port
    struct sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(server.port);
    if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        fprintf(stderr, "Error: Failed to bind socket\n");
        exit(1);
    }
    // Listen on the socket
    if (listen(serverfd, 10) < 0) {
        fprintf(stderr, "Error: Failed to listen on socket\n");
        exit(1);
    }
    // Accept connections
    while (1) {
        // Accept the connection
        struct sockaddr_in client_addr;
        socklen_t          client_addr_len = sizeof(client_addr);
        int                clientfd =
            accept(serverfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (clientfd < 0) {
            fprintf(stderr, "Error: Failed to accept connection\n");
            exit(1);
        }
        // Call the request handler
        server.handle_client(clientfd);
    }
}
