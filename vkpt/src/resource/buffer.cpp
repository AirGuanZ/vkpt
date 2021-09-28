#include <vk_mem_alloc.h>

#include <vkpt/resource/buffer_impl.h>

VKPT_BEGIN

Buffer::operator bool() const
{
    return impl_ != nullptr;
}

Buffer::operator vk::Buffer() const
{
    return get();
}

vk::Buffer Buffer::get() const
{
    return impl_ ? impl_->buffer : nullptr;
}

Buffer::State &Buffer::getState()
{
    assert(impl_);
    return impl_->state;
}

const Buffer::State &Buffer::getState() const
{
    assert(impl_);
    return impl_->state;
}

const Buffer::Description &Buffer::getDescription() const
{
    assert(impl_);
    return impl_->description;
}

vk::Device Buffer::getDevice() const
{
    assert(impl_);
    return impl_->device;
}

void Buffer::setName(std::string name)
{
    assert(impl_);
    impl_->name.swap(name);
}

const std::string &Buffer::getName() const
{
    assert(impl_);
    return impl_->name;
}

void *Buffer::map() const
{
    assert(impl_);
    if(!impl_->allocation)
    {
        throw VKPTException(
            "buffer {} is not created by vma, thus cannot be mapped",
            getName());
    }

    void *ret;
    auto rt = vmaMapMemory(impl_->allocator, impl_->allocation, &ret);
    if(rt != VK_SUCCESS)
    {
        throw std::runtime_error(
            "failed to map vma buffer. error code is " +
            std::to_string(rt));
    }
    return ret;
}

void Buffer::unmap()
{
    assert(impl_ && impl_->allocation);
    vmaUnmapMemory(impl_->allocator, impl_->allocation);
}

std::strong_ordering Buffer::operator<=>(const Buffer &rhs) const
{
    return get() <=> rhs.get();
}

VKPT_END
