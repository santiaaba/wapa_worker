#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>

#ifndef SEND_RECEIVE_H
#define SEND_RECEIVE_H

#define PORT 3550 /* El puerto que será abierto */
#define BACKLOG 1 /* El número de conexiones permitidas */
#define ROLE_BUFFER_SIZE 25
#define ROLE_HEADER_SIZE 8

int recv_all_message(int fd_client, char **rcv_message, uint32_t *rcv_message_size);
int send_all_message(int fd_client, char *send_message, uint32_t send_message_size);

#endif
