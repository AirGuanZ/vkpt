#pragma once

#include <vkpt/vulkan/frame_synchronizer.h>
#include <vkpt/vulkan/perframe_command_buffers.h>
#include <vkpt/vulkan/perframe_fences.h>
#include <vkpt/vulkan/perframe_semaphores.h>

VKPT_BEGIN

class FrameResources : public agz::misc::uncopyable_t
{
public:

    FrameResources(
        vk::Device device,
        uint32_t   frame_count,
        Queue      graphics_queue,
        Queue      compute_queue,
        Queue      transfer_queue);

    void beginFrame();

    void endFrame(vk::ArrayProxy<const Queue> queues);

    CommandBufferAllocator &getCommandBufferAllocator();

    SemaphoreAllocator &getSemaphoreAllocator();

    FenceAllocator &getFenceAllocator();

    void executeAfterSync(std::function<void()> func);

    template<typename T>
    void destroyAfterSync(T obj);

    void _triggerAllSync();

private:

    FrameSynchronizer              sync_;
    PerFrameFences                 fences_;
    PerFramePerQueueCommandBuffers cmd_buffers_;
    PerFrameSemaphores             semaphores_;
};

template<typename T>
void FrameResources::destroyAfterSync(T obj)
{
    sync_.destroyAfterSync(std::move(obj));
}

VKPT_END
