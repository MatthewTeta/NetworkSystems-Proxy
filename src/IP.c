/**
 * @file IP.c
 * @brief IP contains helper functions for working with IP addresses and hostnames.
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @version 0.1
 * @date 2023-04-14
 * 
 * @copyright Copyright (c) 2023
*/

#include "IP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "debug.h"

// Global variables
regex_t ip_regex, host_regex;

/**
 * @brief Convert a hostname (or IP address) to an IP address
 *
 * @param host Host / IP string
 * @param ipstr IP string (output)
 * @param ipstr_len Length of ipstr buffer
 */
void hostname_to_ip(const char* hostname, char *ipstr, size_t ipstr_len)
{
    struct in_addr ipv4addr;
    memset(ipstr, 0, ipstr_len);

    // Check if input is an IP address
    if (inet_pton(AF_INET, hostname, &ipv4addr) == 1)
    {
        // Convert IP address string to presentation format and return it
        if (inet_ntop(AF_INET, &ipv4addr, ipstr, NI_MAXHOST) == NULL)
        {
            return;
        }
    }
    else
    {
        // Resolve hostname to IP address and return it
        struct hostent *he;
        struct in_addr **addr_list;

        if ((he = gethostbyname(hostname)) == NULL)
        {
            return;
        }

        addr_list = (struct in_addr **)he->h_addr_list;

        for (int i = 0; addr_list[i] != NULL; i++)
        {
            strcpy(ipstr, inet_ntoa(*addr_list[i]));
        }
    }

    strncpy(ipstr, hostname, ipstr_len);
}
// void hostname_to_ip(const char *host, char *ipstr, size_t ipstr_len) {
//     DEBUG_PRINT("Converting Host: %s to IP...\n", host)
//     static int regex_initialized = 0;
//     // Compile the regexs
//     if (!regex_initialized) {
//         regex_initialized = 1;
//         int status;
//         status = regcomp(&ip_regex, IP_REGEX, REG_EXTENDED);
//         if (status) {
//             DEBUG_PRINT("Could not compile ip regex\n");
//             return;
//         }
//         status = regcomp(&host_regex, HOST_REGEX, REG_EXTENDED);
//         if (status) {
//             DEBUG_PRINT("Could not compile host regex\n");
//             return;
//         }
//     }

//     memset(ipstr, 0, ipstr_len);
//     // Check if the host matches the IP regex
//     if (regexec(&ip_regex, host, 0, NULL, 0) == 0) {
//         // Move the host to the ipstr
//         strncpy(ipstr, host, ipstr_len);
//     } else if (regexec(&host_regex, host, 0, NULL, 0) == 0) {
//         // Perform a DNS lookup
//         struct addrinfo hints, *res;
//         memset(&hints, 0, sizeof(hints));
//         hints.ai_family   = AF_INET;
//         hints.ai_socktype = SOCK_STREAM;
//         hints.ai_addr
//         int status        = getaddrinfo(NULL, NULL, &hints, &res);
//         if (status != 0) {
//             DEBUG_PRINT("Could not resolve host: %s\n", host);
//             return;
//         }
//         // Convert to a string
//         struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
//         void               *addr = &(ipv4->sin_addr);
//         if (inet_ntop(res->ai_family, addr, ipstr, ipstr_len) == NULL) {
//             DEBUG_PRINT("Could not convert IP to string\n");
//             return;
//         }
//         // Free the addrinfo
//         freeaddrinfo(res);
//         // Move the host to the ipstr
//         strncpy(ipstr, host, ipstr_len);
//     } else {
//         fprintf(stderr, "WARNING: Invalid host: %s\n", host);
//     }
// }
