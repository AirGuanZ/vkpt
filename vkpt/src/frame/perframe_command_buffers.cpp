#include <vkpt/frame/perframe_command_buffers.h>

VKPT_BEGIN

PerFrameSingleQueueCommandBuffers::PerFrameSingleQueueCommandBuffers()
    : device_(nullptr), frame_index_(0), queue_family_(0)
{
    
}

PerFrameSingleQueueCommandBuffers::PerFrameSingleQueueCommandBuffers(
    vk::Device device,
    uint32_t   frame_count,
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

PerFrameSingleQueueCommandBuffers::PerFrameSingleQueueCommandBuffers(
    PerFrameSingleQueueCommandBuffers &&other) noexcept
    : PerFrameSingleQueueCommandBuffers()
{
    swap(other);
}

PerFrameSingleQueueCommandBuffers &PerFrameSingleQueueCommandBuffers::operator=(
    PerFrameSingleQueueCommandBuffers &&other) noexcept
{
    swap(other);
    return *this;
}

PerFrameSingleQueueCommandBuffers::operator bool() const
{
    return !frames_.empty();
}

void PerFrameSingleQueueCommandBuffers::swap(PerFrameSingleQueueCommandBuffers &other) noexcept
{
    std::swap(device_, other.device_);
    std::swap(frame_index_, other.frame_index_);
    std::swap(frames_, other.frames_);
    std::swap(queue_family_, other.queue_family_);
}

void PerFrameSingleQueueCommandBuffers::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(frames_.size());

    auto &f = frames_[frame_index_];

    if(f.next_available_buffer_index)
    {
        device_.resetCommandPool(f.pool.get());
        f.next_available_buffer_index = 0;
    }
}

vk::CommandBuffer PerFrameSingleQueueCommandBuffers::newCommandBuffer()
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

PerFrameCommandBuffers::PerFrameCommandBuffers(
    vk::Device device,
    uint32_t   frame_count,
    uint32_t   graphics_queue,
    uint32_t   compute_queue,
    uint32_t   transfer_queue,
    uint32_t   present_queue)
{
    per_queue_[static_cast<int>(Queue::Type::Graphics)] =
        PerFrameSingleQueueCommandBuffers(device, frame_count, graphics_queue);
    per_queue_[static_cast<int>(Queue::Type::Compute)] =
        PerFrameSingleQueueCommandBuffers(device, frame_count, compute_queue);
    per_queue_[static_cast<int>(Queue::Type::Transfer)] =
        PerFrameSingleQueueCommandBuffers(device, frame_count, transfer_queue);
    per_queue_[static_cast<int>(Queue::Type::Present)] =
        PerFrameSingleQueueCommandBuffers(device, frame_count, present_queue);
}

PerFrameCommandBuffers::operator bool() const
{
    return per_queue_[0];
}

void PerFrameCommandBuffers::newFrame()
{
    for(auto &q : per_queue_)
        q.newFrame();
}

CommandBuffer PerFrameCommandBuffers::newCommandBuffer(Queue::Type type)
{
    return per_queue_[static_cast<int>(type)].newCommandBuffer();
}

VKPT_END
