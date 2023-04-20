/**
 * @file IP.test.c
 * @brief Test the IP address functions
 *
 * @author Matthew Teta (matthew.teta@colorado.edu)
 * @version 0.1
 * @date 2023-04-14
 *
 */

#include "IP.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char  ipstr[INET6_ADDRSTRLEN];
    char *hostname = "netsys.cs.colorado.edu";
    hostname_to_ip(hostname, ipstr, INET6_ADDRSTRLEN);
    printf("IP address of %s is %s\n", hostname, ipstr);
    return 0;
}
