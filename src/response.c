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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "http.h"
#include "request.h"

/**
 * @brief Send the request to the server and get the response
 *
 * @param request Request to send
 * @param response_dest File to write response to
 * @return response_t* Response from server
 */
response_t *response_recv(request_t *request, FILE *response_dest) {
    // Open a connection to the server
    connection_t *connection = connection_init(request->host, request->port);
    if (connection == NULL) {
        fprintf(stderr, "Could not connect to server\n");
        close_connection(connection);
        return NULL;
    }

    // Send the request
    // Seek to the beginning of the request
    int request_fd = fileno(request->request_fp);
    lseek(request_fd, 0, SEEK_SET);
    ssize_t nsent;
    size_t  ntot = 0;
    while (ntot < request->request_len) {
        nsent = sendfile(connection->clientfd, request_fd, NULL, request->request_len - ntot);
        if (nsent < 0) {
            fprintf(stderr, "Could not send request\n");
            return NULL;
        } else if (nsent == 0) {
            DEBUG_PRINT("Sent 0 bytes in response_recv!!\n");
        }
        ntot += nsent;
    }


    // Get the response
    response_t *response = response_parse(connection->clientfd);
}

/**
 * @brief Parse a response
 *
 * @param response_str Response string
 * @return response_t* Parsed response
 */
response_t *response_parse(char *response_str);

/**
 * @brief Free a response
 *
 * @param response Response to free
 */
void response_free(response_t *response);
