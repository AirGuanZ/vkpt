#pragma once

#include <vkpt/allocator/resource_allocator.h>

VKPT_BEGIN

class ImageAllocator
{
public:

    virtual ~ImageAllocator() = default;

    virtual Image newImage(
        const vk::ImageCreateInfo &create_info, vma::MemoryUsage usage) = 0;
};

VKPT_END
