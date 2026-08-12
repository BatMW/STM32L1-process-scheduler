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
    SYSCALL_WAIT,
    SYSCALL_SLEEP,
    SYSCALL_YIELD
} Syscall_Type;

typedef struct {
    Syscall_Type type;
    void* arg;
} Syscall_Request;


static Pid_queue ready_qs[NR_READY_QS];



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
    /*
    if (queue->tail == queue->head && !queue->full) {
        return -1; // empty queue
    }
    */
    uint8_t ret = queue->buffer[queue->tail];
    queue->tail = (queue->tail + 1) & (PROC_PID_QUEUE_SIZE - 1);
    queue->full = false;
    return ret;
}

static inline void pid_unordered_remove(Pid_queue* queue, uint8_t value) {
    if(queue->tail == queue->head && !queue->full){
        // empty
        return;
    }

    uint8_t count = queue->full ? PROC_PID_QUEUE_SIZE :
                     (queue->head - queue->tail + PROC_PID_QUEUE_SIZE) & (PROC_PID_QUEUE_SIZE - 1);

    for(uint8_t i = 0; i < count; i++) {
        uint8_t idx = (queue->tail + i) & (PROC_PID_QUEUE_SIZE - 1);
        if(queue->buffer[idx] == value) {
            uint8_t last = (queue->head - 1 + PROC_PID_QUEUE_SIZE) & (PROC_PID_QUEUE_SIZE - 1);

            queue->buffer[idx] = queue->buffer[last];

            queue->head = last;
            queue->full = false;
            return;
        }
    }
}


static inline void pid_queue_init(Pid_queue* queue){
    queue->head = 0;
    queue->tail = 0;
    queue->full = false;
}


static uint32_t free_list[PROC_PID_BITFIELD_SIZE];
static Process processes[NR_PROC_ALLOWED];
static int32_t running_process;

typedef struct Unordered_sleep_list{
    uint8_t count;
    uint32_t ticks_left[NR_PROC_ALLOWED];
    uint8_t pids[NR_PROC_ALLOWED];
} Unordered_sleep_list;

static inline void sleep_list_init(Unordered_sleep_list* list){
    list->count = 0;
    for(uint32_t i=0; i<NR_PROC_ALLOWED; ++i){
        list->pids[i] = 0;
    }
}

static inline bool sleep_list_add(Unordered_sleep_list* list, const uint8_t pid, const uint32_t ticks){
    if(list->count >= NR_PROC_ALLOWED){
        return false;
    }
    list->pids[list->count] = pid;
    list->ticks_left[list->count] = ticks;
    list->count++;
    return true;
}

static void sleep_list_decrement_and_ready(Unordered_sleep_list* list){
    for(uint32_t i = 0; i<list->count; ++i){
        list->ticks_left[i]--;
        if(list->ticks_left[i] == 0){
            uint8_t pid = list->pids[i];
            processes[pid].status &= ~PROC_STATE_SLEEPING;
            pid_enqueue(&ready_qs[processes[pid].priority], pid);

            list->ticks_left[i] = list->ticks_left[list->count-1];
            list->pids[i] = list->pids[list->count-1];
            list->count--;
            i--; //NOTE: need to check new value at current position
        }
    }
}
static Unordered_sleep_list sleep_list;

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
    running_process = -1;

    sleep_list_init(&sleep_list);

    // TODO: Set priority of PendSV_IRQn to 0xFF (PendSV = context switch entry IRQ)
    //       - Set priority of TIM2_IRQn to something higher (TIM2 = time for context switch)



}

