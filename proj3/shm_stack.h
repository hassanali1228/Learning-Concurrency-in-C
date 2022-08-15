
/* The code is 
 * Copyright(c) 2018-2019 Yiqing Huang, <yqhuang@uwaterloo.ca>.
 *
 * This software may be freely redistributed under the terms of the X11 License.
 */
/**
 * @brief  stack to push/pop integer(s), API header  
 * @author yqhuang@uwaterloo.ca
 */

typedef struct recv_buf2
{
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

struct recv_stack;

int sizeof_shm_stack(int size);
int init_shm_stack(struct recv_stack *p, int stack_size);
struct recv_stack *create_stack(int size);
void destroy_stack(struct recv_stack *p);
int is_full(struct recv_stack *p);
int is_empty(struct recv_stack *p);
int push(struct recv_stack *p, RECV_BUF item);
int pop(struct recv_stack *p, RECV_BUF *p_item);
