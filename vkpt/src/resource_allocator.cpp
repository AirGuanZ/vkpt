#include <vkpt/resource_allocator.h>

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

    const BufferDescription description = {
        .size         = create_info.size,
        .usage        = create_info.usage,
        .sharing_mode = create_info.sharingMode,
        .allocation   = allocation,
        .allocator    = allocator_
    };

    return Buffer(buffer, description);
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

    const ImageDescription description = {
        .device       = device_,
        .type         = create_info.imageType,
        .format       = create_info.format,
        .samples      = create_info.samples,
        .extent       = create_info.extent,
        .sharing_mode = create_info.sharingMode,
        .mip_levels   = create_info.mipLevels,
        .array_layers = create_info.arrayLayers,
        .allocation   = allocation,
        .allocator    = allocator_
    };
    return Image(image, description);
}

void ResourceAllocator::swap(ResourceAllocator &other) noexcept
{
    std::swap(allocator_, other.allocator_);
}

VKPT_END
