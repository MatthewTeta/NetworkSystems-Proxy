/**
 * @file main.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Multithreaded / Forking HTTP Proxy Server with Caching and Link
 * Prefetching
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "debug.h"
#include "proxy.h"

#define CACHE_PATH     "cache"
#define BLOCKLIST_PATH "blocklist"

/**
 * @brief Handle SIGINT and SIGCHLD signal
 *
 * @param sig Signal number
 */
void sig_handler(int sig) {
    if (sig == SIGINT) {
        printf("Stopping the proxy...\n");

        // Stop the proxy server
        proxy_stop();

        // Exit the program
        exit(0);
    }
}

void print_usage(char *argv[]) {
    printf("Usage: %s <port> <cache_ttl> [<prefetch_depth>] [-v]\r\n", argv[0]);
}

int main(int argc, char *argv[]) {
    int port           = 0;
    int cache_ttl      = 0;
    int prefetch_depth = 0;
    int verbose        = 0;
    int i              = 0;

    // Parse command line arguments
    if (argc < 3) {
        print_usage(argv);
        exit(1);
    }

    port      = atoi(argv[1]);
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

    // Handle SIGINT
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Restart interrupted system calls
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT) failed");
        fprintf(stderr, "Error setting up signal handler.\n");
        exit(-1);
    }

    // Initialize the proxy server
    proxy_init(CACHE_PATH, BLOCKLIST_PATH, port, cache_ttl, prefetch_depth,
               verbose);

    // Start the proxy server
    proxy_start();

    // Wait for the proxy server to stop
    while (proxy_is_running()) {
        sleep(1);
    }

    DEBUG_PRINT("Proxy server stopped.\n");

    return 0;
}
