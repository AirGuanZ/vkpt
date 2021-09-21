#pragma once

#include <vk_mem_alloc.h>

#include <vkpt/object/object.h>

VKPT_BEGIN

struct BufferDescription
{
    size_t               size;
    vk::BufferUsageFlags usage;
    vk::SharingMode      sharing_mode;

    VmaAllocation allocation;
    VmaAllocator  allocator;
};

class Buffer : Object<vk::Buffer, vk::Buffer, BufferDescription>
{
    struct BufferDeleter
    {
        void operator()(Record *record) const
        {
            vk::Buffer buffer = record->object;
            BufferDescription &desc = record->description;
            if(desc.allocation)
                vmaDestroyBuffer(desc.allocator, buffer, desc.allocation);
            delete record;
        }
    };

public:

    VKPT_USING_OBJECT_METHODS(vk::Buffer);

    Buffer() = default;

    Buffer(vk::Buffer buffer, const BufferDescription &description)
        : Object(buffer, description, BufferDeleter())
    {

    }

    void *map()
    {
        void *ret;
        auto rt = vmaMapMemory(
            getDescription().allocator, getDescription().allocation, &ret);
        if(rt != VK_SUCCESS)
        {
            throw std::runtime_error(
                "failed to map vma buffer. error code is " +
                std::to_string(rt));
        }
        return ret;
    }

    void unmap()
    {
        vmaUnmapMemory(getDescription().allocator, getDescription().allocation);
    }

    auto operator<=>(const Buffer &) const = default;
};

VKPT_END
