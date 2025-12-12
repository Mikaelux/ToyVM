#include "rl_comm.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int rl_sock = -1;

int rl_comm_init(const char* server_path) {
    if (!server_path) return -1;

    rl_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (rl_sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, server_path, sizeof(addr.sun_path) - 1);

    if (connect(rl_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(rl_sock);
        rl_sock = -1;
        return -1;
    }

    return 0;
}

int rl_send_state(const float* vector, uint32_t vector_size) {
    if (rl_sock < 0 || !vector || vector_size == 0) return -1;
    
  //send indicator to show state 
    uint8_t type = 0;
    if(send(rl_sock, &type, 1, 0) != 1){
      perror("send type");
      return -1;
    }

    if(send(rl_sock, &vector_size, sizeof(uint32_t), 0) != sizeof(uint32_t)){
      perror("send size");
      return -1;
    }
    size_t bytes_to_send = vector_size * sizeof(float);
    size_t total_sent = 0;

    while (total_sent < bytes_to_send) {
        ssize_t sent = send(rl_sock, ((const char*)vector) + total_sent, bytes_to_send - total_sent, 0);
        if (sent <= 0) {
            perror("send data");
            return -1;
        }
        total_sent += sent;
    }

    return 0;
}


int rl_send_reward(float reward) {
    if (rl_sock < 0) return -1;

    uint8_t type = 1;
    size_t total_sent = 0;
    ssize_t sent;

    // send type
    while (total_sent < 1) {
        sent = send(rl_sock, ((char*)&type) + total_sent, 1 - total_sent, 0);
        if (sent <= 0) {
            if(errno == EINTR) continue;
            fprintf(stderr, "rl_send_reward: send(type) failed: %d %s\n", errno, strerror(errno));
            return -1;
        }
        total_sent += sent;
    }

    // send reward
    total_sent = 0;
    size_t bytes = sizeof(float);
    while (total_sent < bytes) {
        sent = send(rl_sock, ((char*)&reward) + total_sent, bytes - total_sent, 0);
        if (sent <= 0) {
            if(errno == EINTR) continue;
            fprintf(stderr, "rl_send_reward: send(reward) failed: %d %s\n", errno, strerror(errno));
            return -1;
        }
        total_sent += sent;
    }

    return 0;
}


int rl_recv_action(int* action_buffer, size_t buffer_len) {
    if (rl_sock < 0 || !action_buffer || buffer_len == 0) return -1;

    size_t total_read = 0;
    size_t bytes_to_read = buffer_len * sizeof(int);

    while (total_read < bytes_to_read) {
        ssize_t n = recv(rl_sock, ((char*)action_buffer) + total_read, bytes_to_read - total_read, 0);
        if (n < 0) {
            if(errno == EINTR) continue;
            perror("recv");
            return -1;
        } else if (n == 0) {
            return 0;
        }
        total_read += (size_t)n;
    }

    return (int)(total_read / sizeof(int));
}

void rl_comm_close() {
    if (rl_sock >= 0) {
        close(rl_sock);
        rl_sock = -1;
    }
}

