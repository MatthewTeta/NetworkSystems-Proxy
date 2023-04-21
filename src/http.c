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
#define max(a, b) (((a) > (b)) ? (a) : (b))

// Private structs

/**
 * @brief HTTP header structure
 *
 */
typedef struct http_header {
    char *key;   // Header key
    char *value; // Header value
} http_header_t;

/**
 * @brief HTTP headers structure
 *
 */
typedef struct http_headers {
    int            size;    // Size of array
    int            count;   // Number of headers in array
    http_header_t *headers; // Array of headers
} http_headers_t;

// Private functions
void http_headers_parse(http_message_t *message);
int  http_headers_send(http_headers_t *headers, connection_t *connection);
void http_message_header_set_(http_message_t *message, char *key, char *value,
                              int search);
http_header_t *http_message_header_search(http_message_t *message,
                                          const char *key, int *index);

/**
 * @brief HTTP message structure
 *
 * @note This structure is used for both requests and responses
 */
struct http_message {
    char           *message;      // message buffer
    size_t          message_size; // message buffer size
    size_t          message_len;  // message string length
    char           *header_line;  // message line
    size_t          header_len;   // message header length
    char           *body;         // message body
    size_t          body_len;     // message body length
    http_headers_t *headers;      // message headers
};

// Private functions
void http_headers_free(http_headers_t *headers);

// Global functions

/**
 * @brief Create a new HTTP message
 *
 * @param data Message data
 * @param size Message size
 * @return http_message_t* HTTP message
 */
http_message_t *http_message_create(char *data, size_t size) {
    http_message_t *message = malloc(sizeof(http_message_t));
    memset(message, 0, sizeof(http_message_t));
    message->message      = data;
    message->message_size = size;
    message->message_len  = size;
    return message;
}

