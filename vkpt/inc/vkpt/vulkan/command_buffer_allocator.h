#pragma once

#include <vkpt/vulkan/command_buffer.h>

VKPT_BEGIN

class CommandBufferAllocator
{
public:

    virtual ~CommandBufferAllocator() = default;

    virtual CommandBuffer newGraphicsCommandBuffer() = 0;

    virtual CommandBuffer newComputeCommandBuffer() = 0;

    virtual CommandBuffer newTransferCommandBuffer() = 0;
};

VKPT_END
