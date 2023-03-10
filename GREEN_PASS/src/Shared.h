//
// Created by Mattia Danisi on 19/02/23.
//

#ifndef GREEN_PASS_GP_UTILITY_H
#define GREEN_PASS_GP_UTILITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAX_RESPONSE_SIZE 120

//used ports
#define SERVER_V_PORT 1024
#define SERVER_G_PORT 1025
#define CV_PORT 1026

//localhost
#define LOCAL_HOST "127.0.0.1"

#define THREAD_POOL_SIZE 64
#define FILE_NAME "../data/green_pass_serverV.dat"

#define TESSERA_SANITARIA_LEN 20
#define EXPIRATION_DATE_LEN 10
#define SIX_MONTHS 15780000

#define SA struct sockaddr

//service code
#define SERVICE_READ_GP 1
#define SERVICE_VALIDITY_GP 2
#define EXPIRED 0
#define VALID 1
#define NOT_VALID 2
#define NOT_FOUND 3

struct green_pass {
    char card[TESSERA_SANITARIA_LEN + 1];
    time_t expiration_date;
    int service;
};

struct green_pass_check {
    char card[TESSERA_SANITARIA_LEN + 1];
    int service;
};


enum boolean {FALSE, TRUE};

#endif //GREEN_PASS_GP_UTILITY_H
