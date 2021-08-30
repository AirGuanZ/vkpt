#pragma once

#include <vkpt/vulkan/fence_allocator.h>

VKPT_BEGIN

class PerFrameFences : public agz::misc::uncopyable_t, public FenceAllocator
{
public:

    PerFrameFences(vk::Device device, uint32_t frame_count);

    void newFrame();

    vk::Fence newFence() override;

private:

    struct PerFrame
    {
        std::vector<vk::UniqueFence> fences;
        size_t next_available_fence = 0;
    };

    vk::Device device_;

    int frame_index_;
    std::vector<PerFrame> frames_;
};

VKPT_END
