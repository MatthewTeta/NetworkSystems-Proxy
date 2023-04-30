/**
 * @file main.c
 * @brief Proxy server using forking in C. The purpose of this is to be as short
 * and concise as possible.
 * @details This is a simple proxy server that uses forking to handle multiple
 * requests. It is not meant to be used in production.
 *
 * @date 2023-04-30
 *
 */

#include <arpa/inet.h>  // inet_ntoa()
#include <errno.h>      // errno
#include <netdb.h>      // gethostbyname()
#include <netinet/in.h> // struct sockaddr_in
#include <signal.h>     // signal()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <sys/stat.h>
#include <sys/wait.h> // waitpid()
#include <unistd.h>

#include "blocklist.h"
#include "connection.h"
#include "request.h"
#include "response.h"

// Constants

// Global variables
volatile int running        = 1;
volatile int parent         = 1;
volatile int num_children   = 0;
int          port           = 8080;
int          ttl            = 60;
char        *blocklist_path = "blocklist";
char        *cache_path     = "cache";
blocklist_t *blocklist      = NULL;

// Function prototypes
void handle_request(connection_t *connection);

void print_usage(char *argv[]) { printf("Usage: %s [port] [ttl]\n", argv[0]); }

/**
 * @brief Handle SIGINT and SIGCHLD signal
 *
 * @param sig Signal number
 */
void sig_handler(int sig) {
    if (sig == SIGINT) {
        printf("Stopping the proxy...\n");

        // Stop accepting new connections
        running = 0;

        // Kill all children
        kill(0, SIGTERM);

        // Wait for all children to exit
        while (num_children > 0) {
            wait(NULL);
        }

        // Free memory
        blocklist_free(blocklist);

        // Exit the program
        exit(0);
    } else if (sig == SIGCHLD) {
        // Wait for all children to exit
        while (waitpid(-1, NULL, WNOHANG) > 0) {
            num_children--;
        }
    }
}

/**
 * @brief Main function
 * @details This is the main function of the program. It is responsible for
 * parsing the command line arguments, setting up the socket, and handling
 * incoming requests.
 *
 * @param argc The number of command line arguments
 * @param argv The command line arguments
 * @return int The exit code of the program
 */
int main(int argc, char *argv[]) {
    if (argc > 3) {
        print_usage(argv);
        exit(EXIT_FAILURE);
    }

    // Parse command line arguments
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    if (argc > 2) {
        ttl = atoi(argv[2]);
    }

    // Validate command line arguments
    if (port < 1 || port > 65535) {
        print_usage(argv);
        exit(EXIT_FAILURE);
    }
    if (ttl < 1) {
        print_usage(argv);
        exit(EXIT_FAILURE);
    }

    // Initialize the blocklist
    blocklist = blocklist_init(blocklist_path);
    if (blocklist == NULL) {
        perror("blocklist_init");
        exit(EXIT_FAILURE);
    }

    // Initialize the cache
    mkdir(cache_path, 0777);

    // Register signal handler
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Restart functions if interrupted by handler
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction(SIGINT) failed");
        fprintf(stderr, "Error setting up signal handler.\n");
        exit(-1);
    }
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction(SIGCHLD) failed");
        fprintf(stderr, "Error setting up signal handler.\n");
        exit(-1);
    }

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind socket to port
    struct sockaddr_in address = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port),
    };
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Handle incoming connections
    while (running) {
        // Accept connection
        int addrlen = sizeof(address);
        int fd      = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen);
        if (fd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Fork process
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // Child process
        if (pid == 0) {
            parent = 0;

            // Close server socket
            close(server_fd);

            // Handle request
            connection_t connection = {
                .fd = fd,
                .ip = {0},
            };
            // Populate the IP address field
            if (inet_ntop(AF_INET, &address.sin_addr, connection.ip,
                          INET_ADDRSTRLEN) == NULL) {
                perror("inet_ntop");
                exit(EXIT_FAILURE);
            }
            handle_request(&connection);

            // Close client socket
            close(fd);

            // Exit child process
            exit(EXIT_SUCCESS);
        }

        // Parent process
        else {
            num_children++;
            // Close client socket
            close(fd);
        }
    }

    // Close server socket
    close(server_fd);

    // Exit program
    return EXIT_SUCCESS;
}

/**
 * @brief Handle incoming request
 * @details This function is responsible for handling incoming requests. It
 * parses the request, checks if the requested URL is blocked, and forwards the
 * request to the server. It then parses the response and forwards it to the
 * client.
 *
 * @param connection The connection to handle
 */
void handle_request(connection_t *connection) {
    http_message_t *message;
    request_t      *request;
    response_t     *response;

    // Recv the request from the client
    message = http_message_recv(connection);
    request = request_parse(message);
    if (request == NULL) {
        fprintf(stderr, "Error: Failed to parse the request\n");
        response_send_error(connection, 400, "Bad Request");
        return;
    }

    // Check if the request is in the blocklist
    if (blocklist_check(blocklist, request->host)) {
        response_send_error(connection, 403, "Forbidden");
        fprintf(stderr, "Error: Request is in the blocklist\n");
        request_free(request);
        return;
    }

    // connection_keep_alive = request_is_connection_keep_alive(request);
    // connection_keep_alive |= !http_message_header_compare(
    //     message, "Proxy-Connection", "Keep-Alive");
    // fprintf(stderr, "Connection: Keep-Alive = %d\n", connection_keep_alive);
    http_message_header_set(message, "Connection", "close");

    // Add the proxy headers
    http_message_header_set(message, "Forwarded", connection->ip);
    http_message_header_set(message, "Via", "1.1 MatthewTetaProxy");
    // Remove proxy headers
    http_message_header_remove(message, "Proxy-Connection");
    http_message_header_remove(message, "Proxy-Authorization");
    http_message_header_remove(message, "Proxy-Authenticate");

    // Send the request to the server
    response = response_fetch(request);
    if (response == NULL) {
        fprintf(stderr, "Error: Failed to send the request\n");
        response_send_error(connection, 500, "Internal Server Error");
        request_free(request);
        return;
    }

    // Send the response to the client
    response_send(response, connection);

    // Free memory
    request_free(request);
    response_free(response);

    return;
}
