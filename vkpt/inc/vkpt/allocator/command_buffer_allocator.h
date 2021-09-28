#pragma once

#include <vkpt/object/queue.h>
#include <vkpt/command_buffer.h>

VKPT_BEGIN

class CommandBufferAllocator
{
public:

    virtual ~CommandBufferAllocator() = default;

    virtual CommandBuffer newCommandBuffer(Queue::Type type) = 0;

    virtual CommandBuffer newGraphicsCommandBuffer()
    {
        return newCommandBuffer(Queue::Type::Graphics);
    }

    virtual CommandBuffer newComputeCommandBuffer()
    {
        return newCommandBuffer(Queue::Type::Compute);
    }

    virtual CommandBuffer newTransferCommandBuffer()
    {
        return newCommandBuffer(Queue::Type::Transfer);
    }

    virtual CommandBuffer newPresentCommandBuffer()
    {
        return newCommandBuffer(Queue::Type::Present);
    }
};

VKPT_END
