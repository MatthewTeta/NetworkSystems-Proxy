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

#include <arpa/inet.h> // inet_ntoa()
#include <errno.h>     // errno
#include <fcntl.h>
#include <netdb.h>      // gethostbyname()
#include <netinet/in.h> // struct sockaddr_in
#include <signal.h>     // signal()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h> // waitpid()
#include <time.h>
#include <unistd.h>
#include <wait.h> // waitpid()

#include "blocklist.h"
#include "connection.h"
#include "md5.h"
#include "request.h"
#include "response.h"

// Constants

// Global variables
volatile int running        = 1;
volatile int parent         = 1;
volatile int num_children   = 0;
int          port           = 8080;
int          cache_timeout  = 60;
char        *blocklist_path = "blocklist";
char        *cache_path     = "cache";
blocklist_t *blocklist      = NULL;

// Function prototypes
void handle_request(connection_t *connection);

void print_usage(char *argv[]) {
    printf("Usage: %s [port] [cache_timeout]\n", argv[0]);
}

/**
 * @brief Handle SIGINT and SIGCHLD signal
 *
 * @param sig Signal number
 */
void sig_handler(int sig) {
    if (sig == SIGINT) {
        // Stop accepting new connections
        running = 0;
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
        cache_timeout = atoi(argv[2]);
    }

    // Validate command line arguments
    if (port < 1 || port > 65535) {
        print_usage(argv);
        exit(EXIT_FAILURE);
    }
    if (cache_timeout < 1) {
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
    sa.sa_flags = 0; // Restart functions if interrupted by handler
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
            // If accept() was interrupted by a signal, try again
            continue;
        }

        // Fork process
        pid_t pid = fork();
        // pid_t pid = 0;
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

            // Free memory
            blocklist_free(blocklist);

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

    printf("Stopping the proxy...\n");

    // Close server socket
    close(server_fd);

    // Block the SIGCHLD signal
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    // // Kill all children
    // kill(0, SIGTERM);

    // Wait for all children to exit
    while (num_children > 0) {
        wait(NULL);
        num_children--;
    }
    printf("All children exited (%d)\n", num_children);

    // Free memory
    printf("Freeing blocklist...\n");
    blocklist_free(blocklist);

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
    http_message_t *message  = NULL;
    request_t      *request  = NULL;
    response_t     *response = NULL;

    // Recv the request from the client
    message = http_message_recv(connection);
    if (message == NULL) {
        fprintf(stderr, "Error: Failed to receive the request\n");
        return;
    }
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

    // Send the request to the serve
    // Get a hash key from the request
    char key[1024];
    request_get_key(request, key, 1024);
    uint8_t hash[16];
    char    hash_str[33];
    // Compute the MD5 hash of the request key
    md5String(key, hash);
    // Convert the hash to a string for the cache entry path
    for (int i = 0; i < 16; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    char path[2048], meta_path[2048];
    snprintf(path, 2048, "%s/%s", cache_path, hash_str);
    snprintf(meta_path, 2048, "%s/.%s", cache_path, hash_str);
    printf("Cache path: %s\n", path);
    // Touch the file
    int fd = open(path, O_RDONLY | O_CREAT);
    // if (fd != -1)
    //     close(fd);
    // Check if the request is in the cache
    // fd = open(path, __O_PATH, 0644);
    // while (fd == -1) {
    //     printf("Waiting for file to be created...\n");
    //     fd = open(path, O_RDONLY | O_CREAT | O_EXCL);
    //     if (fd != 1)
    //         break;
    //     sleep(1);
    // }
    if (fd == -1) {
        perror("open");
        request_free(request);
        exit(EXIT_FAILURE);
    }
    printf("File descriptor: %d\n", fd);
    if (flock(fd, LOCK_EX) == -1) {
        perror("flock");
        exit(EXIT_FAILURE);
    }
    // Get a file descriptor for the cached response
    FILE *f, *f2;
    f = fdopen(fd, "r");
    if (f != NULL) {
        // Check if the cached response is still valid
        struct stat attr;
        stat(path, &attr);
        time_t now = time(NULL);
        // Check if the file is empty
        if (attr.st_size == 0) {
            printf("Cached response is empty\n");
            // fclose(f);
            remove(path);
        } else if (difftime(now, attr.st_ctime) > cache_timeout) {
            printf("Cached response is stale\n");
            // Remove the cached response
            // fclose(f);
            remove(path);
        } else {
            printf("Cached response is valid\n");
            // Read the response from the cache
            response = response_read(f);
            // fclose(f);
            if (response == NULL) {
                fprintf(stderr, "Error: Failed to read the cached response\n");
            }
        }
    }
    // If the response is not in the cache, fetch it from the server
    if (response == NULL) {
        printf("Fetching response from the server\n");
        response = response_fetch(request);
        if (response == NULL) {
            fprintf(stderr, "Error: Failed to send the request\n");
            response_send_error(connection, 500, "Internal Server Error");
            request_free(request);
            // Unlock the cache entry
            if (flock(fd, LOCK_UN) == -1) {
                perror("flock");
                exit(EXIT_FAILURE);
            }
            close(fd);
            return;
        }
        fclose(f);
        // Cache the response
        f = fopen(path, "w");
        if (f == NULL) {
            fprintf(stderr, "Error: Failed to cache the response\n");
        } else {
            fd = fileno(f);
            f2 = fopen(meta_path, "w");
            if (f2 == NULL) {
                fprintf(stderr, "Error: Failed to cache the response\n");
            } else {
                fprintf(f2, "%s", key);
                fclose(f2);
            }
            if (0 != response_write(response, f)) {
                fprintf(stderr, "Error: Failed to cache the response\n");
                // fclose(f);
                remove(path);
            } else {
                // fclose(f);
            }
        }
    }
    // Unlock the cache entry
    if (flock(fd, LOCK_UN) == -1) {
        perror("flock");
        exit(EXIT_FAILURE);
    }
    close(fd);

    if (response == NULL) {
        fprintf(stderr, "Error: Failed to read the response\n");
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
