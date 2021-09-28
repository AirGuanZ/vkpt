#include <vkpt/resource/image_impl.h>

VKPT_BEGIN

Image::operator bool() const
{
    return impl_ != nullptr;
}

Image::operator vk::Image() const
{
    return get();
}

vk::Image Image::get() const
{
    return impl_ ? impl_->image : nullptr;
}

Image::State &Image::getState(const vk::ImageSubresource &subrsc)
{
    assert(impl_);
    const uint32_t index = impl_->computeStateIndex(
        subrsc.arrayLayer, subrsc.mipLevel, subrsc.aspectMask);
    return impl_->state[index];
}

const Image::State &Image::getState(const vk::ImageSubresource &subrsc) const
{
    assert(impl_);
    const uint32_t index = impl_->computeStateIndex(
        subrsc.arrayLayer, subrsc.mipLevel, subrsc.aspectMask);
    return impl_->state[index];
}

const Image::Description &Image::getDescription() const
{
    assert(impl_);
    return impl_->description;
}

vk::Device Image::getDevice() const
{
    assert(impl_);
    return impl_->device;
}

ImageView Image::createView(
    vk::ImageViewType                type,
    const vk::ImageSubresourceRange &range,
    vk::ComponentMapping             component_mapping) const
{
    assert(impl_);

    const vk::ImageViewCreateInfo create_info = {
        .image            = get(),
        .viewType         = type,
        .format           = getDescription().format,
        .components       = component_mapping,
        .subresourceRange = range
    };

    auto raw_image_view = impl_->device.createImageViewUnique(create_info);

    auto impl = std::make_shared<ImageView::Impl>();
    impl->image_view  = std::move(raw_image_view);
    impl->image       = *this;
    impl->description = ImageView::Description{
        .type              = type,
        .range             = range,
        .component_mapping = component_mapping
    };

    ImageView result;
    result.impl_ = std::move(impl);
    return result;
}

void Image::setName(std::string name)
{
    assert(impl_);
    impl_->name.swap(name);
}

const std::string &Image::getName() const
{
    assert(impl_);
    return impl_->name;
}

std::strong_ordering Image::operator<=>(const Image &rhs) const
{
    return get() <=> rhs.get();
}

vk::Image ImageSubresource::get() const
{
    return image.get();
}

const std::string &ImageSubresource::getName() const
{
    return image.getName();
}

const ResourceState &ImageSubresource::getState() const
{
    return image.getState(subrsc);
}

ResourceState &ImageSubresource::getState()
{
    return image.getState(subrsc);
}

ImageView::operator bool() const
{
    return impl_ != nullptr;
}

ImageView::operator vk::ImageView() const
{
    return get();
}

vk::ImageView ImageView::get() const
{
    return impl_ ? impl_->image_view.get() : nullptr;
}

const ImageView::Description &ImageView::getDescription() const
{
    assert(impl_);
    return impl_->description;
}

const Image &ImageView::getImage() const
{
    assert(impl_);
    return impl_->image;
}

std::strong_ordering ImageView::operator<=>(const ImageView &rhs) const
{
    return get() <=> rhs.get();
}

VKPT_END
