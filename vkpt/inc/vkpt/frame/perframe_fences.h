#pragma once

#include <vkpt/allocator/fence_allocator.h>

VKPT_BEGIN

class PerFrameFences : public FenceAllocator, public agz::misc::uncopyable_t
{
public:

    PerFrameFences();

    PerFrameFences(vk::Device device, uint32_t frame_count);

    PerFrameFences(PerFrameFences &&other) noexcept;

    PerFrameFences &operator=(PerFrameFences &&other) noexcept;

    operator bool() const;

    void swap(PerFrameFences &other) noexcept;

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
