#include <vkpt/frame/perframe_semaphores.h>

VKPT_BEGIN

PerFrameSemaphores::PerFrameSemaphores()
    : device_(nullptr), frame_index_(0)
{
    
}

PerFrameSemaphores::PerFrameSemaphores(vk::Device device, uint32_t frame_count)
    : device_(device), frame_index_(0)
{
    frames_.resize(frame_count);
}

PerFrameSemaphores::PerFrameSemaphores(PerFrameSemaphores &&other) noexcept
    : PerFrameSemaphores()
{
    swap(other);
}

PerFrameSemaphores &PerFrameSemaphores::operator=(PerFrameSemaphores &&other) noexcept
{
    swap(other);
    return *this;
}

PerFrameSemaphores::operator bool() const
{
    return !frames_.empty();
}

void PerFrameSemaphores::swap(PerFrameSemaphores &other) noexcept
{
    std::swap(device_, other.device_);
    std::swap(frame_index_, other.frame_index_);
    std::swap(frames_, other.frames_);
}

void PerFrameSemaphores::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(frames_.size());
    frames_[frame_index_].next_available_semaphore = 0;
    frames_[frame_index_].next_available_timeline_semaphore = 0;
}

vk::Semaphore PerFrameSemaphores::newSemaphore()
{
    auto &frame = frames_[frame_index_];
    if(frame.next_available_semaphore >= frame.semaphores.size())
        frame.semaphores.push_back(device_.createSemaphoreUnique({}));
    return frame.semaphores[frame.next_available_semaphore++].get();
}

TimelineSemaphore PerFrameSemaphores::newTimelineSemaphore()
{
    auto &frame = frames_[frame_index_];
    if(frame.next_available_timeline_semaphore >=
       frame.timeline_semaphores.size())
    {
        const vk::SemaphoreTypeCreateInfo real_create_info = {
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0
        };
        const vk::SemaphoreCreateInfo create_info = {
            .pNext = &real_create_info
        };
        auto semaphore = device_.createSemaphoreUnique(create_info);
        auto timeline = TimelineSemaphore(std::move(semaphore), 0);

        frame.timeline_semaphores.push_back(std::move(timeline));
    }

    return frame.timeline_semaphores[
        frame.next_available_timeline_semaphore++];
}

VKPT_END
