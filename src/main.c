/**
 * @file main.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Multithreaded / Forking HTTP Proxy Server with Caching and Link Prefetching
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "proxy.h"

/**
 * @brief Handle SIGINT signal
 * 
 * @param sig Signal number
 */
void sigint_handler(int sig) {
    // Close the server socket
    server_close();

    // Stop the proxy server
    proxy_stop();

    // Exit the program
    exit(0);
}

void print_usage(char *argv[]) {
    printf("Usage: %s <port> <cache_ttl> [<prefetch_depth>] [-v]\r\n", argv[0]);
}

int main(int argc, char *argv[]) {
    int port = 0;
    int cache_ttl = 0;
    int prefetch_depth = 0;
    int verbose = 0;
    int i = 0;

    // Parse command line arguments
    if (argc < 3) {
        print_usage(argv);
        exit(1);
    }

    port = atoi(argv[1]);
    cache_ttl = atoi(argv[2]);

    if (argc > 3) {
        for (i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-v") == 0) {
                verbose = 1;
            } else {
                prefetch_depth = atoi(argv[i]);
            }
        }
    }

    // Register SIGINT handler
    signal(SIGINT, sigint_handler);

    // Initialize the proxy server
    proxy_init(port, cache_ttl, prefetch_depth, verbose);

    // Start the proxy server
    proxy_start();

    return 0;
}
