#pragma once

#include <vkpt/object/queue.h>
#include <vkpt/resource_allocator.h>

VKPT_BEGIN

class ResourceUploader : public agz::misc::uncopyable_t
{
public:

    ResourceUploader(
        Queue *transfer_queue, ResourceAllocator &resource_allocator);

    ~ResourceUploader();

    void uploadBuffer(
        vk::Buffer  dst_buffer,
        const void *data,
        size_t      bytes);

    // upload and release ownership
    void uploadBuffer(
        vk::Buffer  dst_buffer,
        const void *data,
        size_t      bytes,
        uint32_t    dst_queue_family_index);
    
    void uploadBuffer(
        vk::Buffer   dst_buffer,
        const void  *data,
        size_t       bytes,
        const Queue &dst_queue);

    void submitAndSync();

private:

    struct StagingResource
    {
        virtual ~StagingResource() = default;
    };

    struct StagingBuffer : StagingResource
    {
        Buffer buffer;
    };

    Queue *queue_;

    ResourceAllocator &resource_allocator_;

    vk::UniqueFence sync_fence_;

    vk::UniqueCommandPool   command_pool_;
    vk::UniqueCommandBuffer command_buffer_;
    bool                    is_dirty_ = false;

    std::vector<vk::BufferMemoryBarrier>          buffer_memory_barriers_;
    std::vector<std::unique_ptr<StagingResource>> staging_resources_;
};

VKPT_END