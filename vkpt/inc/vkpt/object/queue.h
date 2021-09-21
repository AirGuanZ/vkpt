#pragma once

#include <vkpt/command_buffer.h>

VKPT_BEGIN

class Queue
{
public:

    enum class Type
    {
        Graphics = 0,
        Compute  = 1,
        Transfer = 2,
        Present  = 3
    };

    Queue();

    Queue(
        vk::Device device,
        vk::Queue  raw_queue,
        Type       type,
        uint32_t   family_index);

    operator bool() const;

    vk::Device getDevice();

    vk::Queue getRaw();

    Type getType() const;

    uint32_t getFamilyIndex() const;

    void submit(
        vk::ArrayProxy<const vk::Semaphore>          wait_semaphores,
        vk::ArrayProxy<const vk::PipelineStageFlags> wait_stages,
        vk::ArrayProxy<const vk::Semaphore>          signal_semaphores,
        vk::ArrayProxy<const CommandBuffer>          command_buffers,
        vk::Fence                                    fence) const;

    void submit(
        vk::ArrayProxy<const vk::SemaphoreSubmitInfoKHR>     wait_semaphores,
        vk::ArrayProxy<const vk::SemaphoreSubmitInfoKHR>     signal_semaphores,
        vk::ArrayProxy<const vk::CommandBufferSubmitInfoKHR> command_buffers,
        vk::Fence                                            fence) const;

    auto operator<=>(const Queue &rhs) const { return queue_ <=> rhs.queue_; }

private:

    vk::Device device_;
    vk::Queue  queue_;
    Type       type_;
    uint32_t   family_index_;
};

inline Queue::Queue()
    : Queue(nullptr, nullptr, Type::Graphics, 0)
{
    
}

inline Queue::Queue(
    vk::Device device,
    vk::Queue  raw_queue,
    Type       type,
    uint32_t   family_index)
    : device_(device), queue_(raw_queue),
      type_(type), family_index_(family_index)
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

inline Queue::Type Queue::getType() const
{
    return type_;
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
    vk::Fence                                    fence) const
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

inline void Queue::submit(
    vk::ArrayProxy<const vk::SemaphoreSubmitInfoKHR>     wait_semaphores,
    vk::ArrayProxy<const vk::SemaphoreSubmitInfoKHR>     signal_semaphores,
    vk::ArrayProxy<const vk::CommandBufferSubmitInfoKHR> command_buffers,
    vk::Fence                                            fence) const
{
    queue_.submit2KHR(
    {
        vk::SubmitInfo2KHR{
            .waitSemaphoreInfoCount   = wait_semaphores.size(),
            .pWaitSemaphoreInfos      = wait_semaphores.data(),
            .commandBufferInfoCount   = command_buffers.size(),
            .pCommandBufferInfos      = command_buffers.data(),
            .signalSemaphoreInfoCount = signal_semaphores.size(),
            .pSignalSemaphoreInfos    = signal_semaphores.data()
        }
    }, fence);
}

VKPT_END
