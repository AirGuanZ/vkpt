#include <vkpt/allocator/resource_allocator.h>
#include <vkpt/resource/buffer_impl.h>
#include <vkpt/resource/image_impl.h>

VKPT_BEGIN

ResourceAllocator::ResourceAllocator()
    : allocator_(nullptr)
{
    
}

ResourceAllocator::ResourceAllocator(
    vk::Instance       instance,
    vk::PhysicalDevice physical_device,
    vk::Device         device)
    : device_(device), allocator_(nullptr)
{
    VmaAllocatorCreateInfo create_info = {};
    create_info.instance       = instance;
    create_info.physicalDevice = physical_device;
    create_info.device         = device;

    auto rt = vmaCreateAllocator(&create_info, &allocator_);
    if(rt != VK_SUCCESS)
    {
        throw VKPTException(
            "failed to create vma allocator. err code is " +
            std::to_string(rt));
    }
}

ResourceAllocator::ResourceAllocator(ResourceAllocator &&other) noexcept
    : ResourceAllocator()
{
    swap(other);
}

ResourceAllocator &ResourceAllocator::operator=(ResourceAllocator &&other) noexcept
{
    swap(other);
    return *this;
}

ResourceAllocator::~ResourceAllocator()
{
    if(allocator_)
        vmaDestroyAllocator(allocator_);
}

Buffer ResourceAllocator::createBuffer(
    const vk::BufferCreateInfo &create_info, vma::MemoryUsage usage)
{
    VkBufferCreateInfo vk_create_info = create_info;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = static_cast<VmaMemoryUsage>(usage);

    VkBuffer buffer; VmaAllocation allocation;
    auto rt = vmaCreateBuffer(
        allocator_, &vk_create_info, &alloc_info,
        &buffer, &allocation, nullptr);
    if(rt != VK_SUCCESS)
    {
        throw VKPTException(
            "failed to create vma buffer. err code is " +
            std::to_string(rt));
    }

    AGZ_SCOPE_FAIL{ vmaDestroyBuffer(allocator_, buffer, allocation); };

    const Buffer::Description description = {
        .size         = create_info.size,
        .usage        = create_info.usage,
        .sharing_mode = create_info.sharingMode
    };

    auto raw_impl = new Buffer::Impl{
        .device      = device_,
        .buffer      = buffer,
        .description = description,
        .state       = FreeState{},
        .allocation  = allocation,
        .allocator   = allocator_
    };

    auto impl = std::shared_ptr<Buffer::Impl>(
        raw_impl, [allocator = allocator_, allocation](Buffer::Impl *p)
    {
        assert(p && p->buffer);
        vmaDestroyBuffer(allocator, p->buffer, allocation);
        delete p;
    });

    Buffer result;
    result.impl_ = std::move(impl);
    return result;
}

Image ResourceAllocator::createImage(
    const vk::ImageCreateInfo &create_info, vma::MemoryUsage usage)
{
    VkImageCreateInfo vk_create_info = create_info;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = static_cast<VmaMemoryUsage>(usage);

    VkImage image; VmaAllocation allocation;
    auto rt = vmaCreateImage(
        allocator_, &vk_create_info, &alloc_info,
        &image, &allocation, nullptr);
    if(rt != VK_SUCCESS)
    {
        throw VKPTException(
            "failed to create vma image. err code is " +
            std::to_string(rt));
    }

    AGZ_SCOPE_FAIL{ vmaDestroyImage(allocator_, image, allocation); };

    const Image::Description description = {
        .type         = create_info.imageType,
        .format       = create_info.format,
        .samples      = create_info.samples,
        .extent       = create_info.extent,
        .sharing_mode = create_info.sharingMode,
        .mip_levels   = create_info.mipLevels,
        .array_layers = create_info.arrayLayers
    };

    auto raw_impl = new Image::Impl{
        .device      = device_,
        .image       = image,
        .description = description
    };

    auto impl = std::shared_ptr<Image::Impl>(
        raw_impl, [allocator = allocator_, allocation](Image::Impl *p)
    {
        assert(p &&p->image);
        vmaDestroyImage(allocator, p->image, allocation);
        delete p;
    });

    raw_impl->initializeStateIndices();
    const uint32_t state_count = raw_impl->A * create_info.arrayLayers;
    raw_impl->state = std::make_unique<Image::State[]>(state_count);
    for(uint32_t i = 0; i < state_count; ++i)
        raw_impl->state[i] = FreeState{ create_info.initialLayout };

    Image result;
    result.impl_ = std::move(impl);
    return result;
}

void ResourceAllocator::swap(ResourceAllocator &other) noexcept
{
    std::swap(allocator_, other.allocator_);
}

VKPT_END
