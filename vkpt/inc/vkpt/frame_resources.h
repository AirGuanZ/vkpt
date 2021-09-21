#pragma once

#include <vkpt/frame_synchronizer.h>
#include <vkpt/perframe_command_buffers.h>
#include <vkpt/perframe_fences.h>
#include <vkpt/perframe_semaphores.h>

VKPT_BEGIN

class FrameResources :
    public agz::misc::uncopyable_t,
    public CommandBufferAllocator,
    public FenceAllocator,
    public SemaphoreAllocator
{
public:

    FrameResources() = default;

    FrameResources(
        vk::Device device,
        uint32_t   frame_count,
        Queue     *graphics_queue,
        Queue     *compute_queue,
        Queue     *transfer_queue,
        Queue     *present_queue);

    FrameResources(FrameResources &&) noexcept = default;

    FrameResources &operator=(FrameResources &&) noexcept = default;

    ~FrameResources();

    operator bool() const;

    void beginFrame();

    void endFrame(vk::ArrayProxy<Queue *const> queues);

    CommandBufferAllocator &getCommandBufferAllocator();

    SemaphoreAllocator &getSemaphoreAllocator();

    FenceAllocator &getFenceAllocator();

    CommandBuffer newCommandBuffer(Queue::Type type) override;

    vk::Fence newFence();

    vk::Semaphore newSemaphore();

    void executeAfterSync(std::function<void()> func);

    template<typename T>
    void destroyAfterSync(T obj);

    void _triggerAllSync();

private:

    FrameSynchronizer      sync_;
    PerFrameFences         fences_;
    PerFrameCommandBuffers cmd_buffers_;
    PerFrameSemaphores     semaphores_;
};

template<typename T>
void FrameResources::destroyAfterSync(T obj)
{
    sync_.destroyAfterSync(std::move(obj));
}

VKPT_END
