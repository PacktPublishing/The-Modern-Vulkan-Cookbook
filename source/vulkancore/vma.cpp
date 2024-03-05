#define VMA_IMPLEMENTATION  // needed for vma, needs to be before vk_mem_alloc.h
                            // and in single cpp file
#define VMA_STATIC_VULKAN_FUNCTIONS 0

#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#include "vk_mem_alloc.h"