http_message_t *http_message_recv(connection_t *connection) {
    http_message_t *message = malloc(sizeof(http_message_t));
    memset(message, 0, sizeof(http_message_t));

    // Read the message into the message buffer
    ssize_t bytes_read;
    int     header_complete = 0;
    // Poll vars
    short         revents;
    struct pollfd fds;
    int           rv;
    // Use sendfile to copy the message into the stream
    while (!header_complete) {
        if (message->message_size > HTTP_MESSAGE_MAX_HEADER_SIZE) {
            // Message is too large, close the connection
            DEBUG_PRINT("Message is too large, closing connection\n");
            http_message_free(message);
            return NULL;
        }
        // DEBUG_PRINT("message->message_len: %ld\n", message->message_size);
        message->message_size += MESSAGE_CHUNK_SIZE;
        // DEBUG_PRINT("message->message_len: %ld\n", message->message_size);
        if (message->message == NULL)
            message->message = malloc(message->message_size);
        else
            message->message = realloc(message->message, message->message_size);
        // DEBUG_PRINT("Waiting for data...\n");
        // Set up a poll to timeout if we don't get any data
        // Poll the socket for data until we get something or a
        // timeout is recieved
        revents     = 0;
        fds.fd      = connection->fd;
        fds.events  = POLLIN;
        fds.revents = revents;
        rv          = poll(&fds, 1, KEEP_ALIVE_TIMEOUT_MS);
        if (rv < 0) {
            // This will fail if the parent recieves a SIGINT
            // This is fine, check if nread is 0 to see if we have
            // received any data If we have received data, continue
            // processing the message If we have not received any
            // data, exit the child process
            if (bytes_read == 0) {
                DEBUG_PRINT("Poll failed, exiting child process\n");
                http_message_free(message);
                return NULL;
            } else {
                DEBUG_PRINT("Poll failed, but we have received data\n");
            }
        } else if (rv == 0) {
            // Timeout -> close connection
            DEBUG_PRINT("Timeout occured in http_message_recv()\n");
            http_message_free(message);
            return NULL;
        }
        // We got data before the timeout, read it
        bytes_read =
            recv(connection->fd, message->message + message->message_len,
                 MESSAGE_CHUNK_SIZE, 0);
        // DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
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

        // Slight optimization causes the strstr to only search the last 4 bytes
        // before
        // size_t search_start = max(0, message->message_len - 4);
        size_t search_start = 0;
        message->body = strstr(message->message + search_start, "\r\n\r\n");
        // DEBUG_PRINT("strstr \\r\\n\\r\\n RV: %p\n", rv);
        if (message->body != NULL) {
            message->body += 4;
            message->header_len = message->body - message->message;
            // Exit hte loop
            header_complete = 1;
        }
    }
    // At this point, we have recieved the entire header and a partial body.
    // We need to parse the header to get the content length, and then read
    // the rest of the body.
    // Parse the message
    http_headers_parse(message);
    if (message->headers == NULL) {
        DEBUG_PRINT("Error parsing message header.\n");
        // Send a 400 Bad message response
        // response_send(connection->fd, 400, "Bad message", NULL, 0);
        http_message_free(message);
        return NULL;
    }

    // Get the body length
    char *body_length = http_message_header_get(message, "Content-Length");
    // DEBUG_PRINT("Body length: %s\n", body_length);
    if (body_length != NULL) {
        message->body_len = atoi(body_length);
    } else {
        message->body_len = 0;
        http_message_header_set(message, "Content-Length", "0");
    }
    DEBUG_PRINT("Header length: %lu, Body length: %lu, (a+b)(%lu), Message "
                "len: %lu, Message size: %lu\n",
                message->header_len, message->body_len,
                message->header_len + message->body_len, message->message_len,
                message->message_size);
    // TODO: Check if the body is too long
    // if (message->body_len > HTTP_MAX_BODY_LEN) {
    //     DEBUG_PRINT("Body is too long.\n");
    //     // Send a 400 Bad message response
    //     // response_send(connection->fd, 400, "Bad message", NULL, 0);
    //     http_message_free(message);
    //     return NULL;
    // }
    // Allocate space for the body
    if (message->body_len > 0) {
        message->message_size +=
            ((message->header_len + message->body_len) / MESSAGE_CHUNK_SIZE) *
            MESSAGE_CHUNK_SIZE;
        DEBUG_PRINT("message->message_size: %ld\n", message->message_size);
        message->message = realloc(message->message, message->message_size);
        // memset(message->message + message->message_len, 0,
        //        message->message_size - message->message_len);
    }

    if (message->body_len > 0) {
        int read_remaining =
            message->body_len + message->header_len - message->message_len;
        if (read_remaining < 0) {
            DEBUG_PRINT("Body is longer than content length.\n");
            http_message_free(message);
            return NULL;
        }
        // if (read_remaining < message->body_len) {
        //     DEBUG_PRINT("Reading the rest of the body.\n");
        // } else if (read_remaining > message->body_len) {
        //     DEBUG_PRINT("Body is longer than content length.\n");
        // }
        while (read_remaining) {
            // DEBUG_PRINT("Reading %d bytes from socket.\n", read_remaining);
            // Use recv to copy the message into the stream
            size_t recv_len = min(MESSAGE_CHUNK_SIZE, read_remaining);
            // DEBUG_PRINT("Reading %ld bytes from socket.\n", recv_len);
            bytes_read =
                recv(connection->fd, message->message + message->message_len,
                     recv_len, 0);
            // DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
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
            read_remaining -= bytes_read;
        }
        // fflush(message->fp);
    }

    // DEBUG_PRINT("\n\nRead body (%lu):\n", message->body_len);
    // DEBUG_PRINT("%s\n\n", message->body);

    return message;
}

/**
 * @brief Send an http message to the socket including the header line, headers,
 * and body. This will reconstruct the message from the headers and body.
 *
 * @param message The message to send
 */
int http_message_send(http_message_t *message, connection_t *connection) {
    if (message == NULL || message->headers == NULL) {
        return -1;
    }

    // Get the body length
    char *body_length = http_message_header_get(message, "Content-Length");
    // DEBUG_PRINT("Body length: %s\n", body_length);
    if (body_length != NULL) {
        message->body_len = atoi(body_length);
    } else {
        message->body_len = 0;
        http_message_header_set(message, "Content-Length", "0");
    }

    // Send the header line
    send_to_connection(connection, message->header_line,
                       strlen(message->header_line));
    // Send the headers
    http_headers_send(message->headers, connection);
    // Send the body
    if (message->body_len > 0) {
        send_to_connection(connection, message->body, message->body_len);
    }
    return 0;
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
    free(message->header_line);
    if (message->headers != NULL) {
        http_headers_free(message->headers);
    }
    if (message->message != NULL) {
        free(message->message);
    }
    free(message);
}

// Private functions