void TIM2_IRQHandler(void){
    if(TIM2->SR & TIM_SR_UIF){
        TIM2->SR &= ~TIM_SR_UIF;
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
}

// NOTE: Shared between asm and C implementation (process_context_switch, PendSV_Handler)
__attribute__((used)) __attribute__((visibility("hidden")))
volatile uint32_t* new_stack_ptr;

__attribute__((used)) __attribute__((visibility("hidden")))
 void process_context_switch(void){
    if(running_process == -1){
        idle_process.stack_ptr = (uint32_t*)new_stack_ptr;
    }else{
        processes[running_process].stack_ptr = (uint32_t*)new_stack_ptr;
        if((processes[running_process].status & PROC_STATE_SLEEPING) == 0){
            //NOTE: If it is sleeping it just got added to the sleep list
            pid_enqueue(&ready_qs[processes[running_process].priority], running_process);
        }
    }
    //TODO: update the correct queues
    //   - Enqueue the current process into the correct ready queue
    //   - Move pids from wait_q to correct ready_qs
    //   - Perhaps this is only for tick based wait?

    sleep_list_decrement_and_ready(&sleep_list);
    //select new process to run
    int32_t next_pid = -1;
    CRITICAL_SECTION(
        for(uint32_t i=0; i < NR_READY_QS; ++i){
            if(ready_qs[i].head == ready_qs[i].tail && !ready_qs[i].full) continue;

            next_pid = (int32_t)pid_dequeue(&ready_qs[i]);
            break;
        }
    );
    //update new_stack_ptr
    if(next_pid != -1){
        running_process = next_pid;
        new_stack_ptr = processes[next_pid].stack_ptr;
    }else{
        new_stack_ptr = idle_process.stack_ptr;
    }
    //NOTE: PendSV_Handler restores the correct registers
    return;
}

__attribute__((naked)) void PendSV_Handler(void){
    __asm volatile(
        "mrs r0, psp\n"
        "stmdb r0!, {r4-r11}\n"
        "msr psp, r0\n"
        "ldr r1,=new_stack_ptr\n" // &new_stack_ptr
        //"ldr r1, [r1]\n"
        "str r0, [r1]\n" // new_stack_ptr

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

    if(form->priority < processes[running_process].priority || form->priority >= NR_READY_QS){
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

    uint32_t mem_size = 0;
    switch (form->process_memory_allocator){
        case ALLOC_S:
            mem_size = __stack_block_size_s;
            break;
        case ALLOC_M:
            mem_size = __stack_block_size_m;
            break;
        case ALLOC_L:
            mem_size = __stack_block_size_l;
            break;
        default:
            goto error1;
            break;
    }

    processes[process_index].mem_size = form->process_memory_allocator;

    //It will start running after a context switch
    processes[process_index].stack_ptr = (uint32_t*)((uint8_t*)mem_base +
    mem_size - sizeof(Context_stack_frame));

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

static inline void destroy_process(void){
    int32_t pid = -1;
    CRITICAL_SECTION(
        pid = running_process;
        processes[pid].status |= PROC_STATE_EXITED;
        Pid_queue* q = &ready_qs[processes[pid].priority];
        pid_unordered_remove(q, pid);
        MEM_process_pool_allocator_free(processes[pid].mem_size, processes[pid].mem_base);
        CLEAR_BITFIELD(free_list, pid);
    );

    while(1){
        __WFI(); // just wait for the next context switch
    }
}

__attribute__((used)) __attribute__((visibility("hidden")))
void SVC_Handler_C(Interrupt_stack_frame* stack){
    Syscall_Request* req = (Syscall_Request*)stack->r0;
    switch(req->type){
        case SYSCALL_EXEC:
            stack->r0 = syscall_exec(req->arg);
            break;
        case SYSCALL_EXIT:
            destroy_process(); //NOTE: Never returns.
            break;
        case SYSCALL_SLEEP:
            //put in wait_q
            sleep_list_add(&sleep_list, running_process, *(uint32_t*)req->arg);
            // TODO: remove running_process from ready queue
            processes[running_process].status |= PROC_STATE_SLEEPING;

            //force a context switch
            SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
            break;
        case SYSCALL_YIELD:
            //force a context switch
            SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
;
            break;
        default:
            stack->r0 = -1;
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

void sleep_ticks(uint32_t ticks){
     Syscall_Request req = {
        .type = SYSCALL_SLEEP,
        .arg = &ticks,
    };

        __asm volatile (
        "mov r0, %0\n"
        "svc #0\n"
        :
        : "r" (&req)
        : "r0", "memory"
    );
}

void wait(uint8_t pid){
     Syscall_Request req = {
        .type = SYSCALL_SLEEP,
        .arg = &pid,
    };

        __asm volatile (
        "mov r0, %0\n"
        "svc #0\n"
        :
        : "r" (&req)
        : "r0", "memory"
    );
}

void yield(void){
     Syscall_Request req = {
        .type = SYSCALL_YIELD,
        .arg = NULL,
    };

        __asm volatile (
        "mov r0, %0\n"
        "svc #0\n"
        :
        : "r" (&req)
        : "r0", "memory"
    );
}
