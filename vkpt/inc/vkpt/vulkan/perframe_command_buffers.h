#pragma once

#include <vkpt/vulkan/queue.h>

VKPT_BEGIN

class PerFrameCommandBuffers : public agz::misc::uncopyable_t
{
public:

    PerFrameCommandBuffers(
        vk::Device device,
        int        frame_count,
        uint32_t   queue_family);
    
    void newFrame();

    vk::CommandBuffer newCommandBuffer();
    
private:

    struct PerFrame
    {
        vk::UniqueCommandPool                pool;
        std::vector<vk::UniqueCommandBuffer> buffers;
        size_t                               next_available_buffer_index = 0;
    };

    vk::Device device_;

    int frame_index_;
    std::vector<PerFrame> frames_;

    uint32_t queue_family_;
};

VKPT_END
