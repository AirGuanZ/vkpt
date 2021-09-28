#pragma once

#include <agz-utils/misc.h>

#include <vkpt/object/queue.h>

VKPT_BEGIN

struct FreeState
{
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
};

struct UsingState
{
    const Queue               *queue;
    vk::PipelineStageFlags2KHR stages;
    vk::AccessFlags2KHR        access;
    vk::ImageLayout            layout = vk::ImageLayout::eUndefined;
};

struct ReleasedState
{
    const Queue    *src_queue;
    const Queue    *dst_queue;
    vk::ImageLayout old_layout = vk::ImageLayout::eUndefined;
    vk::ImageLayout new_layout = vk::ImageLayout::eUndefined;
};

using ResourceState = agz::misc::variant_t<FreeState, UsingState, ReleasedState>;

class Buffer
{
public:

    struct Description
    {
        size_t               size;
        vk::BufferUsageFlags usage;
        vk::SharingMode      sharing_mode;
    };

    using State = ResourceState;

    operator bool() const;

    operator vk::Buffer() const;

    vk::Buffer get() const;

    State &getState();

    const State &getState() const;

    const Description &getDescription() const;

    vk::Device getDevice() const;

    void setName(std::string name);

    const std::string &getName() const;

    void *map() const;

    void unmap();

    std::strong_ordering operator<=>(const Buffer &rhs) const;

private:

    friend class ResourceAllocator;

    struct Impl;

    std::shared_ptr<Impl> impl_;
};

VKPT_END
