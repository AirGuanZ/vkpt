#pragma once

#include <vkpt/common.h>

VKPT_BEGIN

class SemaphoreAllocator
{
public:

    virtual ~SemaphoreAllocator() = default;

    virtual vk::Semaphore newSemaphore() = 0;
};

VKPT_END
