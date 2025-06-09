#include "process.h"
#include <stdint.h>
#include <stddef.h>

#define true 1
#define false 0
typedef uint8_t bool


#define PROC_PID_QUEUE_SIZE 256
#define PROC_PID_BITFIELD_SIZE (PROC_PID_QUEUE_SIZE / 8)

static uint8_t free_list[PROC_PID_BITFIELD_SIZE];


typedef struct Pid_queue{
    uint8_t head;
    uint8_t tail;
    bool full;
    uint8_t buffer[PROC_PID_QUEUE_SIZE];
}Pid_queue;

static Pid_queue run_q_0;
static Pid_queue run_q_1;
static Pid_queue run_q_2;

static Pid_queue wait_q;

static bool pid_enqueue(Pid_queue* queue, uint8_t pid){
    if(queue->full) return false;
    queue->buffer[queue->head] = pid;
    queue->head = (queue->head + 1) & (PROC_PID_QUEUE_SIZE - 1);
    queue->full = false;
    return true;
}

static uint8_t pid_dequeue(Pid_queue* queue){
    uint8_t ret = queue->tail;
   queue->tail = (queue->tail + 1) & (PROC_PID_QUEUE_SIZE - 1);
   queue->full = false;
   return ret;
}

static void Pid_queue_init(Pid_queue* queue){
    queue->head = 0;
    queue->head = 0;
    queue->full = false;
}

static Process processes[256];
