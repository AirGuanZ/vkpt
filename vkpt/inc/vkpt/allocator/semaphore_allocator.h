#pragma once

#include <vkpt/object/semaphore.h>

VKPT_BEGIN

class SemaphoreAllocator
{
public:

    virtual ~SemaphoreAllocator() = default;

    virtual vk::Semaphore newSemaphore() = 0;

    virtual TimelineSemaphore newTimelineSemaphore() = 0;
};

VKPT_END
