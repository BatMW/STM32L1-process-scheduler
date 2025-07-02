#include "stm32l152xc.h"
#include "stm32l1xx.h"
#include "stm32l1xx_hal.h"

#include "process.h"
#include <stdint.h>
#include <stddef.h>
#include "pool_allocator.h"


extern const uint32_t __stack_block_size_s;
extern const uint32_t __stack_block_size_m;
extern const uint32_t __stack_block_size_l;

#define true 1
#define false 0
typedef uint8_t bool;

//TODO: NR_PROC_ALLOWED must be the same as in the .ld, define it during compilation and assert in .ld?
#define NR_PROC_ALLOWED 32
#define PROC_PID_QUEUE_SIZE 16
#define PROC_PID_BITFIELD_SIZE (NR_PROC_ALLOWED / 32)
#define NR_READY_QS 4

#define SET_BITFIELD(bitmap, bit)   ((bitmap)[(bit) >> 5] |=  (1U << ((bit) & 31)))
#define CLEAR_BITFIELD(bitmap, bit) ((bitmap)[(bit) >> 5] &= ~(1U << ((bit) & 31)))
#define TEST_BITFIELD(bitmap, bit)  ((bitmap)[(bit) >> 5] &   (1U << ((bit) & 31)))


#define SCHEDULER_IRQ_PRIORITY  0x80  // NOTE: Blocks priority 8 and above (0–15)

#define CRITICAL_SECTION(code)            \
    do {                                  \
        uint32_t __basepri_orig;          \
        __asm volatile (                  \
            "MRS %0, BASEPRI\n"           \
            "MSR BASEPRI, %1\n"           \
            : "=r" (__basepri_orig)       \
            : "r" (SCHEDULER_IRQ_PRIORITY)\
            : "memory"                    \
        );                                \
        code;                             \
        __asm volatile (                  \
            "MSR BASEPRI, %0\n"           \
            :                             \
            : "r" (__basepri_orig)        \
            : "memory"                    \
        );                                \
    } while (0)



_Static_assert((NR_PROC_ALLOWED & (NR_PROC_ALLOWED - 1)) == 0,
               "NR_PROC_ALLOWED must be a power of two.");
_Static_assert((PROC_PID_QUEUE_SIZE & (PROC_PID_QUEUE_SIZE - 1)) == 0,
               "PROC_PID_QUEUE_SIZE must be a power of two.");

typedef struct Pid_queue{
    uint8_t head;
    uint8_t tail;
    bool full;
    uint8_t buffer[PROC_PID_QUEUE_SIZE];
}Pid_queue;

typedef enum {
    SYSCALL_EXEC,
    SYSCALL_EXIT,
} Syscall_Type;

typedef struct {
    Syscall_Type type;
    void* arg;
} Syscall_Request;


static Pid_queue ready_qs[NR_READY_QS];


static Pid_queue wait_q;

static inline bool pid_enqueue(Pid_queue* queue, uint8_t pid){
    if(queue->full) return false;
    queue->buffer[queue->head] = pid;
    queue->head = (queue->head + 1) & (PROC_PID_QUEUE_SIZE - 1);
    if(queue->head == queue->tail){
        queue->full = true;
    }
    return true;
}

static inline uint8_t pid_dequeue(Pid_queue* queue){
    uint8_t ret = queue->tail;
   queue->tail = (queue->tail + 1) & (PROC_PID_QUEUE_SIZE - 1);
   queue->full = false;
   return ret;
}

static inline void pid_queue_init(Pid_queue* queue){
    queue->head = 0;
    queue->tail = 0;
    queue->full = false;
}


static uint32_t free_list[PROC_PID_BITFIELD_SIZE];
static Process processes[NR_PROC_ALLOWED];
static uint8_t running_process;

static Process idle_process;
__attribute__((noreturn)) static void idle_process_func(void){
    while(1){
        __WFI();
    }
}

void process_scheduler_init(void){
    MEM_process_pool_allocator_init();

    idle_process.mem_base = MEM_process_pool_allocator_alloc(ALLOC_S);
    idle_process.mem_size = __stack_block_size_s;
    idle_process.stack_ptr = (uint32_t*)((uint8_t*)idle_process.mem_base + idle_process.mem_size - sizeof(Context_stack_frame));
    Context_stack_frame* sf = (Context_stack_frame*)idle_process.stack_ptr;
    *sf = (const Context_stack_frame){0};
    /*
    ** NOTE: T-bit (xPSR[24]) must be set or generate a INVSTATE exception.
    ** ref: arm_cortex_m3_r2p0_trm 2-8
    */
    sf->xPSR = 0x01000000;
    sf->PC = (uint32_t)idle_process_func;

    for(uint32_t i=0; i < NR_READY_QS; ++i){
        pid_queue_init(&ready_qs[i]);
    }

    // TODO: Set priority of PendSV_IRQn to 0xFF
    //       - Set priority of TIM2_IRQn to something higher



}





__attribute__((used)) __attribute__((visibility("hidden")))
volatile uint32_t* new_stack_ptr; // NOTE: Shared between asm and C implementation

