#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include "pool_allocator.h"

//STATUS FLAGS
#define PROC_STATE_RUNNING  (1 << 0)
#define PROC_STATE_KILLED   (1 << 1)
#define PROC_STATE_EXITED   (1 << 2)
#define PROC_STATE_READY    (1 << 3)
#define PROC_STATE_WAITING  (1 << 4)
#define PROC_STATE_DETACHED (1 << 5)
#define PROC_STATE_SLEEPING (1 << 6)
// 1 more bit

#define true 1
#define false 0
#define bool uint8_t


typedef void* (*Entry_func)(void* args);


typedef struct  Process{
    uint8_t parent_pid;
    uint8_t status; //bitfield
    uint8_t priority;

    Entry_func func;
    void* args;
    void* ret;

    uint32_t* stack_ptr;
    uint32_t* mem_base;
    Allocator mem_size;
} Process;


typedef struct Process_form{
    Entry_func func;
    uint8_t priority; // >= Current process priority?
    Allocator process_memory_allocator;
    void* args;
    void* ret;
} Process_form;

typedef struct __attribute__((packed)) Interrupt_stack_frame {
    uint32_t r0;     // lowest <---- SP after interrupt
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t LR;
    uint32_t PC;
    uint32_t xPSR;   // highest
    // xPSR : [31 NZCVQ 27 | 26 ICI/IT 25| 24 T | 23 Reserved 16 | 15 ICI/IT   10|9|8 ISR Number 0]
    // Old SP---->
    // ^ Stored when invoking exception
} Interrupt_stack_frame;

typedef struct __attribute__((packed)) Context_stack_frame{
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t exc_return;

    uint32_t r0;     //<---- SP after interrupt
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t LR;
    uint32_t PC;
    uint32_t xPSR;   // highest
    // xPSR : [31 NZCVQ 27 | 26 ICI/IT 25| 24 T | 23 Reserved 16 | 15 ICI/IT   10|9|8 ISR Number 0]
    // Old SP---->
    // ^ Stored when invoking exception
}Context_stack_frame;

_Static_assert(sizeof(Interrupt_stack_frame) % 4 == 0, "Stack_frame struct not 4-byte aligned");
_Static_assert(sizeof(Context_stack_frame) % 4 == 0, "Stack_frame struct not 4-byte aligned");


void process_scheduler_init(void);

void process_scheduler_start(void);
int32_t exec(Process_form* form);

bool detach(uint8_t pid);

void wait(uint8_t pid);

void sleep_ticks(uint32_t ticks);

void yield(void);

#endif
