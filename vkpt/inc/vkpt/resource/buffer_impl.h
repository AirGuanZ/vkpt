#pragma once

#include <vkpt/resource/buffer.h>

typedef struct VmaAllocation_T *VmaAllocation;
typedef struct VmaAllocator_T  *VmaAllocator;

VKPT_BEGIN

struct Buffer::Impl
{
    vk::Device device;
    vk::Buffer buffer;

    Description description;
    State       state;

    std::string name = "unknown";

    VmaAllocation allocation = nullptr;
    VmaAllocator  allocator  = nullptr;
};

VKPT_END
