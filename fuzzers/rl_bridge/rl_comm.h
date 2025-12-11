#ifndef RL_COMM_H
#define RL_COMM_H

#include<stddef.h>
#include<stdint.h>

int rl_comm_init(const char* path);

int rl_send_state(const float* state, size_t vector_size);

int rl_receive_action(int* action, size_t buffer_len);

void rl_comm_close();
#endif
