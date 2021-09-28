#pragma once

#include <vkpt/common.h>

VKPT_BEGIN

class FenceAllocator
{
public:

    virtual ~FenceAllocator() = default;

    virtual vk::Fence newFence() = 0;
};

VKPT_END