/**
 * @brief Parse HTTP headers
 * @details Parses HTTP headers from a partial http message (body is not
 * complete).
 *
 * @param message HTTP message
 * @return http_headers_t* Parsed headers
 */
void http_headers_parse(http_message_t *message) {
    // Allocate a new headers struct (vector<http_header_t>)
    message->headers        = malloc(sizeof(http_headers_t));
    http_headers_t *headers = message->headers;
    // Allocate space for the headers
    headers->headers =
        malloc(sizeof(http_header_t) * HTTP_HEADER_COUNT_DEFAULT);
    headers->count = 0;
    headers->size  = HTTP_HEADER_COUNT_DEFAULT;
    // Make a copy of the header buffer so we can modify it
    char *header_buf = malloc(message->header_len + 1);
    memcpy(header_buf, message->message, message->header_len);
    header_buf[message->header_len] = '\0';
    // Parse the headers
    char *saveptr1, *saveptr2;
    char *msg  = header_buf;
    char *line = strtok_r(msg, "\r\n", &saveptr1);
    int   i    = 0;
    while (line != NULL) {
        // DEBUG_PRINT("Line %d: %s\n", i, line);
        if (i == 0) {
            // Skip the first line (request line)
            message->header_line = strdup(line);
            line                 = strtok_r(NULL, "\r\n", &saveptr1);
            i++;
            continue;
        }
        if (strcmp(line, "\r\n") == 0) {
            // End of headers
            break;
        }
        char *line_copy = strdup(line); // make a copy of line
        char *key       = strtok_r(line_copy, ":", &saveptr2);
        char *val       = strtok_r(NULL, "", &saveptr2);
        // remove leading whitespace from val
        while (isspace(*val)) {
            val++;
        }
        if (key == NULL || val == NULL) {
            // Malformed header
            DEBUG_PRINT("Malformed header: %s\n", line);
            free(line_copy); // free the copy of line
            http_headers_free(headers);
            free(header_buf);
            return;
            // continue;
        }
        DEBUG_PRINT("HEADER -- %s: %s\n", key, val);
        http_message_header_set_(message, key, val, 0);
        free(line_copy); // free the copy of line
        line = strtok_r(NULL, "\r\n", &saveptr1);
        i++;
    }
    free(header_buf);
}

/**
 * @brief Set the header line from an HTTP message
 *
 * @param message HTTP message
 * @return char* Header line
 */
void http_message_set_header_line(http_message_t *message, char *header_line) {
    message->header_line = strdup(header_line);
}

/**
 * @brief Get the header line from an HTTP message
 *
 * @param message HTTP message
 * @return char* Header line
 */
char *http_message_get_header_line(http_message_t *message) {
    if (message == NULL || message->header_line == NULL) {
        return NULL;
    }
    return message->header_line;
}

/**
 * @brief Set the body from an HTTP message
 *
 * @param message HTTP message
 * @param body Body
 * @param len Length of body
 * @return char* Body
 */
void http_message_set_body(http_message_t *message, char *body, size_t len) {
    message->body     = body;
    message->body_len = len;
    char body_length[16];
    sprintf(body_length, "%zu", len);
    http_message_header_set(message, "Content-Length", body_length);
}

/**
 * @brief Get the body from an HTTP message
 *
 * @param message HTTP message
 * @return char* Body
 */
char *http_message_get_body(http_message_t *message) { return message->body; }

/**
 * @brief Get a header value from a list of headers
 *
 * @param headers Headers
 * @param key Header key
 * @return char* Header value
 */
