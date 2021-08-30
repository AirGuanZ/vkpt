#include <vkpt/vulkan/frame_synchronizer.h>

VKPT_BEGIN

FrameSynchronizer::FrameSynchronizer(vk::Device device, int frame_count)
    : device_(device), frame_index_(0)
{
    fence_info_.resize(frame_count);
}

FrameSynchronizer::~FrameSynchronizer()
{
    for(auto &f : fence_info_)
        wait(f);
}

void FrameSynchronizer::newFrame()
{
    frame_index_ = (frame_index_ + 1) % static_cast<int>(fence_info_.size());
    wait(fence_info_[frame_index_]);
}

void FrameSynchronizer::endFrame(vk::ArrayProxy<const Queue> queues)
{
    auto &info = fence_info_[frame_index_];

    while(info.fences.size() < queues.size())
        info.fences.push_back(device_.createFenceUnique({}));

    assert(info.used_fences.empty());
    for(uint32_t i = 0; i < queues.size(); ++i)
    {
        info.used_fences.push_back(info.fences[i].get());
        queues.data()[i].submit({}, {}, {}, {}, info.used_fences.back());
    }

    fence_info_[frame_index_].will_be_signaled = true;
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

void FrameSynchronizer::wait(FenceInfo &info)
{
    if(info.will_be_signaled)
    {
        (void)device_.waitForFences(info.used_fences, true, UINT64_MAX);
        info.will_be_signaled = false;
    }

    if(!info.used_fences.empty())
    {
        device_.resetFences(info.used_fences);
        info.used_fences.clear();
    }

    for(auto &func : info.delayed_functions)
        func();
    info.delayed_functions.clear();
}

VKPT_END
