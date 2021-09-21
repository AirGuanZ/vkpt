#include <vkpt/immediate_command_buffer.h>

VKPT_BEGIN

ImmediateCommandBuffer::ImmediateCommandBuffer(Queue *queue)
{
    queue_ = queue;

    pool_ = queue->getDevice().createCommandPoolUnique(
        vk::CommandPoolCreateInfo{
                .queueFamilyIndex = queue->getFamilyIndex()
        });

    buffer_ = std::move(queue->getDevice().allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo{
            .commandPool        = pool_.get(),
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        }).front());
    buffer_->begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });

    fence_ = queue->getDevice().createFenceUnique({});

    impl_ = buffer_.get();
}

ImmediateCommandBuffer::ImmediateCommandBuffer(
    ImmediateCommandBuffer &&other) noexcept
{
    swap(other);
}

ImmediateCommandBuffer &ImmediateCommandBuffer::operator=(
    ImmediateCommandBuffer &&other) noexcept
{
    swap(other);
    return *this;
}

void ImmediateCommandBuffer::swap(ImmediateCommandBuffer &other) noexcept
{
    std::swap(queue_, other.queue_);
    std::swap(pool_, other.pool_);
    std::swap(buffer_, other.buffer_);
    std::swap(fence_, other.fence_);
    std::swap(impl_, other.impl_);
}

ImmediateCommandBuffer::operator bool() const
{
    return impl_;
}

void ImmediateCommandBuffer::submitAndSync()
{
    buffer_->end();
    vk::CommandBuffer cmds[] = { buffer_.get() };
    queue_->getRaw().submit(
        std::array{
            vk::SubmitInfo{
                .commandBufferCount = 1,
                .pCommandBuffers    = cmds
            }
        }, fence_.get());

    (void)queue_->getDevice().waitForFences(
        std::array{ fence_.get() }, true, UINT64_MAX);

    queue_->getDevice().resetFences(std::array{ fence_.get() });
    queue_->getDevice().resetCommandPool(pool_.get());
    buffer_->begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });
}

VKPT_END
