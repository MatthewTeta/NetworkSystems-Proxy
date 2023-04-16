/**
 * @file request.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 *
 * @brief HTTP Request socket handler and parser
 * @date 2023-04-14
 */

#include "request.h"

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "response.h"

// Private function prototypes
int request_header_parse(request_t *request);

request_t *request_get(connection_t *connection) {
    request_t *request = malloc(sizeof(request_t));
    memset(request, 0, sizeof(request_t));
    // Open a stream to write the request into
    request->request_fp =
        open_memstream(&request->request, &request->request_len);
    int request_fd = fileno(request->request_fp);

    // Poll the socket for data until we get something or a
    // timeout is recieved
    short         revents = 0;
    struct pollfd fds     = {
            .fd      = connection->clientfd,
            .events  = POLLIN,
            .revents = revents,
    };
    int rv = poll(&fds, 1, KEEP_ALIVE_TIMEOUT_MS);
    if (rv < 0) {
        // This will fail if the parent recieves a SIGINT
        // This is fine, check if nread is 0 to see if we have
        // received any data If we have received data, continue
        // processing the request If we have not received any
        // data, exit the child process
        if (nread)
            break;
        DEBUG_PRINT("Poll failed, exiting child process\n");
        request_free(request);
        return NULL;
    } else if (rv == 0) {
        // Timeout -> close connection
        request_free(request);
        return NULL;
    }
    // We got data before the timeout, read it
    if (nread == 0) {
        // Beginning of new header, get the time the header was
        // received This is used to calculate the time it took
        // to receive the header and send the response
        gettimeofday(&time_recv, NULL);
    }

    // Read the request into the stream
    char    buffer[BUFSIZ];
    ssize_t bytes_read;
    int     header_complete = 0;
    // Use sendfile to copy the request into the stream
    while ((bytes_read = sendfile(request_fd, connection->clientfd, NULL,
                                  REQUEST_CHUNK_SIZE)) > 0) {
        DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
        if (bytes_read == 0) {
            DEBUG_PRINT("Client socket closed.\n");
            request_free(request);
            return NULL;
        } else if (bytes_read < 0) {
            DEBUG_PRINT("Error reading from client socket.\n");
            request_free(request);
            return NULL;
        }
        if ((request->body = strstr(request->request, "\r\n\r\n")) != NULL) {
            request->body += 4;
            request->header_len = request->body - request->request;
            header_complete     = 1;
            break;
        }
    }
    if (!header_complete) {
        DEBUG_PRINT("Header is not complete.\n");
        // Send a 400 Bad Request response
        response_send(connection->clientfd, 400, "Bad Request", NULL, 0);
        request_free(request);
        return NULL;
    }
    // At this point, we have recieved the entire header and a partial body.
    // We need to parse the header to get the content length, and then read
    // the rest of the body.
    fflush(request->request_fp);
    // Parse the request
    if (0 != request_header_parse(request)) {
        DEBUG_PRINT("Error parsing request header.\n");
        // Send a 400 Bad Request response
        response_send(connection->clientfd, 400, "Bad Request", NULL, 0);
        request_free(request);
        return NULL;
    }
    if (request->body_len > 0) {
        int body_length = request->request_len - request->body_len;
        if (body_length < content_length_int) {
            DEBUG_PRINT("Reading the rest of the body.\n");
        } else if (body_length > content_length_int) {
            DEBUG_PRINT("Body is longer than content length.\n");
        }
        while (body_length < content_length_int) {
            // Use sendfile to copy the request into the stream
            bytes_read = sendfile(request_fd, connection->clientfd, NULL,
                                  content_length_int - body_length);
            DEBUG_PRINT("Read %ld bytes from client socket.\n", bytes_read);
            if (bytes_read == 0) {
                DEBUG_PRINT("Client socket closed.\n");
                request_free(request);
                return NULL;
            } else if (bytes_read < 0) {
                DEBUG_PRINT("Error reading from client socket.\n");
                request_free(request);
                return NULL;
            }
            body_length += bytes_read;
        }
        fflush(request->request_fp);
    }
}

void request_free(request_t *request) {
    if (request == NULL) {
        return;
    }
    if (request->request_fp != NULL) {
        fclose(request->request_fp);
    }
    if (request->uri != NULL) {
        free(request->uri);
    }
    if (request->host != NULL) {
        free(request->host);
    }
    if (request->method != NULL) {
        free(request->method);
    }
    if (request->version != NULL) {
        free(request->version);
    }
    if (request->headers != NULL) {
        http_headers_free(request->headers);
    }
    if (request->request != NULL) {
        free(request->request);
    }
    free(request);
}

// Private function definitions
int request_header_parse(request_t *request) {
    // Parse the request line using regex
    regex_t     uri_regex;
    regmatch_t  uri_matches[7];
    int         status = regcomp(&uri_regex, REQUEST_URI_REGEX, REG_EXTENDED);
    if (status != 0) {
        DEBUG_PRINT("Error compiling regex.\n");
        return -1;
    }
    status = regexec(&uri_regex, request->request, 7, uri_matches, 0);
    if (status != 0) {
        DEBUG_PRINT("Error parsing request line.\n");
        return -1;
    }
    // Get the method
    request->method = strndup(request->request + uri_matches[1].rm_so,
                              uri_matches[1].rm_eo - uri_matches[1].rm_so);
    // Get http vs https
    if (uri_matches[2].rm_so != -1) {
        if (strncmp(request->request + uri_matches[2].rm_so, "https", 5) == 0) {
            request->https = 1;
        }
    }
    // Get the host
    request->host = strndup(request->request + uri_matches[3].rm_so,
                            uri_matches[3].rm_eo - uri_matches[3].rm_so);
    // Get the port
    if (uri_matches[4].rm_so != -1) {
        request->port =
            strndup(request->request + uri_matches[4].rm_so + 1,
                    uri_matches[4].rm_eo - uri_matches[4].rm_so - 1);
    }
    // Get the uri
    request->uri = strndup(request->request + uri_matches[5].rm_so,
                           uri_matches[5].rm_eo - uri_matches[5].rm_so);
    // Get the http version
    request->http_version =
        strndup(request->request + uri_matches[6].rm_so,
                uri_matches[6].rm_eo - uri_matches[6].rm_so);

    // Parse all of the headers
    request->headers = http_headers_parse(request->request);

    // Get the body length
    char *body_length = http_headers_get(request->headers, "Content-Length");
    if (body_length != NULL) {
        request->body_length = atoi(body_length);
    }

    // Get the Host header
    char *host = http_headers_get(request->headers, "Host");
    if (host != NULL) {
        request->host = strndup(host, strlen(host));
    }

    return 0;
}

/**
 * Returns 1 if the request is a keep-alive connection, 0 otherwise.
 */
int request_is_connection_keep_alive(request_t *request) {
    // Check for the connection header
    if (http_headers_get(request->headers, "Connection") == NULL) {
        // Set the connection header to close
        http_headers_set(request->headers, "Connection", "close");
    } else {
        // Check if the connection header is keep-alive
        if (strcmp(http_headers_get(request->headers, "Connection"),
                   "keep-alive") == 0) {
            return 1;
        }
    }
    return 0;
}
