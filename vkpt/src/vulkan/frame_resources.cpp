#include <vkpt/vulkan/frame_resources.h>

VKPT_BEGIN

FrameResources::FrameResources(
    vk::Device device,
    uint32_t   frame_count,
    Queue      graphics_queue,
    Queue      compute_queue,
    Queue      transfer_queue)
    : sync_(device, frame_count),
      fences_(device, frame_count),
      cmd_buffers_(
          device, frame_count,
          graphics_queue.getFamilyIndex(),
          compute_queue.getFamilyIndex(),
          transfer_queue.getFamilyIndex()),
      semaphores_(device, frame_count)
{
    
}

void FrameResources::beginFrame()
{
    sync_.newFrame();
    fences_.newFrame();
    cmd_buffers_.newFrame();
    semaphores_.newFrame();
}

void FrameResources::endFrame(vk::ArrayProxy<const Queue> queues)
{
    sync_.endFrame(queues);
}

FenceAllocator &FrameResources::getFenceAllocator()
{
    return fences_;
}

CommandBufferAllocator &FrameResources::getCommandBufferAllocator()
{
    return cmd_buffers_;
}

SemaphoreAllocator &FrameResources::getSemaphoreAllocator()
{
    return semaphores_;
}

void FrameResources::executeAfterSync(std::function<void()> func)
{
    sync_.executeAfterSync(std::move(func));
}

void FrameResources::_triggerAllSync()
{
    sync_._triggerAllSync();
}

VKPT_END
