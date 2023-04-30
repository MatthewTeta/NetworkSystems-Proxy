/**
 * @file connection.h
 * @brief Provide an interface for connections
 *
 * @date 2023-04-30
 * @version 1.0
 */

#ifndef CONNECTION_H
#define CONNECTION_H

#include <netinet/in.h>
#include <stdio.h>

/**
 * @brief Connection structure
 *
 * @details This struct contains information about a client connection
 *
 * @param fd Client file descriptor
 * @param ip Client IP address
 * @param host Client host information
 */
typedef struct connection {
    int  fd;
    char ip[INET_ADDRSTRLEN];
} connection_t;

/**
 * @brief Initialize a connection
 *
 * @param host Client host information
 * @param port Client port
 * @param connection Connection (output)
 * @return int 0 on success, -1 on error
 */
int connect_to_hostname(char *host, int port, connection_t *connection);

/**
 * @brief Send a message to a connection (guarentees all bytes are sent)
 *
 * @param connection Connection
 * @param msg Message to send
 * @param msg_len Length of message
 *
 * @return ssize_t Number of bytes sent or -1 on error
 */
ssize_t send_to_connection(connection_t *connection, char *msg, size_t msg_len);

/**
 * @brief Send a file to a connection (guarentees all bytes are sent)
 * @param connection Connection
 * @param file File to send
 * @param msg_len Length of message
 *
 * @return ssize_t Number of bytes sent or -1 on error
 */
ssize_t send_to_connection_f(connection_t *connection, FILE *file,
                             size_t msg_len);

/**
 * @brief Close a connection and free the memory
 */
void close_connection(connection_t *connection);

#endif /* CONNECTION_H */
