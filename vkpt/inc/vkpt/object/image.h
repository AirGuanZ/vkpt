#pragma once

#include <vk_mem_alloc.h>

#include <vkpt/object/object.h>

VKPT_BEGIN

class Image;
class ImageView;

struct ImageDescription
{
    vk::Device              device;
    vk::ImageType           type;
    vk::Format              format;
    vk::SampleCountFlagBits samples;
    vk::Extent3D            extent;
    vk::SharingMode         sharing_mode;
    uint32_t                mip_levels;
    uint32_t                array_layers;

    VmaAllocation allocation = nullptr;
    VmaAllocator  allocator = nullptr;
};

class Image :
    public Object<vk::Image, vk::Image, ImageDescription>
{
    struct ImageDeleter
    {
        void operator()(Record *record) const
        {
            vk::Image image = record->object;
            ImageDescription &desc = record->description;
            if(desc.allocation)
                vmaDestroyImage(desc.allocator, image, desc.allocation);
            delete record;
        }
    };

public:

    Image() = default;

    Image(vk::Image image, const ImageDescription &description);

    VKPT_USING_OBJECT_METHODS(vk::Image);

    ImageView createView(
        vk::ImageViewType type, const vk::ImageSubresourceRange &range) const;

    auto operator<=>(const Image &rhs) const = default;
};

struct ImageViewDescription
{
    Image                     image;
    vk::ImageViewType         type;
    vk::ComponentMapping      components;
    vk::ImageSubresourceRange range;
};

class ImageView :
    public Object<vk::ImageView, vk::UniqueImageView, ImageViewDescription>
{
public:

    using Object::Object;

    VKPT_USING_OBJECT_METHODS(vk::ImageView);

    const Image &getImage() const;

    auto operator<=>(const ImageView &rhs) const = default;
};

inline Image::Image(vk::Image image, const ImageDescription &description)
    : Object<vk::Image, vk::Image, ImageDescription>(
        image, description, ImageDeleter())
{

}

inline ImageView Image::createView(
    vk::ImageViewType                type,
    const vk::ImageSubresourceRange &range) const
{
    const vk::ImageViewCreateInfo create_info = {
        .image            = *this,
        .viewType         = type,
        .format           = getDescription().format,
        .components       = {},
        .subresourceRange = range
    };
    auto view = getDescription().device.createImageViewUnique(create_info);

    return ImageView(std::move(view), { *this, type, {}, range });
}

inline const Image &ImageView::getImage() const
{
    return getDescription().image;
}

VKPT_END
