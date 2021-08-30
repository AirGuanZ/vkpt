#pragma once

#include <vkpt/vulkan/command_buffer_allocator.h>
#include <vkpt/vulkan/queue.h>

VKPT_BEGIN

class PerFrameCommandBuffers : public agz::misc::uncopyable_t
{
public:

    PerFrameCommandBuffers(
        vk::Device device,
        uint32_t   frame_count,
        uint32_t   queue_family);
    
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

class PerFramePerQueueCommandBuffers :
    public agz::misc::uncopyable_t,
    public CommandBufferAllocator
{
public:

    PerFramePerQueueCommandBuffers(
        vk::Device device,
        uint32_t   frame_count,
        uint32_t   graphics_queue,
        uint32_t   compute_queue,
        uint32_t   transfer_queue);

    void newFrame();

    CommandBuffer newGraphicsCommandBuffer() override;

    CommandBuffer newComputeCommandBuffer() override;

    CommandBuffer newTransferCommandBuffer() override;

private:

    PerFrameCommandBuffers graphics_;
    PerFrameCommandBuffers compute_;
    PerFrameCommandBuffers transfer_;
};

VKPT_END
