#pragma once

#include <vkpt/vulkan/command_buffer.h>
#include <vkpt/vulkan/queue.h>

VKPT_BEGIN

class ImmediateCommandBuffer :
    public CommandBuffer, public agz::misc::uncopyable_t
{
public:

    ImmediateCommandBuffer() = default;

    explicit ImmediateCommandBuffer(Queue queue);

    ImmediateCommandBuffer(ImmediateCommandBuffer &&other) noexcept;

    ImmediateCommandBuffer &operator=(ImmediateCommandBuffer &&other) noexcept;

    void swap(ImmediateCommandBuffer &other) noexcept;

    operator bool() const;

    void submitAndSync();

private:

    Queue                   queue_;
    vk::UniqueCommandPool   pool_;
    vk::UniqueCommandBuffer buffer_;
    vk::UniqueFence         fence_;
};

VKPT_END
