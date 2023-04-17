/**
 * @file http.h
 * @brief HTTP definitions and helpers
 * @author Matthew Teta (matthew.teta@coloradod.edu)
 * @version 0.1
 */

#include "http.h"

#include <ctype.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

http_message_t *http_message_recv(connection_t *connection) {
    http_message_t *message = malloc(sizeof(http_message_t));
    memset(message, 0, sizeof(http_message_t));
    // Open a stream to write the message into
    message->fp    = open_memstream(&message->message, &message->message_len);
    int message_fd = fileno(message->fp);

    // Read the message into the stream
    char    buffer[BUFSIZ];
    ssize_t bytes_read;
    int     header_complete = 0;
    // Poll vars
    short         revents;
    struct pollfd fds;
    int           rv;
    // Use sendfile to copy the message into the stream
    while (!header_complete) {
        // Set up a poll to timeout if we don't get any data
        // Poll the socket for data until we get something or a
        // timeout is recieved
        revents     = 0;
        fds.fd      = connection->clientfd;
        fds.events  = POLLIN;
        fds.revents = revents;
        rv          = poll(&fds, 1, KEEP_ALIVE_TIMEOUT_MS);
        if (rv < 0) {
            // This will fail if the parent recieves a SIGINT
            // This is fine, check if nread is 0 to see if we have
            // received any data If we have received data, continue
            // processing the message If we have not received any
            // data, exit the child process
            if (bytes_read > 0)
                break;
            DEBUG_PRINT("Poll failed, exiting child process\n");
            message_free(message);
            return NULL;
        } else if (rv == 0) {
            // Timeout -> close connection
            message_free(message);
            return NULL;
        }
        // We got data before the timeout, read it
        while ((bytes_read = sendfile(message_fd, connection->clientfd, NULL,
                                      MESSAGE_CHUNK_SIZE)) > 0) {
            DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
            if (bytes_read == 0) {
                DEBUG_PRINT("Client socket closed.\n");
                message_free(message);
                return NULL;
            } else if (bytes_read < 0) {
                DEBUG_PRINT("Error reading from client socket.\n");
                message_free(message);
                return NULL;
            }
            if ((message->body = strstr(message->message, "\r\n\r\n")) !=
                NULL) {
                message->body += 4;
                message->header_len = message->body - message->message;
                header_complete     = 1;
                break;
            }
        }
    }
    if (!header_complete) {
        DEBUG_PRINT("Header is not complete.\n");
        // Send a 400 Bad message response
        response_send(connection->clientfd, 400, "Bad message", NULL, 0);
        message_free(message);
        return NULL;
    }
    // At this point, we have recieved the entire header and a partial body.
    // We need to parse the header to get the content length, and then read
    // the rest of the body.
    fflush(message->fp);
    // Parse the message
    if (0 != message_header_parse(message)) {
        DEBUG_PRINT("Error parsing message header.\n");
        // Send a 400 Bad message response
        response_send(connection->clientfd, 400, "Bad message", NULL, 0);
        message_free(message);
        return NULL;
    }

    // Get the body length
    char *body_length = http_headers_get(message->headers, "Content-Length");
    if (body_length != NULL) {
        message->body_len = atoi(body_length);
    }

    if (message->body_len > 0) {
        int body_length = message->message_len - message->body_len;
        if (body_length < message->body_len) {
            DEBUG_PRINT("Reading the rest of the body.\n");
        } else if (body_length > message->body_len) {
            DEBUG_PRINT("Body is longer than content length.\n");
        }
        while (body_length < message->body_len) {
            // Use sendfile to copy the message into the stream
            bytes_read = sendfile(message_fd, connection->clientfd, NULL,
                                  message->body_len - body_length);
            DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
            if (bytes_read == 0) {
                DEBUG_PRINT("Client socket closed.\n");
                message_free(message);
                return NULL;
            } else if (bytes_read < 0) {
                DEBUG_PRINT("Error reading from client socket.\n");
                message_free(message);
                return NULL;
            }
            body_length += bytes_read;
        }
        fflush(message->fp);
    }
}

/**
 * @brief Send an HTTP message to a connection
 *
 * @param message HTTP message
 */
void http_message_free(http_message_t *message) {
    if (message == NULL) {
        return;
    }
    if (message->fp != NULL) {
        fclose(message->fp);
    }
    if (message->headers != NULL) {
        http_headers_free(message->headers);
    }
    if (message->message != NULL) {
        free(message->message);
    }
    free(message);
}

/**
 * @brief Parse HTTP headers
 * @details Parses HTTP headers from a string. The string must include the
 * \r\n\r\n delimiter. If the function returns NULL, the headers were malformed
 * and the list is automatically freed.
 *
 * @param headers_str Headers string including \r\n\r\n delimiter
 * @return http_headers_t* Parsed headers
 */
http_headers_t *http_headers_parse(char *headers_str) {
    // TODO: Skip the first line (request line)
    http_headers_t *headers = malloc(sizeof(http_headers_t));
    headers->headers =
        malloc(sizeof(http_header_t *) * HTTP_HEADER_COUNT_DEFAULT);
    headers->size = HTTP_HEADER_COUNT_DEFAULT;
    char *line    = strtok(headers_str, "\r\n");
    while (line != NULL) {
        if (strcmp(line, "\r\n") == 0) {
            // End of headers
            break;
        }
        char *key = strtok(line, ":");
        char *val = strtok(NULL, ":");
        if (key == NULL || val == NULL) {
            // Malformed header
            DEBUG_PRINT("Malformed header: %s\n", line);
            http_headers_free(headers);
            return NULL;
        }
        http_headers_set(headers, key, val);
        line = strtok(NULL, "\r\n");
    }
}

/**
 * @brief Free HTTP headers
 *
 * @param headers Headers to free
 */
void http_headers_free(http_headers_t *headers) {
    if (headers == NULL) {
        return;
    }
    for (int i = 0; i < headers->count; i++) {
        free(headers->headers[i]->key);
        free(headers->headers[i]->value);
        free(headers->headers[i]);
    }
    free(headers->headers);
    free(headers);
}

/**
 * @brief Get a header value from a list of headers
 *
 * @param headers Headers
 * @param key Header key
 * @return char* Header value
 */
char *http_headers_get(http_headers_t *headers, char *key) {
    // TODO: Use a hash table for faster lookup
    // TODO: Convert to lowercase for case-insensitive comparison
    for (int i = 0; i < headers->count; i++) {
        if (strcmp(headers->headers[i]->key, key) == 0) {
            return headers->headers[i]->value;
        }
    }
    return NULL;
}

/**
 * @brief Set a header value in a list of headers
 *
 * @param headers Headers
 * @param key Header key
 * @param value Header value
 */
void http_headers_set(http_headers_t *headers, char *key, char *value) {
    // TODO: Use a hash table for faster lookup
    // TODO: Convert to lowercase for case-insensitive comparison
    // First search for the header
    for (int i = 0; i < headers->count; i++) {
        if (strcmp(headers->headers[i]->key, key) == 0) {
            // Header already exists, just update the value
            free(headers->headers[i]->value) headers->headers[i]->value =
                strdup(value);
            return;
        }
    }
    // Header does not exist, add it
    // Realloc for additional headers
    if (headers->count >= headers->size) {
        headers->size *= 2;
        headers->headers =
            realloc(headers->headers, sizeof(http_header_t *) * headers->size);
    }
    http_header_t *header = headers->headers[n] = malloc(sizeof(http_header_t));
    header->key                                 = strdup(key);
    header->value                               = strdup(val);
    headers->count++;
}
