#include "pool_allocator.h"
#include <stdint.h>
#include "stddef.h"


extern uint8_t _pool_s_start;
extern uint8_t _pool_s_end;

extern uint8_t _pool_m_start;
extern uint8_t _pool_m_end;

extern uint8_t _pool_l_start;
extern uint8_t _pool_l_end;

extern const uint32_t __stack_block_size_s;
extern const uint32_t __stack_block_size_m;
extern const uint32_t __stack_block_size_l;

struct MEM_Pool_Allocator pool_s_allocator;
struct MEM_Pool_Allocator pool_m_allocator;
struct MEM_Pool_Allocator pool_l_allocator;



static void init_pool(struct MEM_Pool_Allocator* allocator,
                      uint8_t* start, uint8_t* end, size_t block_size) {
  allocator->block_size = block_size;
  allocator->base = (char*)start;
  allocator->head = (struct MEM_Pool_Memory_Block*)start;
  allocator->nr_blocks = (size_t)(end - start) / block_size;

  struct MEM_Pool_Memory_Block* block_ptr = (struct MEM_Pool_Memory_Block*)start;
  for (size_t i = 0; i < allocator->nr_blocks; ++i) {
    block_ptr->next = (struct MEM_Pool_Memory_Block*)((uint8_t*)block_ptr + block_size);
    block_ptr = (struct MEM_Pool_Memory_Block*)((uint8_t*)block_ptr + block_size);
  }
  block_ptr = (struct MEM_Pool_Memory_Block*)((uint8_t*)block_ptr - block_size);
  block_ptr->next = NULL;
}


void MEM_process_pool_allocator_init(void){
  init_pool(&pool_s_allocator, &_pool_s_start, &_pool_s_end, __stack_block_size_s);
  init_pool(&pool_m_allocator, &_pool_m_start, &_pool_m_end, __stack_block_size_m);
  init_pool(&pool_l_allocator, &_pool_l_start, &_pool_l_end, __stack_block_size_l);
}


static struct MEM_Pool_Allocator* get_allocator(Allocator alloc_type){
   switch (alloc_type){
    case ALLOC_S:
      return &pool_s_allocator;
      break;
    case ALLOC_M:
      return &pool_m_allocator;
      break;
    case ALLOC_L:
      return &pool_l_allocator;
      break;
    default:
      return NULL;
  }
}


void* MEM_process_pool_allocator_alloc(Allocator alloc_type){
  struct MEM_Pool_Allocator* allocator = get_allocator(alloc_type);

  if(allocator == NULL)return NULL;
  if(allocator->base == NULL || allocator->head == NULL)return NULL;
  void* ret = (void*)allocator->head;
  allocator->head = allocator->head->next;
  return ret;
}

bool MEM_process_pool_allocator_free(Allocator alloc_type, void* ptr){
  struct MEM_Pool_Allocator* allocator = get_allocator(alloc_type);

  if(allocator == NULL)return false;
  if(allocator->base == NULL)return false;
  if ((char*)ptr < (char*)allocator->base || (char*)ptr >= (char*)(allocator->base + (allocator->nr_blocks * allocator->block_size))) return false;
  if ((size_t)ptr % allocator->block_size != 0) return false;
  struct MEM_Pool_Memory_Block* insert = (struct MEM_Pool_Memory_Block*)ptr;
  insert->next = allocator->head;
  allocator->head = insert;
  return true;
}

void MEM_process_pool_allocator_reset(Allocator alloc_type){
  struct MEM_Pool_Allocator* allocator = get_allocator(alloc_type);

  struct MEM_Pool_Memory_Block* block_ptr = (struct MEM_Pool_Memory_Block*)allocator->base;
  for (size_t i = 0; i < allocator->nr_blocks; ++i) {
    block_ptr->next = (struct MEM_Pool_Memory_Block*)((uint8_t*)block_ptr + allocator->block_size);
    block_ptr = (struct MEM_Pool_Memory_Block*)((uint8_t*)block_ptr + allocator->block_size);
  }
  block_ptr = (struct MEM_Pool_Memory_Block*)((uint8_t*)block_ptr - allocator->block_size);
  block_ptr->next = NULL;
}

void MEM_pool_allocator_init(struct MEM_Pool_Allocator* allocator){
  char* block_iterator = (char*)(allocator->base + allocator->block_size);
  struct MEM_Pool_Memory_Block* block_ptr = allocator->head;
  for(size_t i=0; i<allocator->nr_blocks-1; ++i){
    block_ptr->next = (struct MEM_Pool_Memory_Block*)block_iterator;
    block_iterator += allocator->block_size;
    block_ptr = block_ptr->next;
  }
  block_ptr->next = NULL;
}

void* MEM_pool_allocator_alloc(struct MEM_Pool_Allocator* allocator){

  if(allocator == NULL)return NULL;
  if(allocator->base == NULL || allocator->head == NULL)return NULL;
  void* ret = (void*)allocator->head;
  allocator->head = allocator->head->next;
  return ret;
}

bool MEM_pool_allocator_free(struct MEM_Pool_Allocator* allocator, void* ptr){

  if(allocator == NULL)return false;
  if(allocator->base == NULL)return false;
  if ((char*)ptr < (char*)allocator->base || (char*)ptr >= (char*)(allocator->base + (allocator->nr_blocks * allocator->block_size))) return false;
  if ((size_t)ptr % allocator->block_size != 0) return false;
  struct MEM_Pool_Memory_Block* insert = (struct MEM_Pool_Memory_Block*)ptr;
  insert->next = allocator->head;
  allocator->head = insert;
  return true;
}

void MEM_pool_allocator_destroy(struct MEM_Pool_Allocator* allocator){
  MEM_pool_allocator_init(allocator);
}
