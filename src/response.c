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
 * @brief Receive a response from the connection
 *
 * @param connection Connection to receive from
 * @return response_t* Response. Must be freed. NULL on failure.
 */
response_t *response_recv(connection_t *connection) {
    // Get the response
    response_t *response = malloc(sizeof(response_t));
    memset(response, 0, sizeof(response_t));
    http_message_t *response_message = http_message_recv(connection);
    if (response_message == NULL) {
        fprintf(stderr, "Could not get response message\n");
        free(response);
        return NULL;
    }
    response->message = response_message;
    response_header_parse(response);

    return response;
}

/**
 * @brief Send the request to the server and get the response
 *
 * @param request Request to send
 * @param response_dest File to write response to
 * @return response_t* Response from server
 */
response_t *response_fetch(request_t *request) {
    // Open a connection to the server
    connection_t *server_connection =
        connect_to_hostname(request->host, request->port);
    if (server_connection == NULL) {
        fprintf(stderr, "Could not connect to server\n");
        return NULL;
    }

    // Send the message
    request_send(request, server_connection);

    // Get the response
    response_t *response = response_recv(server_connection);

    // Close the connection
    close_connection(server_connection);

    return response;
}

/**
 * @brief Send the response to the client
 *
 * @param response Response to send
 * @param connection Connection
 * @return int 0 on success, -1 on failure
 */
int response_send(response_t *response, connection_t *connection) {
    int rv;
    // create the response message
    http_message_t *message = response->message;
    char            header_line[4096];
    snprintf(header_line, 4096, "%s %d %s\r\n", response->version,
             response->status_code, response->reason);
    DEBUG_PRINT("--> %s", header_line);
    http_message_set_header_line(message, header_line);
    rv = http_message_send(message, connection);
    if (rv != 0) {
        fprintf(stderr, "Could not send message\n");
        return -1;
    }
    return rv;
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

    char *header_line = http_message_get_header_line(response->message);

    // Execute the regex
    regmatch_t match[4];
    reti = regexec(&regex, header_line, 4, match, 0);
    if (reti == REG_NOMATCH) {
        fprintf(stderr, "Could not match regex\n");
        regfree(&regex);
        return -1;
    }

    // Get the version (if it exists)
    if (match[1].rm_so != -1) {
        response->version = strndup(header_line + match[1].rm_so,
                                    match[1].rm_eo - match[1].rm_so);
    }

    // Get the status code
    char *status_code_str =
        strndup(header_line + match[2].rm_so, match[2].rm_eo - match[2].rm_so);
    response->status_code = atoi(status_code_str);
    free(status_code_str);

    // Get the reason
    response->reason =
        strndup(header_line + match[3].rm_so, match[3].rm_eo - match[3].rm_so);

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
    free(response);
}

/**
 * @brief Create a response from a message
 *
 * @param message Message
 *
 * @return response_t* Response
 */
response_t *response_parse(http_message_t *message) {
    response_t *response = malloc(sizeof(response_t));
    memset(response, 0, sizeof(response_t));
    response->message = message;
    int rv            = response_header_parse(response);
    if (rv != 0) {
        fprintf(stderr, "Could not parse response\n");
        response_free(response);
        return NULL;
    }
    return response;
}

/**
 * @brief Create a response
 *
 * @param status_code Status code
 * @param reason Reason
 * @return response_t* Response
 */
response_t *response_create(int status_code, char *reason) {
    response_t *response = malloc(sizeof(response_t));
    memset(response, 0, sizeof(response_t));
    http_message_t *message = http_message_create();
    if (message == NULL) {
        fprintf(stderr, "Could not create message\n");
        free(response);
        return NULL;
    }
    response->message     = message;
    response->status_code = status_code;
    response->reason      = strdup(reason);
    response->version     = strdup("HTTP/1.1");
    return response;
}

/**
 * @brief Set the response body
 *
 * @param response Response to set body of
 * @param body Body to set
 * @param len Length of body
 *
 * @return int 0 on success, -1 on failure
 */
int response_set_body(response_t *response, char *body, size_t len) {
    if (response->message == NULL) {
        fprintf(stderr, "Response message is null\n");
        return -1;
    }
    http_message_set_body(response->message, body, len);
    return 0;
}

/**
 * @brief Set the response body
 *
 * @param response Response to set body of
 * @param f File to read from
 *
 * @return int 0 on success, -1 on failure
 */
int response_set_body_f(response_t *response, FILE *f) {
    if (response->message == NULL) {
        fprintf(stderr, "Response message is null\n");
        return -1;
    }
    http_message_set_body_f(response->message, f);
    return 0;
}

/**
 * @brief Send an error response
 * 
 * @param connection Connection to send response on
 * @param status_code Status code
 * @param reason Reason
 * 
 * @return int 0 on success, -1 on failure
*/
int response_send_error(connection_t *connection, int status_code, char *reason) {
    response_t *response = response_create(status_code, reason);
    if (response == NULL) {
        fprintf(stderr, "Could not create response\n");
        return -1;
    }
    response_set_body(response, reason, strlen(reason));
    int rv = response_send(response, connection);
    if (rv != 0) {
        fprintf(stderr, "Could not send response\n");
        response_free(response);
        return -1;
    }
    response_free(response);
    return 0;
}
