#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

//STATUS FLAGS
#define PROC_STATE_RUNNING  (1 << 0)
#define PROC_STATE_KILLED   (1 << 1)
#define PROC_STATE_EXITED   (1 << 2)
#define PROC_STATE_READY    (1 << 3)
#define PROC_STATE_WAITING  (1 << 4)
#define PROC_STATE_DETACHED (1 << 5)
#define PROC_STATE_FREE     (1 << 6)
#define PROC_WILL_RETURN    (1 << 7)

#define true 1
#define false 0
#define bool uint8_t


typedef void* (*Entry_func)(void* args);


typedef struct __attribute__((packed)) Process{
    uint8_t parent_pid;
    uint8_t status; //bitfield
    uint8_t priority;
    uint8_t unused;

    Entry_func func;
    void* ret;

    uint32_t* stack_ptr;
    uint32_t* stack_base;
} Process;

_Static_assert(sizeof(Process) % 4 == 0, "Process struct not 4-byte aligned");


typedef struct __attribute__((packed)) Stack_frame{
    // Stored when invoking exception
    // Old SP---->
    uint32_t xPSR;
    uint32_t PC;
    uint32_t LR;
    uint32_t r12;
    uint32_t r3;
    uint32_t r2;
    uint32_t r1;
    uint32_t r0; // <---- SP after interrupt

    // Stored when context switching
    uint32_t r11;
    uint32_t r10;
    uint32_t r9;
    uint32_t r8;
    uint32_t r7;
    uint32_t r6;
    uint32_t r5;
    uint32_t r4;
}Stack_frame;

_Static_assert(sizeof(Stack_frame) % 4 == 0, "Stack_frame struct not 4-byte aligned");


uint8_t exec(Entry_func func);

bool detach(uint8_t pid);

void wait(uint8_t pid);

#endif
