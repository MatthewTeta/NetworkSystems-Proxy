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
#include <sys/sendfile.h>

#include "debug.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))

http_message_t *http_message_recv(connection_t *connection) {
    http_message_t *message = malloc(sizeof(http_message_t));
    memset(message, 0, sizeof(http_message_t));
    // // Open a stream to write the message into
    // message->fp = open_memstream(&message->message, &message->message_len);
    // DEBUG_PRINT("FP: %p\n", message->fp);
    // if (message->fp == NULL) {
    //     fprintf(stderr, "Error opening memstream in http_message_recv:
    //     FP=%p\n",
    //             message->fp);
    // }

    // Read the message into the
    ssize_t bytes_read;
    int     header_complete = 0;
    // Poll vars
    short         revents;
    struct pollfd fds;
    int           rv;
    // Use sendfile to copy the message into the stream
    while (!header_complete) {
        DEBUG_PRINT("message->message_len: %ld\n", message->message_size);
        message->message_size += MESSAGE_CHUNK_SIZE;
        DEBUG_PRINT("message->message_len: %ld\n", message->message_size);
        if (message->message == NULL)
            message->message = malloc(message->message_size);
        else
            message->message = realloc(message->message, message->message_size);
        DEBUG_PRINT("Waiting for data...\n");
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
            http_message_free(message);
            return NULL;
        } else if (rv == 0) {
            // Timeout -> close connection
            DEBUG_PRINT("Timeout occured in http_message_recv()\n");
            http_message_free(message);
            return NULL;
        }
        // We got data before the timeout, read it
        bytes_read =
            recv(connection->clientfd, message->message + message->message_len,
                 MESSAGE_CHUNK_SIZE, 0);
        DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
        if (bytes_read == 0) {
            // client socket closed
            DEBUG_PRINT("Client socket closed.\n");
            http_message_free(message);
            return NULL;
        } else if (bytes_read < 0) {
            // error reading from client socket
            DEBUG_PRINT("Error reading from client socket.\n");
            http_message_free(message);
            return NULL;
        }
        message->message_len += bytes_read;
        // DEBUG_PRINT("Writing %ld bytes to stream.\n", bytes_read);
        // fwrite(recv_buffer, sizeof(char), bytes_read, message->fp) c;
        // DEBUG_PRINT("Checking if header is complete.\n");
        char *rv = message->body = strstr(message->message, "\r\n\r\n");
        DEBUG_PRINT("strstr \\r\\n\\r\\n RV: %p\n", rv);
        if ((rv) != NULL) {
            message->body += 4;
            message->header_len = message->body - message->message;
            header_complete     = 1;
        }
    }
    if (!header_complete) {
        DEBUG_PRINT("Header is not complete.\n");
        // Send a 400 Bad message response
        DEBUG_PRINT("Sending 400 Bad message response.\n");
        // response_send(connection->clientfd, 400, "Bad message", NULL, 0);
        http_message_free(message);
        return NULL;
    }
    DEBUG_PRINT("HEADER_COMPLETE\n");
    // At this point, we have recieved the entire header and a partial body.
    // We need to parse the header to get the content length, and then read
    // the rest of the body.
    // Parse the message
    message->headers = http_headers_parse(message);
    if (message->headers == NULL) {
        DEBUG_PRINT("Error parsing message header.\n");
        // Send a 400 Bad message response
        // response_send(connection->clientfd, 400, "Bad message", NULL, 0);
        http_message_free(message);
        return NULL;
    }

    // Get the body length
    char *body_length = http_headers_get(message->headers, "Content-Length");
    DEBUG_PRINT("Body length: %s\n", body_length);
    if (body_length != NULL) {
        message->body_len = atoi(body_length);
    }
    DEBUG_PRINT("Body length: %lu\n", message->body_len);
    // TODO: Check if the body is too long
    // if (message->body_len > HTTP_MAX_BODY_LEN) {
    //     DEBUG_PRINT("Body is too long.\n");
    //     // Send a 400 Bad message response
    //     // response_send(connection->clientfd, 400, "Bad message", NULL, 0);
    //     http_message_free(message);
    //     return NULL;
    // }
    // Allocate space for the body
    if (message->body_len > 0) {
        message->message_size +=
            ((message->body_len / MESSAGE_CHUNK_SIZE) + 1) * MESSAGE_CHUNK_SIZE;
        DEBUG_PRINT("message->message_len: %ld\n", message->message_size);
        message->message = realloc(message->message, message->message_size);
    }

    if (message->body_len > 0) {
        int body_length = message->message_len - message->body_len;
        if (body_length < message->body_len) {
            DEBUG_PRINT("Reading the rest of the body.\n");
        } else if (body_length > message->body_len) {
            DEBUG_PRINT("Body is longer than content length.\n");
        }
        while (body_length < message->body_len) {
            // Use recv to copy the message into the stream
            size_t recv_len =
                min(MESSAGE_CHUNK_SIZE, message->body_len - body_length);
            bytes_read =
                recv(connection->clientfd,
                     message->message + message->message_len, recv_len, 0);
            DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
            if (bytes_read == 0) {
                DEBUG_PRINT("Client socket closed.\n");
                http_message_free(message);
                return NULL;
            } else if (bytes_read < 0) {
                DEBUG_PRINT("Error reading from client socket.\n");
                http_message_free(message);
                return NULL;
            }
            message->message_len += bytes_read;
            body_length += bytes_read;
        }
        // fflush(message->fp);
    }

    return message;
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
    // if (message->fp != NULL) {
    //     fclose(message->fp);
    // }
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
 * @details Parses HTTP headers from a partial http message (body is not
 * complete).
 *
 * @param message HTTP message
 * @return http_headers_t* Parsed headers
 */
