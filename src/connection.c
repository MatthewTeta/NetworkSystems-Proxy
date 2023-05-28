/**
 * @file connection.c
 * @brief Connection handling
 *
 * @author Matthew Teta (matthew.tetad@colorado.edu)
 */

#include "connection.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @brief Initialize a connection
 *
 * @param host Client host information
 * @param port Client port
 * @param connection Connection (output)
 * @return int 0 on success, -1 on error
 */
int connect_to_hostname(char *host, int port, connection_t *connection) {
    int             status;
    char            port_str[6] = {0};
    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Get the address of the host

    sprintf(port_str, "%d", port == -1 ? 80 : port);
    status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "Error: Failed to get address info for %s: %s\n", host,
                gai_strerror(status));
        free(connection);
        return -1;
    }

    // Copy the ip string into the connection struct
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    void               *addr = &(ipv4->sin_addr);
    inet_ntop(res->ai_family, addr, connection->ip, INET_ADDRSTRLEN);

    // Create a socket for the client
    connection->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connection->fd < 0) {
        fprintf(stderr, "Error: Failed to create socket for %s\n", host);
        free(connection);
        return -1;
    }
    // fprintf(stderr, "Created socket %d\n", connection->fd);

    // Connect to the server
    status = connect(connection->fd, (struct sockaddr *)res->ai_addr,
                     res->ai_addrlen);
    if (status != 0) {
        perror("Error: Failed to connect to server");
        fprintf(stderr, "Error: Failed to connect to %s\n", connection->ip);
        freeaddrinfo(res);
        close(connection->fd);
        free(connection);
        return -1;
    } else {
        printf("Connected to %s\n", connection->ip);
    }

    freeaddrinfo(res);

    return 0;
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
        // fprintf(stderr, "Sending %ld bytes\n", msg_len - bytes_sent);
        ssize_t sent =
            send(connection->fd, msg + bytes_sent, msg_len - bytes_sent, 0);
        if (sent < 0) {
            fprintf(stderr, "Error: Failed to send message\n");
            return -1;
        }
        if (sent == 0) {
            // fprintf(stderr, "Sent 0 bytes in send_to_connection... \n");
            break;
        }
        // char *tmp = malloc(sent + 1);
        // memcpy(tmp, msg + bytes_sent, sent);
        // tmp[sent] = '\0';
        // fprintf(stderr, "Sent %ld bytes: %s\n", sent, tmp);
        // free(tmp);
        bytes_sent += sent;
    }
    return bytes_sent;
}

/**
 * @brief Send a file to a connection (guarentees all bytes are sent)
 * @param connection Connection
 * @param file File to send
 * @param msg_len Length of message
 *
 * @return ssize_t Number of bytes sent or -1 on error
 */
ssize_t send_to_connection_f(connection_t *connection, FILE *file,
                             size_t msg_len) {
    if (connection == NULL) {
        fprintf(stderr, "Error: Connection is NULL\n");
        return -1;
    }
    if (file == NULL) {
        fprintf(stderr, "Error: File is NULL\n");
        return -1;
    }
    if (msg_len == 0) {
        fprintf(stderr, "Error: Message length is 0\n");
        return -1;
    }
    int fd = fileno(file);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to get file descriptor\n");
        return -1;
    }
    // Seek to the beginning of the file
    off_t off = lseek(fd, 0, SEEK_SET);
    fseek(file, 0, SEEK_SET);
    ssize_t bytes_sent = 0;
    while (bytes_sent < msg_len) {
        ssize_t sent = sendfile(connection->fd, fd, &off, msg_len - bytes_sent);
        if (sent < 0) {
            fprintf(stderr, "Error: Failed to send message_f\n");
            return -1;
        }
        if (sent == 0) {
            // Print the error
            perror("Error: Failed to send message (0) ");
            // fprintf(stderr, "Sent 0 bytes in send_to_connection_f... \n");
            break;
        }
        off += sent;
        bytes_sent += sent;
    }
    return bytes_sent;
}

/**
 * @brief Close a connection
 */
void close_connection(connection_t *connection) { close(connection->fd); }
