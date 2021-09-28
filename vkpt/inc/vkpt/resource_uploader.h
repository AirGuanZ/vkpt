#pragma once

#include <vkpt/allocator/resource_allocator.h>
#include <vkpt/object/queue.h>

VKPT_BEGIN

class ResourceUploader : public agz::misc::uncopyable_t
{
public:

    ResourceUploader(
        Queue *transfer_queue, ResourceAllocator &resource_allocator);

    ~ResourceUploader();

    void uploadBuffer(
        Buffer      dst_buffer,
        const void *data,
        size_t      bytes);

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
