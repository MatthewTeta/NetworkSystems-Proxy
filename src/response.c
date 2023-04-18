/**
 * @file response.c
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @brief This deals with https responses. It parses incoming responses and
 * sends outgoing requests.
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 */

#include "response.h"

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include "debug.h"
#include "http.h"
#include "request.h"
#include "server.h"

/**
 * @brief Send the request to the server and get the response
 *
 * @param request Request to send
 * @param response_dest File to write response to
 * @return response_t* Response from server
 */
response_t *response_recv(request_t *request) {
    // Open a connection to the server
    connection_t *server_connection =
        connect_to_hostname(request->host, request->port);
    if (server_connection == NULL) {
        fprintf(stderr, "Could not connect to server\n");
        close_connection(server_connection);
        return NULL;
    }

    // Send the message
    http_message_t *message = request->message;
    // // Seek to the beginning of the message
    // int message_fd = fileno(message->fp);
    // lseek(message_fd, 0, SEEK_SET);
    ssize_t nsent;
    size_t  ntot = 0;
    while (ntot < message->message_len) {
        nsent = send(server_connection->clientfd, message->message + ntot,
                     message->message_len - ntot, 0);
        if (nsent < 0) {
            fprintf(stderr, "Could not send message\n");
            close_connection(server_connection);
            return NULL;
        } else if (nsent == 0) {
            DEBUG_PRINT("Sent 0 bytes in response_recv!!\n");
        }
        ntot += nsent;
    }
    // Message has been sent

    // Get the response
    http_message_t *response_message = http_message_recv(server_connection);
    response_t     *response         = malloc(sizeof(response_t));
    memset(response, 0, sizeof(response_t));
    response->message = response_message;
    response_header_parse(response);

    // Close the connection
    close_connection(server_connection);

    return response;
}

/**
 * @brief Parse a response
 *
 * @param request Request to parse
 * @return int 0 on success, -1 on failure
 */
int response_header_parse(response_t *response) {
    // Get the status line using regex
    // #define RESPONSE_STATUS_REGEX "^(HTTP/[\\d\\.]+)?\\s+(\\d+)\\s+(.*)"

    // Compile the regex
    regex_t regex;
    int     reti = regcomp(&regex, RESPONSE_STATUS_REGEX, REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        regfree(&regex);
        return -1;
    }

    char *status_line = response->message->header_line;

    // Execute the regex
    regmatch_t match[4];
    reti = regexec(&regex, status_line, 4, match, 0);
    if (reti == REG_NOMATCH) {
        fprintf(stderr, "Could not match regex\n");
        regfree(&regex);
        return -1;
    }

    // Get the version (if it exists)
    if (match[1].rm_so != -1) {
        response->version = strndup(status_line + match[1].rm_so,
                                    match[1].rm_eo - match[1].rm_so);
    }

    // Get the status code
    char *status_code_str =
        strndup(status_line + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
    response->status_code = atoi(status_code_str);
    free(status_code_str);

    // Get the reason
    response->reason =
        strndup(status_line + match[3].rm_so, match[3].rm_eo - match[3].rm_so);

    // Free the regex
    regfree(&regex);

    return 0;
}

/**
 * @brief Free a response
 *
 * @param response Response to free
 */
void response_free(response_t *response) {
    if (response == NULL) {
        return;
    }
    if (response->version != NULL) {
        free(response->version);
    }
    if (response->reason != NULL) {
        free(response->reason);
    }
    if (response->message != NULL) {
        http_message_free(response->message);
    }
    if (response->reason != NULL) {
        free(response->reason);
    }
    free(response);
}

/**
 * @brief Send a response to a client
 *
 * @param response Response to send
 * @param clientfd Client socket
 * @return int 0 on success, -1 on failure
 */
int response_send(response_t *response, connection_t *connection) {
    http_message_t *message = response->message;
    // Seek to the beginning of the message
    // int message_fd = fileno(response->message->fp);
    // lseek(message_fd, 0, SEEK_SET);
    ssize_t nsent;
    size_t  ntot = 0;
    while (ntot < response->message->message_len) {
        nsent = recv(connection->clientfd, message->message + ntot,
                     response->message->message_len - ntot, 0);
        if (nsent < 0) {
            fprintf(stderr, "Could not send message\n");
            return -1;
        } else if (nsent == 0) {
            DEBUG_PRINT("Sent 0 bytes in response_send!!\n");
        }
        ntot += nsent;
    }
    return 0;
}
