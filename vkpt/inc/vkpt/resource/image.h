#pragma once

#include <agz-utils/misc.h>

#include <vkpt/object/queue.h>
#include <vkpt/resource/buffer.h>

VKPT_BEGIN

class ImageView;

class Image
{
public:

    struct Description
    {
        vk::ImageType           type;
        vk::Format              format;
        vk::SampleCountFlagBits samples;
        vk::Extent3D            extent;
        vk::SharingMode         sharing_mode;
        uint32_t                mip_levels;
        uint32_t                array_layers;
    };

    using State = ResourceState;

    operator bool() const;

    operator vk::Image() const;

    vk::Image get() const;

    State &getState(const vk::ImageSubresource &subrsc);

    const State &getState(const vk::ImageSubresource &subrsc) const;

    const Description &getDescription() const;

    vk::Device getDevice() const;

    ImageView createView(
        vk::ImageViewType                type,
        const vk::ImageSubresourceRange &range,
        vk::ComponentMapping             component_mapping) const;

    void setName(std::string name);

    const std::string &getName() const;

    std::strong_ordering operator<=>(const Image &rhs) const;

private:

    friend class Context;
    friend class ResourceAllocator;

    struct Impl;

    std::shared_ptr<Impl> impl_;
};

struct ImageSubresource
{
    Image                image;
    vk::ImageSubresource subrsc;

    vk::Image get() const;

    const std::string &getName() const;

    const ResourceState &getState() const;

    ResourceState &getState();

    auto operator<=>(const ImageSubresource &) const = default;
};

class ImageView
{
public:

    struct Description
    {
        vk::ImageViewType         type;
        vk::ImageSubresourceRange range;
        vk::ComponentMapping      component_mapping;
    };

    operator bool() const;

    operator vk::ImageView() const;

    vk::ImageView get() const;

    const Description &getDescription() const;

    const Image &getImage() const;

    std::strong_ordering operator<=>(const ImageView &rhs) const;

private:

    friend class Image;

    struct Impl;

    std::shared_ptr<Impl> impl_;
};

VKPT_END
