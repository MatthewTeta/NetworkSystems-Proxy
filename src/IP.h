/**
 * @file IP.h
 * @brief IP helper functions
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @version 0.1
 * @date 2023-104-14
 * 
 * @copyright Copyright (c) 2023
*/

#ifndef IP_H
#define IP_H

#include <sys/types.h>

/**
 * @brief Convert a hostname (or IP address) to an IP address
 *
 * @param host Host / IP string
 * @param ipstr IP string (output)
 * @param ipstr_len Length of ipstr buffer
 */
void host_to_ip(const char *host, char *ipstr, size_t ipstr_len);

#endif
