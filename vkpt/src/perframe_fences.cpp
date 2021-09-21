#include <vkpt/perframe_fences.h>

VKPT_BEGIN

PerFrameFences::PerFrameFences()
    : device_(nullptr), frame_index_(0)
{
    
}

PerFrameFences::PerFrameFences(vk::Device device, uint32_t frame_count)
    : device_(device), frame_index_(0)
{
    frames_.resize(frame_count);
}

PerFrameFences::PerFrameFences(PerFrameFences &&other) noexcept
    : PerFrameFences()
{
    swap(other);
}

PerFrameFences &PerFrameFences::operator=(PerFrameFences &&other) noexcept
{
    swap(other);
    return *this;
}

PerFrameFences::operator bool() const
{
    return frames_.data();
}

void PerFrameFences::swap(PerFrameFences &other) noexcept
{
    std::swap(device_, other.device_);
    std::swap(frame_index_, other.frame_index_);
    std::swap(frames_, other.frames_);
}

void PerFrameFences::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(frames_.size());
    frames_[frame_index_].next_available_fence = 0;
}

vk::Fence PerFrameFences::newFence()
{
    auto &frame = frames_[frame_index_];

    if(frame.next_available_fence >= frame.fences.size())
        frame.fences.push_back(device_.createFenceUnique({}));

    auto result = frame.fences[frame.next_available_fence++].get();
    device_.resetFences({ result });
    return result;
}

VKPT_END
