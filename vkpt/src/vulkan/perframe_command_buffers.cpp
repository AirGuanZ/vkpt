#include <vkpt/vulkan/perframe_command_buffers.h>

VKPT_BEGIN

PerFrameCommandBuffers::PerFrameCommandBuffers(
    vk::Device device,
    int        frame_count,
    uint32_t   queue_family)
    : device_(device), frame_index_(0),
      queue_family_(queue_family)
{
    frames_.resize(frame_count);
    for(auto &f : frames_)
    {
        f.pool = device.createCommandPoolUnique(
            vk::CommandPoolCreateInfo{
                .queueFamilyIndex = queue_family
            });
    }
}

void PerFrameCommandBuffers::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(frames_.size());

    auto &f = frames_[frame_index_];

    if(f.next_available_buffer_index)
    {
        device_.resetCommandPool(f.pool.get());
        f.next_available_buffer_index = 0;
    }
}

vk::CommandBuffer PerFrameCommandBuffers::newCommandBuffer()
{
    auto &f = frames_[frame_index_];
    if(f.next_available_buffer_index >= f.buffers.size())
    {
        auto buffer = std::move(device_.allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo{
                .commandPool        = f.pool.get(),
                .level              = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1
            }).front());
        f.buffers.push_back(std::move(buffer));
    }
    return f.buffers[f.next_available_buffer_index++].get();
}

VKPT_END