http_headers_t *http_headers_parse(http_message_t *message) {
    // Set the header line which will be skipped
    message->header_line    = message->message;
    http_headers_t *headers = malloc(sizeof(http_headers_t));
    headers->headers =
        malloc(sizeof(http_header_t *) * HTTP_HEADER_COUNT_DEFAULT);
    headers->size = HTTP_HEADER_COUNT_DEFAULT;
    char *msg     = message->message;
    char *line    = strtok(msg, "\r\n");
    int   i       = 0;
    while (line != NULL) {
        DEBUG_PRINT("Line %d: %s\n", i, line);
        if (i == 0) {
            // Skip the first line (request line)
            line = strtok(NULL, "\r\n");
            i++;
            continue;
        }
        if (strcmp(line, "\r\n") == 0) {
            // End of headers
            break;
        }
        char *line_copy = strdup(line); // make a copy of line
        char *key       = strtok(line_copy, ":");
        char *val       = strtok(NULL, "");
        // remove leading whitespace from val
        while (isspace(*val)) {
            val++;
        }
        if (key == NULL || val == NULL) {
            // Malformed header
            DEBUG_PRINT("Malformed header: %s\n", line);
            free(line_copy); // free the copy of line
            http_headers_free(headers);
            return NULL;
            // continue;
        }
        http_headers_set(headers, key, val);
        free(line_copy); // free the copy of line
        line = strtok(NULL, "\r\n");
        i++;
    }

    return headers;
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
    // // First search for the header
    // for (int i = 0; i < headers->count; i++) {
    //     if (strcmp(headers->headers[i]->key, key) == 0) {
    //         // Header already exists, just update the value
    //         free(headers->headers[i]->value);
    //         headers->headers[i]->value = strdup(value);
    //         return;
    //     }
    // }
    // Header does not exist, add it
    DEBUG_PRINT("Adding header: %s: %s\n", key, value);
    // Realloc for additional headers
    if (headers->count >= headers->size) {
        headers->size *= 2;
        headers->headers =
            realloc(headers->headers, sizeof(http_header_t *) * headers->size);
    }
    http_header_t *header = headers->headers[headers->count] =
        malloc(sizeof(http_header_t));
    DEBUG_PRINT("Allocated header at %p\n", header);
    header->key                      = strdup(key);
    header->value                    = strdup(value);
    headers->headers[headers->count] = header;
    headers->count++;
}