void TIM2_IRQHandler(void){
    if(TIM2->SR & TIM_SR_UIF){
        TIM2->SR &= ~TIM_SR_UIF;
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
}

__attribute__((used)) __attribute__((visibility("hidden")))
 void process_context_switch(void){
    processes[running_process].stack_ptr = (uint32_t*)new_stack_ptr;
    //TODO: update the correct queues
    //   - Move pids from wait_q to correct ready_qs
    //   - Perhaps this is only for tick based wait?

    //select new process to run
    uint32_t next_pid = -1;
    CRITICAL_SECTION(
        for(uint32_t i=0; i < NR_READY_QS; ++i){
            if(ready_qs[i].head == ready_qs[i].tail && !ready_qs[i].full) continue;
            next_pid = pid_dequeue(&ready_qs[i]);
            break;
        }
    );
    //update new_stack_ptr
    if(next_pid != -1){
        new_stack_ptr = processes[next_pid].stack_ptr;
    }else{
        new_stack_ptr = idle_process.stack_ptr;
    }
    //let the PendSV_Handler restore the correct registers
    return;
}

__attribute__((naked)) void PendSV_Handler(void){
    __asm volatile(
        "mrs r0, psp\n"
        "stmdb r0!, {r4-r11}\n"
        "msr psp, r0\n"
        "ldr r1,=new_stack_ptr\n"
        "ldr r1, [r1]\n"
        "str r0, [r1]\n"

        "bl process_context_switch\n"

        "ldr r0, =new_stack_ptr\n"
        "ldr r0, [r0]\n"
        "ldmia r0!, {r4-r11}\n"
        "msr psp, r0\n"

        "bx lr\n"
);
}



void process_exit(void){
    Syscall_Request req = {
        .type = SYSCALL_EXIT,
        .arg = NULL,
    };
    int32_t ret;

    __asm volatile (
        "mov r0, %1\n"
        "svc #0\n"
        "mov %0, r0\n"
        : "=r" (ret)
        : "r" (&req)
        : "r0", "memory"
    );
    return;
}

int32_t syscall_exec(Process_form* form){
    // TODO: Check if the function is in the allowed functions table?

    if(form->priority < processes[running_process].priority || form->priority > NR_READY_QS){
        return -1;
    }
    uint32_t* mem_base = NULL;
    CRITICAL_SECTION(
        mem_base = (uint32_t*)MEM_process_pool_allocator_alloc(form->process_memory_allocator);
    );
    if(mem_base == NULL){
        return -1;
    }

    int32_t process_index = -1;
    CRITICAL_SECTION(
        for(uint32_t i = 0; i< PROC_PID_BITFIELD_SIZE*32; ++i){
            if(TEST_BITFIELD(free_list, i) == 0){
                process_index = i;
                SET_BITFIELD(free_list, i);
                break;
            }
        }
    );
    if(process_index == -1){
        goto error1;
    }

    processes[process_index].parent_pid = running_process;
    processes[process_index].status = (uint8_t)(PROC_STATE_READY);
    processes[process_index].priority = form->priority;
    processes[process_index].func = form->func;
    processes[process_index].args = form->args;
    processes[process_index].ret = form->ret;
    processes[process_index].mem_base = mem_base;

    switch (form->process_memory_allocator){
        case ALLOC_S:
            processes[process_index].mem_size = __stack_block_size_s;
            break;
        case ALLOC_M:
            processes[process_index].mem_size = __stack_block_size_m;
            break;
        case ALLOC_L:
            processes[process_index].mem_size = __stack_block_size_l;
            break;
        default:
            goto error1;
            break;
    }

    processes[process_index].stack_ptr = (uint32_t*)((uint8_t*)mem_base +
    processes[process_index].mem_size - sizeof(Context_stack_frame));

    //It will start running after a context switch
    Context_stack_frame* sf = (Context_stack_frame*)processes[process_index].stack_ptr;
    *sf = (const Context_stack_frame){0};
    /*
    ** NOTE: T-bit (xPSR[24]) must be set or generate a INVSTATE exception.
    ** ref: arm_cortex_m3_r2p0_trm 2-8
     */
    sf->xPSR = 0x01000000;
    sf->PC = (uint32_t)form->func;
    sf->LR = (uint32_t)process_exit;
    sf->r0 = (uint32_t)form->args;

    bool enqueue_ok = false;
    CRITICAL_SECTION(
        enqueue_ok = pid_enqueue(&ready_qs[form->priority], process_index);
    );
    if(!enqueue_ok){
        goto error2;
    }

    return process_index;
error2:
    CRITICAL_SECTION(
        CLEAR_BITFIELD(free_list, process_index);
    );
error1:
    CRITICAL_SECTION(
        MEM_process_pool_allocator_free(form->process_memory_allocator, mem_base);
    );
    return -1;
}

void SVC_Handler_C(uint32_t* stack){
    Syscall_Request* req = (Syscall_Request*)stack[0];
    switch(req->type){
        case SYSCALL_EXEC:
            stack[0] = syscall_exec(req->arg);
            break;
        case SYSCALL_EXIT:
            // TODO: call cleanup function, remove from lists.
            // FIXME: Do not return from this function, must start executing a new process
            break;
        default:
            stack[0] = -1;
    }
}

void SVC_Handler(void) {
    __asm volatile (
        "tst lr, #4        \n"  // Test bit 2 of EXC_RETURN (Link Register)
        "ite eq            \n"  // If/Then/Else
        "mrseq r0, msp     \n"  // If bit 2 is 0 (equal), use MSP (Main Stack Pointer)
        "mrsne r0, psp     \n"  // Else (bit 2 is 1), use PSP (Process Stack Pointer)
        "b SVC_Handler_C   \n"  // Branch to C handler, with r0 = pointer to saved stack frame
    );
}


int32_t exec(Process_form *form){
    Syscall_Request req = {
        .type = SYSCALL_EXEC,
        .arg = form,
    };
    int32_t ret;

    __asm volatile (
        "mov r0, %1\n"
        "svc #0\n"
        "mov %0, r0\n"
        : "=r" (ret)
        : "r" (&req)
        : "r0", "memory"
    );

    return ret;
}
