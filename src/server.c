/**
 * @file server.c
 *
 * @brief Calls socket, bind, listen, and accept, fork
 */

#include "server.h"

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
#include "pid_list.h"

// Global variables
server_config_t server;
pid_list_t     *child_pids  = NULL;
int             stop_server = 0;
int             parent      = 1;

/**
 * @brief Start the server
 *
 */
void server_start(server_config_t server_config) {
    memcpy(&server, &server_config, sizeof(server_config_t));
    // Create the socket
    server.serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.serverfd < 0) {
        fprintf(stderr, "Error: Failed to create socket\n");
        exit(1);
    }
    DEBUG_PRINT("Created socket\n");
    // Set the socket options to reuse the address
    int opt = 1;
    if (setsockopt(server.serverfd, SOL_SOCKET, SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        fprintf(stderr, "Error: Failed to set socket options\n");
        exit(1);
    }
    DEBUG_PRINT("Set socket options: port %d\n", server.port);
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
        struct sockaddr_in *c_addr_in  = &(connection->client_addr);
        socklen_t          *c_addr_len = &(connection->client_addr_len);
        connection->clientfd =
            accept(server.serverfd, (struct sockaddr *)c_addr_in, c_addr_len);
        DEBUG_PRINT("Accepted connection\n");
        if (connection->clientfd < 0) {
            fprintf(stderr, "Error: Failed to accept connection\n");
            exit(1);
        }
        // Fork the process to handle the request
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error: Failed to fork process\n");
            exit(1);
        }
        if (pid) {
            // Parent process
            // Append the child process to the list of child processes
            if (child_pids == NULL) {
                child_pids = pid_list_create(pid);
            } else {
                pid_list_append(child_pids, pid);
            }
            pid_list_print(child_pids);
            close(connection->clientfd);
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
            char *client_ip = inet_ntoa(connection->client_addr.sin_addr);
            printf("Client IP address: %s\n", client_ip);
            // printf("Established connection with %s (%s)\n", hostp->h_name,
            //        hostaddrp);
        }
        // Call the request handler
        while (!stop_server) {
            server.handle_client(connection);
        }
    }
}

/**
 * @brief Stop the server. This function is called by the signal handler.
 *
 */
void server_stop() {
    stop_server = 1;
    if (parent) {
        // Close the listening socket and tell all child processes to exit
        close(server.serverfd);
        // Mask the SIGCHLD signal so that it doesn't interrupt the waitpid
        // call
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, NULL);
        pid_list_t *current = child_pids;
        while (current != NULL) {
            // KILL ALL THE CHILDREN
            kill(current->pid, SIGINT);
            current = current->next;
        }
        // Loop through the child pids and wait for them to exit
        printf("\nWaiting for child processes to exit gracefully...\n");
        current = child_pids;
        while (current != NULL) {
            int status;
            // REAP ALL THE CHILDREN
            waitpid(current->pid, &status, 0);
            current = current->next;
        }
        // Free the child pid list
        pid_list_free(child_pids);
        // Unblock the SIGCHLD signal
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        if (server.verbose) {
            printf("Server stopped\n");
        }
    }
}

/**
 * @brief Exit the client process
 */
void close_connection(connection_t *connection) {
    if (server.verbose) {
        printf("Closing connection\n");
    }
    close(connection->clientfd);
    free(connection);
    exit(0);
}

/**
 * @brief Initialize a connection
 *
 * @param clientfd Client file descriptor
 * @param host Client host information
 * @param port Client port
 * @return connection_t* Connection
 */
connection_t *connect_to_hostname(char *host, int port) {
    connection_t *connection = malloc(sizeof(connection_t));
    memset(connection, 0, sizeof(connection_t));

    // Create a socket for the client
    connection->clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection->clientfd < 0) {
        fprintf(stderr, "Error: Failed to create socket\n");
        return NULL;
    }

    // Set the servers address
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port   = htons(port);
    // Convert the request host to an IP address
    char ipstr[INET_ADDRSTRLEN];
    hostname_to_ip(host, ipstr, INET_ADDRSTRLEN);
    DEBUG_PRINT("Host: %s, IP: %s\n", host, ipstr);
    // Set the server address to the IP address
    if (inet_pton(AF_INET, ipstr, &client_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", ipstr);
        return NULL;
    }
    memcpy((&connection->client_addr), &client_addr, sizeof(client_addr));
    connection->client_addr_len = client_addr_len;
    // Get the servers's address
    char           *hostaddrp;
    struct hostent *hostp;
    // TODO: Wat?? Make this more readable using more stack variables
    hostp = gethostbyaddr(
        (const char *)&((struct sockaddr_in *)(&connection->client_addr))
            ->sin_addr.s_addr,
        sizeof(((struct sockaddr_in *)(&connection->client_addr))
                   ->sin_addr.s_addr),
        AF_INET);
    if (hostp == NULL) {
        fprintf(stderr, "ERROR on gethostbyaddr\n");
        exit(-3);
    }
    hostaddrp =
        inet_ntoa(((struct sockaddr_in *)(&connection->client_addr))->sin_addr);
    if (hostaddrp == NULL) {
        fprintf(stderr, "ERROR on inet_ntoa\n");
        exit(-3);
    }
    // Print who has connected
    if (server.verbose)
        printf("Established connection with %s (%s)\n", hostp->h_name,
               hostaddrp);

    // Set up the connection struct
    strncpy(connection->client_ip, hostaddrp, INET_ADDRSTRLEN);
    memcpy(&connection->host, hostp, sizeof(struct hostent));
    memcpy((&connection->client_addr), &client_addr, sizeof(client_addr));
    connection->client_addr_len = client_addr_len;

    return connection;
}
