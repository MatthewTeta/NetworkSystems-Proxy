/**
 * @file server.c
 *
 * @brief Calls socket, bind, listen, and accept, fork
 */

#include "server.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "IP.h"
#include "debug.h"

// Global variables
server_config_t server;
// pid_list_t     *child_pids  = NULL;
int stop_server = 0;
int is_running  = 0;
int parent      = 1;

/**
 * @brief Check if the server is running
 * @details The server takes some time to clean up after itself, so this allows
 * the caller to wait
 * @return 1 if the server is running, 0 otherwise
 */
int server_is_running() { return is_running; }

// void sigchld_handler(int sig) {
//     if (sig == SIGCHLD) {
//         // Reap any child processes that have exited
//         int status;
//         if (waitpid(-1, &status, WNOHANG) > 0) {
//             DEBUG_PRINT("Child process has exited.\n");
//         }
//     }
// }

/**
 * @brief Start the server
 *
 */
void server_start(server_config_t server_config) {
    memcpy(&server, &server_config, sizeof(server_config_t));
    is_running = 1;

    // // Register the SIGCHLD signal handler
    // struct sigaction sa;
    // sa.sa_handler = sigchld_handler;
    // sigemptyset(&sa.sa_mask);
    // if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    //     perror("sigaction(SIGCHLD) failed");
    //     fprintf(stderr, "Error setting up signal handler.\n");
    //     exit(-1);
    // }

    // Create the socket
    server.serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.serverfd < 0) {
        fprintf(stderr, "Error: Failed to create socket\n");
        exit(1);
    }
    DEBUG_PRINT("Created socket\n");
#ifdef DEBUG
    // Set the socket options to reuse the address
    int opt = 1;
    if (setsockopt(server.serverfd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        fprintf(stderr, "Error: Failed to set socket options\n");
        exit(1);
    }
    DEBUG_PRINT("Set socket options: port %d\n", server.port);
#endif
    // Bind the socket to the port
    struct sockaddr_in server_addr;
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(server.port);
    if (bind(server.serverfd, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: Failed to bind socket\n");
        exit(1);
    }
    DEBUG_PRINT("Bound socket\n");
    // Listen on the socket
    if (listen(server.serverfd, 10) < 0) {
        fprintf(stderr, "Error: Failed to listen on socket\n");
        exit(1);
    }
    DEBUG_PRINT("Listening on socket\n");
    stop_server = 0;
    // Accept connections
    while (!stop_server) {
        // Reap any child processes that have exited
        int   status;
        pid_t p;
        while ((p = waitpid(-1, &status, WNOHANG)) > 0) {
            DEBUG_PRINT("  --  Child process has exited (%d).\n", p);
        }
        // if (child_pids != NULL) {
        //     // Check if any child processes have exited
        //     pid_list_t *pid_list = child_pids;
        //     while (pid_list != NULL) {
        //         pid_t pid = pid_list->pid;
        //         int   status;
        //         if (waitpid(pid, &status, WNOHANG) > 0) {
        //             // Child process has exited
        //             DEBUG_PRINT("Child process %d has exited.\n", pid);
        //             // Remove the child process from the list
        //             pid_list_t *next = pid_list->next;
        //             pid_list_remove(child_pids, pid);
        //             pid_list = next;
        //         } else {
        //             // Child process has not exited
        //             pid_list = pid_list->next;
        //         }
        //     }
        // }
        DEBUG_PRINT("Waiting for connection\n");
        // Accept the connection
        connection_t *connection = malloc(sizeof(connection_t));
        memset(connection, 0, sizeof(connection_t));
        struct sockaddr_in *c_addr_in  = &(connection->addr);
        socklen_t          *c_addr_len = &(connection->addr_len);
        connection->addr_len           = sizeof(struct sockaddr_in);
        connection->fd =
            accept(server.serverfd, (struct sockaddr *)c_addr_in, c_addr_len);
        DEBUG_PRINT("Accepted connection\n");
        if (connection->fd < 0) {
            fprintf(stderr, "Error: Failed to accept connection\n");
            exit(1);
        }
        // Fork the process to handle the request
        pid_t pid = fork();
        // pid_t pid = 0;
        if (pid < 0) {
            fprintf(stderr, "Error: Failed to fork process\n");
            exit(1);
        }
        if (pid) {
            // Parent process
            // Append the child process to the list of child processes
            // if (child_pids == NULL) {
            //     child_pids = pid_list_create(pid);
            // } else {
            //     pid_list_append(child_pids, pid);
            // }
            // pid_list_print(child_pids);
            close(connection->fd);
            continue;
        }
        DEBUG_PRINT("IN CHILD PROCESS: %d\n", pid);
        parent = 0;
        close(server.serverfd);
        // Get the client's address
        // char           *hostaddrp;
        // struct hostent *hostp;
        // // TODO: Wat?? Make this more readable using more stack variables
        // struct in_addr *c_sin_addr   = &c_addr_in->sin_addr;
        // char           *c_s_addr     = (char *)&c_sin_addr->s_addr;
        // size_t          c_s_addr_len = sizeof(c_sin_addr->s_addr);
        // hostp = gethostbyaddr(c_s_addr, c_s_addr_len, AF_INET);
        // if (hostp == NULL) {
        //     herror("Error on gethostbyaddr");
        //     fprintf(stderr, "ERROR on gethostbyaddr\n");
        //     exit(-3);
        // }
        // hostaddrp = inet_ntoa(*c_sin_addr);
        // if (hostaddrp == NULL) {
        //     fprintf(stderr, "ERROR on inet_ntoa\n");
        //     exit(-3);
        // }
        // Print who has connected
        if (server.verbose) {
            // char *ip = inet_ntoa(connection->addr.sin_addr);
            // char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(connection->addr.sin_addr), connection->ip,
                      INET_ADDRSTRLEN);
            printf("Client IP address: %s\n", connection->ip);
            // printf("Established connection with %s (%s)\n", hostp->h_name,
            //        hostaddrp);
        }
        // Call the request handler
        while (1) {
            server.handle_client(connection);
            close_connection(connection);
            exit(0);
        }
    }
    // In parent context
    // Close the listening socket and tell all child processes to exit
    close(server.serverfd);
    // Mask the SIGCHLD signal so that it doesn't interrupt the waitpid
    // call
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    // pid_list_t *current = child_pids;
    // while (current != NULL) {
    //     // KILL ALL THE CHILDREN
    //     kill(current->pid, SIGINT);
    //     current = current->next;
    // }
    kill(-1, SIGINT);
    // Loop through the child pids and wait for them to exit
    printf("\nWaiting for child processes to exit gracefully...\n");
    // current = child_pids;
    // REAP ALL THE CHILDREN
    int status;
    while (waitpid(-1, &status, WNOHANG) >= 0)
        ;
    assert(errno == ECHILD);
    // Free the child pid list
    // pid_list_free(child_pids);
    // Unblock the SIGCHLD signal
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    if (server.verbose) {
        printf("Server stopped\n");
    }
    is_running = 0;
}

