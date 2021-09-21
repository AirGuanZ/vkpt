#pragma once

#include <vkpt/object/queue.h>
#include <vkpt/command_buffer_allocator.h>

VKPT_BEGIN

class PerFrameSingleQueueCommandBuffers : public agz::misc::uncopyable_t
{
public:

    PerFrameSingleQueueCommandBuffers();

    PerFrameSingleQueueCommandBuffers(
        vk::Device device,
        uint32_t   frame_count,
        uint32_t   queue_family);

    PerFrameSingleQueueCommandBuffers(PerFrameSingleQueueCommandBuffers &&other) noexcept;

    PerFrameSingleQueueCommandBuffers &operator=(PerFrameSingleQueueCommandBuffers &&other) noexcept;

    operator bool() const;

    void swap(PerFrameSingleQueueCommandBuffers &other) noexcept;
    
    void newFrame();

    vk::CommandBuffer newCommandBuffer();
    
private:

    struct PerFrame
    {
        vk::UniqueCommandPool                pool;
        std::vector<vk::UniqueCommandBuffer> buffers;
        size_t                               next_available_buffer_index = 0;
    };

    vk::Device device_;

    int frame_index_;
    std::vector<PerFrame> frames_;

    uint32_t queue_family_;
};

class PerFrameCommandBuffers :
    public agz::misc::uncopyable_t,
    public CommandBufferAllocator
{
public:

    PerFrameCommandBuffers() = default;;

    PerFrameCommandBuffers(
        vk::Device device,
        uint32_t   frame_count,
        uint32_t   graphics_queue,
        uint32_t   compute_queue,
        uint32_t   transfer_queue,
        uint32_t   present_queue);

    PerFrameCommandBuffers(PerFrameCommandBuffers &&) noexcept = default;

    PerFrameCommandBuffers &operator=(PerFrameCommandBuffers &&) noexcept = default;

    operator bool() const;

    void newFrame();

    CommandBuffer newCommandBuffer(Queue::Type type) override;

private:

    std::array<PerFrameSingleQueueCommandBuffers, 4> per_queue_;
};

VKPT_END
