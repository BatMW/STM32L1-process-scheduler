#include "pool_allocator.h"
#include <stdint.h>
#include "stddef.h"


extern uint8_t* _pool_64_start;
extern uint8_t* _pool_64_end;

extern uint8_t* _pool_256_start;
extern uint8_t* _pool_256_end;

extern uint8_t* _pool_1KB_start;
extern uint8_t* _pool_1KB_end;

struct MEM_Pool_Allocator pool_64_allocator;
struct MEM_Pool_Allocator pool_256_allocator;
struct MEM_Pool_Allocator pool_1KB_allocator;



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


void MEM_procss_pool_allocator_init(void){
  init_pool(&pool_64_allocator, _pool_64_start, _pool_64_end, 64);
  init_pool(&pool_256_allocator, _pool_256_start, _pool_256_end, 256);
  init_pool(&pool_1KB_allocator, _pool_1KB_start, _pool_1KB_end, 1024);
}


static struct MEM_Pool_Allocator* get_allocator(Allocator alloc_type){
   switch (alloc_type){
    case ALLOC_64:
      return &pool_64_allocator;
      break;
    case ALLOC_256:
      return &pool_256_allocator;
      break;
    case ALLOC_1KB:
      return &pool_1KB_allocator;
      break;
    default:
      return NULL;
  }
}


void* MEM_procss_pool_allocator_alloc(Allocator alloc_type){
  struct MEM_Pool_Allocator* allocator = get_allocator(alloc_type);

  if(allocator == NULL)return NULL;
  if(allocator->base == NULL || allocator->head == NULL)return NULL;
  void* ret = (void*)allocator->head;
  allocator->head = allocator->head->next;
  return ret;
}

bool MEM_procss_pool_allocator_free(Allocator alloc_type, void* ptr){
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

void MEM_procss_pool_allocator_reset(Allocator alloc_type){
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
