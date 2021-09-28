#pragma once

#include <vkpt/allocator/semaphore_allocator.h>

VKPT_BEGIN

class PerFrameSemaphores :
    public agz::misc::uncopyable_t,
    public SemaphoreAllocator
{
public:

    PerFrameSemaphores();

    PerFrameSemaphores(vk::Device device, uint32_t frame_count);

    PerFrameSemaphores(PerFrameSemaphores &&other) noexcept;

    PerFrameSemaphores &operator=(PerFrameSemaphores &&other) noexcept;

    operator bool() const;

    void swap(PerFrameSemaphores &other) noexcept;

    void newFrame();

    vk::Semaphore newSemaphore() override;

    TimelineSemaphore newTimelineSemaphore() override;

private:

    struct PerFrame
    {
        std::vector<vk::UniqueSemaphore> semaphores;
        size_t next_available_semaphore = 0;

        std::vector<TimelineSemaphore> timeline_semaphores;
        size_t next_available_timeline_semaphore = 0;
    };

    vk::Device device_;

    int frame_index_;
    std::vector<PerFrame> frames_;
};

VKPT_END
