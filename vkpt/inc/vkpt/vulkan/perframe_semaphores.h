#pragma once

#include <vkpt/vulkan/semaphore_allocator.h>

VKPT_BEGIN

class PerFrameSemaphores :
    public agz::misc::uncopyable_t,
    public SemaphoreAllocator
{
public:

    PerFrameSemaphores(vk::Device device, uint32_t frame_count);

    void newFrame();

    vk::Semaphore newSemaphore() override;

private:

    struct PerFrame
    {
        std::vector<vk::UniqueSemaphore> semaphores;
        size_t next_available_semaphore = 0;
    };

    vk::Device device_;

    int frame_index_;
    std::vector<PerFrame> frames_;
};

VKPT_END
