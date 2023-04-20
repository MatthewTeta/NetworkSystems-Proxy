/**
 * @file server.h
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief Abstract forking server
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 *
 */

// TOOD: Move some of this stuff to "client"

#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <netdb.h>

/**
 * @brief Connection structure
 *
 * @details This struct contains information about a client connection
 *
 * @param clientfd Client file descriptor
 * @param client_ip Client IP address
 * @param host Client host information
 */
typedef struct connection {
    int                clientfd;
    char               client_ip[INET_ADDRSTRLEN];
    struct hostent     host;
    struct sockaddr_in client_addr;
    socklen_t          client_addr_len;
} connection_t;

/**
 * @brief Server configuration
 *
 * @details This struct contains the configuration for the server
 *
 * @param port Port to listen on
 * @param verbose Verbose mode
 * @param handle_client Connection handler
 */
typedef struct server_config {
    int serverfd;
    int port;
    int verbose;
    void (*handle_client)(connection_t *connection);
} server_config_t;

/**
 * @brief Start the server
 *
 */
void server_start(server_config_t config);

/**
 * @brief Stop the server
 *
 * @details This function will block until all client connections are closed
 */
void server_stop();

/**
 * @brief Exit a client connection
 */
void close_connection(connection_t *connection);

/**
 * @brief Initialize a connection
 *
 * @param host Client host information
 * @param port Client port
 * @return connection_t* Connection
 */
connection_t *connect_to_hostname(char *host, int port);

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

#endif
