#pragma once

#include <vkpt/vulkan/command_buffer.h>

VKPT_BEGIN

class Queue
{
public:

    Queue();

    Queue(
        vk::Device device,
        vk::Queue  raw_queue,
        uint32_t   family_index);

    operator bool() const;

    vk::Device getDevice();

    vk::Queue getRaw();

    uint32_t getFamilyIndex() const;

    void submit(
        vk::ArrayProxy<const vk::Semaphore>          wait_semaphores,
        vk::ArrayProxy<const vk::PipelineStageFlags> wait_stages,
        vk::ArrayProxy<const vk::Semaphore>          signal_semaphores,
        vk::ArrayProxy<const CommandBuffer>          command_buffers,
        vk::Fence fence);

private:

    vk::Device device_;
    vk::Queue  queue_;
    uint32_t   family_index_;
};

inline Queue::Queue()
    : Queue(nullptr, nullptr, 0)
{
    
}

inline Queue::Queue(
    vk::Device device,
    vk::Queue  raw_queue,
    uint32_t   family_index)
    : device_(device), queue_(raw_queue), family_index_(family_index)
{
    assert(!device == !raw_queue);
}

inline Queue::operator bool() const
{
    return !!queue_;
}

inline vk::Device Queue::getDevice()
{
    return device_;
}

inline vk::Queue Queue::getRaw()
{
    return queue_;
}

inline uint32_t Queue::getFamilyIndex() const
{
    return family_index_;
}

inline void Queue::submit(
    vk::ArrayProxy<const vk::Semaphore>          wait_semaphores,
    vk::ArrayProxy<const vk::PipelineStageFlags> wait_stages,
    vk::ArrayProxy<const vk::Semaphore>          signal_semaphores,
    vk::ArrayProxy<const CommandBuffer>          command_buffers,
    vk::Fence                                    fence)
{
    static thread_local std::vector<vk::CommandBuffer> raw_command_buffers;

    raw_command_buffers.resize(command_buffers.size());
    for(uint32_t i = 0; i < command_buffers.size(); ++i)
        raw_command_buffers[i] = command_buffers.data()[i].getRaw();

    queue_.submit(
    {
        vk::SubmitInfo{
            .waitSemaphoreCount   = wait_semaphores.size(),
            .pWaitSemaphores      = wait_semaphores.data(),
            .pWaitDstStageMask    = wait_stages.data(),
            .commandBufferCount   = command_buffers.size(),
            .pCommandBuffers      = raw_command_buffers.data(),
            .signalSemaphoreCount = signal_semaphores.size(),
            .pSignalSemaphores    = signal_semaphores.data()
        }
    }, fence);
}

VKPT_END