char *http_message_header_get(http_message_t *message, char *key) {
    // TODO: Use a hash table for faster lookup
    // TODO: Convert to lowercase for case-insensitive comparison
    http_headers_t *headers = message->headers;
    for (int i = 0; i < headers->count; i++) {
        if (strcmp(headers->headers[i].key, key) == 0) {
            return headers->headers[i].value;
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
void http_message_header_set(http_message_t *message, char *key, char *value) {
    http_message_header_set_(message, key, value, 1);
}
/** @param search Search for the header first */
void http_message_header_set_(http_message_t *message, char *key, char *value,
                              int search) {
    http_headers_t *headers = message->headers;
    http_header_t  *header  = NULL;
    // TODO: Use a hash table for faster lookup
    // TODO: Convert to lowercase for case-insensitive comparison
    if (search) {
        // Attempt to update existing header
        header = http_message_header_search(message, key, NULL);
        if (header != NULL) {
            // Header exists, update it
            free(header->value);
            header->value = strdup(value);
            return;
        }
    }
    // Header does not exist, add it
    // Realloc for additional headers
    if (headers->count >= headers->size) {
        headers->size *= 2;
        headers->headers =
            realloc(headers->headers, sizeof(http_header_t) * headers->size);
    }
    header = &headers->headers[headers->count];
    // DEBUG_PRINT("Allocated header at %p\n", header);
    header->key   = strdup(key);
    header->value = strdup(value);
    headers->count++;
}

/**
 * @brief Compare the header with the key to the value provided
 *
 * @param message HTTP message
 * @param key Header key
 * @param value Header value test
 * @return int 0 if equal, -1 if not equal, -2 if no header found
 */
int http_message_header_compare(http_message_t *message, const void *key,
                                const void *value) {
    http_header_t *header = http_message_header_search(message, key, NULL);
    if (header == NULL) {
        return -2;
    }
    return -!(0 == strcmp(header->value, value));
}

/**
 * @brief Remove a header from a list of headers
 *
 * @param message HTTP message
 * @param key Header key
 *
 * @return 0 on success, -1 on failure
 */
int http_message_header_remove(http_message_t *message, char *key) {
    int             i;
    http_header_t  *header  = http_message_header_search(message, key, &i);
    http_headers_t *headers = message->headers;
    if (header == NULL) {
        // Header not found
        return -1;
    }
    // DEBUG_PRINT("Freeing header at %p\n", header);
    free(header->key);
    free(header->value);
    // Shift headers down
    memmove(header, header + 1,
            sizeof(http_header_t) * (headers->count - i - 1));
    return 0;
}

/**
 * @brief Search for a header in a list of headers
 *
 * @param message HTTP message
 * @param key Header key
 * @param index Index of header (output, optional)
 *
 * @return http_header_t* Header if found, NULL if not found
 */
http_header_t *http_message_header_search(http_message_t *message,
                                          const char *key, int *index) {
    http_headers_t *headers = message->headers;
    // DEBUG_PRINT("COUNT: %d\n", headers->count);
    for (int i = 0; i < headers->count; i++) {
        if (headers->headers[i].key == NULL) {
            i++;
            continue;
        }
        // DEBUG_PRINT("Comparing (%d) %s to %s\n", headers->count,
        // headers->headers[i].key, key);
        if (strcmp(headers->headers[i].key, key) == 0) {
            if (index != NULL)
                *index = i;
            // DEBUG_PRINT("Found header at %d\n", i);
            return &headers->headers[i];
        }
    }
    return NULL;
}

/**
 * @brief Print HTTP headers
 *
 * @param message HTTP message
 */
void http_message_headers_print(http_message_t *message) {
    http_headers_t *headers = message->headers;
    for (int i = 0; i < headers->count; i++) {
        if (headers->headers[i].key == NULL) {
            continue;
        }
        printf("%s: %s\n", headers->headers[i].key, headers->headers[i].value);
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
        if (headers->headers[i].key != NULL) {
            free(headers->headers[i].key);
        }
        if (headers->headers[i].value != NULL) {
            free(headers->headers[i].value);
        }
    }
    free(headers->headers);
    free(headers);
}

/**
 * @brief Send HTTP headers
 *
 * @param headers Headers to send
 * @param connection Connection to send headers on
 *
 * @return int 0 on success, -1 on error
 */
int http_headers_send(http_headers_t *headers, connection_t *connection) {
    if (headers == NULL || connection == NULL) {
        return -1;
    }
    int            rv;
    char           s[1024];
    http_header_t *h;
    for (size_t i = 0; i < headers->count; i++) {
        memset(s, 0, sizeof(s));
        h = &headers->headers[i];
        if (h == NULL)
            continue;
        sprintf(s, "%s: %s\r\n", h->key, h->value);
        rv = send_to_connection(connection, s, strlen(s));
    }
    send_to_connection(connection, "\r\n", 2);
    return rv;
}

/**
 * @brief Get the HTTP data buffer from a message
 *
 * @param message HTTP message
 * @param data Data buffer (output)
 * @param size Size of data buffer (output)
 */
void http_get_message_buffer(http_message_t *message, char **data,
                             size_t *size) {
    *data = message->message;
    *size = message->message_size;
}
