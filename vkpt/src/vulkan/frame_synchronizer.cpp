#include <vkpt/vulkan/frame_synchronizer.h>

VKPT_BEGIN

FrameSynchronizer::FrameSynchronizer(vk::Device device, int frame_count)
    : device_(device), frame_index_(0)
{
    vk::FenceCreateInfo fence_create_info = {
        .flags = vk::FenceCreateFlagBits::eSignaled
    };

    fence_info_.reserve(frame_count);
    for(int i = 0; i < frame_count; ++i)
        fence_info_.push_back(
            { false, device.createFenceUnique(fence_create_info) });
}

FrameSynchronizer::~FrameSynchronizer()
{
    for(auto &f : fence_info_)
    {
        if(f.will_be_signaled)
        {
            auto fence = f.fence.get();
            (void)device_.waitForFences(
                1, &fence, true, UINT64_MAX);
        }

        for(auto &func : f.delayed_functions)
            func();
        f.delayed_functions.clear();
    }
}

void FrameSynchronizer::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(fence_info_.size());

    auto &fence_info = fence_info_[frame_index_];
    fence_info.will_be_signaled = true;

    vk::Fence fence = fence_info.fence.get();
    vk::Result result = device_.waitForFences(1, &fence, true, UINT64_MAX);
    if(result != vk::Result::eSuccess)
    {
        throw VKPTException(
            "error in waiting for frame fence: " + to_string(result));
    }

    result = device_.resetFences(1, &fence);
    if(result != vk::Result::eSuccess)
    {
        throw VKPTException(
            "error in resetting the frame fence: " + to_string(result));
    }

    for(auto &func : fence_info.delayed_functions)
        func();
    fence_info.delayed_functions.clear();
}

vk::Fence FrameSynchronizer::getFrameEndingFence()
{
    return fence_info_[frame_index_].fence.get();
}

void FrameSynchronizer::executeAfterSync(std::function<void()> func)
{
    fence_info_[frame_index_].delayed_functions.push_back(std::move(func));
}

void FrameSynchronizer::_triggerAllSync()
{
    for(auto &f : fence_info_)
    {
        for(auto &func : f.delayed_functions)
            func();
        f.delayed_functions.clear();
    }
}

VKPT_END
