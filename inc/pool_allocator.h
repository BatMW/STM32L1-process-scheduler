#ifndef MEM_POOL_ALLOCATOR
#define MEM_POOL_ALLOCATOR
#include <stddef.h>
#include <stdint.h>

#define true 1
#define false 0
#define bool uint8_t

typedef enum {ALLOC_S, ALLOC_M, ALLOC_L} Allocator;

struct MEM_Pool_Memory_Block{
  struct MEM_Pool_Memory_Block* next;
};

struct MEM_Pool_Allocator{
  size_t block_size;
  size_t nr_blocks;
  struct MEM_Pool_Memory_Block* head;
  uint8_t* base;
};

void MEM_process_pool_allocator_init(void);

void* MEM_process_pool_allocator_alloc(Allocator alloc_type);

bool MEM_process_pool_allocator_free(Allocator alloc_type, void* ptr);

void MEM_process_pool_allocator_reset(Allocator alloc_type);



void MEM_pool_allocator_init(struct MEM_Pool_Allocator* allocator);

void* MEM_pool_allocator_alloc(struct MEM_Pool_Allocator* allocator);

bool MEM_pool_allocator_free(struct MEM_Pool_Allocator* allocator, void* ptr);

bool MEM_pool_allocator_reset(struct MEM_Pool_Allocator* allocator);

#ifdef UNITY_BUILD
  #include "pool_allocator.c"
#endif //UNITY_BUILD

#endif // MEM_POOL_ALLOCATOR