/**
 * @brief Stop the server. This function is called by the signal handler.
 *
 */
void server_stop() { stop_server = 1; }

/**
 * @brief Exit the client process
 */
void close_connection(connection_t *connection) {
    if (server.verbose) {
        printf("Closing connection\n");
    }
    close(connection->fd);
    free(connection);
}

/**
 * @brief Initialize a connection
 *
 * @param host Client host information
 * @param port Client port
 * @return connection_t* Connection (note that the host field is not set)
 */
connection_t *connect_to_hostname(char *host, int port) {
    connection_t *connection = malloc(sizeof(connection_t));
    memset(connection, 0, sizeof(connection_t));

    int             status;
    char            port_str[6] = {0};
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Get the address of the host

    sprintf(port_str, "%d", port);
    status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "Error: Failed to get address info for %s: %s\n", host,
                gai_strerror(status));
        free(connection);
        return NULL;
    }

    // Copy the ip string into the connection struct
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    void               *addr = &(ipv4->sin_addr);
    inet_ntop(res->ai_family, addr, connection->ip, INET_ADDRSTRLEN);

    // Copy the addr into the connection struct
    memcpy(&connection->addr, res->ai_addr, res->ai_addrlen);
    connection->addr_len = res->ai_addrlen;

    // Create a socket for the client
    connection->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection->fd < 0) {
        fprintf(stderr, "Error: Failed to create socket for %s\n", host);
        free(connection);
        return NULL;
    }
    DEBUG_PRINT("Created socket %d\n", connection->fd);

    // Connect to the server
    status = connect(connection->fd, (struct sockaddr *)res->ai_addr,
                     res->ai_addrlen);
    if (status != 0) {
        perror("Error: Failed to connect to server");
        fprintf(stderr, "Error: Failed to connect to %s\n", connection->ip);
        freeaddrinfo(res);
        close(connection->fd);
        free(connection);
        return NULL;
    } else {
        if (server.verbose) {
            printf("Connected to %s\n", connection->ip);
        }
    }

    freeaddrinfo(res);

    return connection;
}

/**
 * @brief Send a message to a connection (guarentees all bytes are sent)
 *
 * @param connection Connection
 * @param msg Message to send
 * @param msg_len Length of message
 *
 * @return ssize_t Number of bytes sent or -1 on error
 */
ssize_t send_to_connection(connection_t *connection, char *msg,
                           size_t msg_len) {
    if (connection == NULL) {
        fprintf(stderr, "Error: Connection is NULL\n");
        return -1;
    }
    if (msg == NULL) {
        fprintf(stderr, "Error: Message is NULL\n");
        return -1;
    }
    if (msg_len == 0) {
        fprintf(stderr, "Error: Message length is 0\n");
        return -1;
    }
    ssize_t bytes_sent = 0;
    while (bytes_sent < msg_len) {
        ssize_t sent =
            send(connection->fd, msg + bytes_sent, msg_len - bytes_sent, 0);
        if (sent < 0) {
            fprintf(stderr, "Error: Failed to send message\n");
            return -1;
        }
        if (sent == 0) {
            DEBUG_PRINT("Sent 0 bytes in send_to_connection... \n");
        }
        bytes_sent += sent;
    }
    return bytes_sent;
}
