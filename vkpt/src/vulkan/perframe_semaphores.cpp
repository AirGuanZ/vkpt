#include <vkpt/vulkan/perframe_semaphores.h>

VKPT_BEGIN

PerFrameSemaphores::PerFrameSemaphores(vk::Device device, uint32_t frame_count)
    : device_(device), frame_index_(0)
{
    frames_.resize(frame_count);
}

void PerFrameSemaphores::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(frames_.size());
    frames_[frame_index_].next_available_semaphore = 0;
}

vk::Semaphore PerFrameSemaphores::newSemaphore()
{
    auto &frame = frames_[frame_index_];
    if(frame.next_available_semaphore >= frame.semaphores.size())
        frame.semaphores.push_back(device_.createSemaphoreUnique({}));
    return frame.semaphores[frame.next_available_semaphore++].get();
}

VKPT_END
