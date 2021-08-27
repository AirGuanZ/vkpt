#pragma once

#include <vkpt/common.h>

VKPT_BEGIN

namespace vma
{

    enum class MemoryUsage
    {
        eUnknown            = VMA_MEMORY_USAGE_UNKNOWN,
        eGPUOnly            = VMA_MEMORY_USAGE_GPU_ONLY,
        eCPUOnly            = VMA_MEMORY_USAGE_CPU_ONLY,
        eCPUToGPU           = VMA_MEMORY_USAGE_CPU_TO_GPU,
        eGPUToCPU           = VMA_MEMORY_USAGE_GPU_TO_CPU,
        eCPUCopy            = VMA_MEMORY_USAGE_CPU_COPY,
        eGPULazilyAllocated = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED
    };

} // namespace vma

template<typename Resource>
class UniqueResource : public agz::misc::uncopyable_t
{
public:

    static_assert(
        std::is_same_v<Resource, vk::Buffer> ||
        std::is_same_v<Resource, vk::Image>);

    UniqueResource();

    UniqueResource(
        Resource resource, VmaAllocation allocation, VmaAllocator allocator);

    UniqueResource(UniqueResource &&other) noexcept;

    UniqueResource &operator=(UniqueResource &&other) noexcept;

    ~UniqueResource();

    void swap(UniqueResource &other) noexcept;

    operator bool() const;

    void reset(
        Resource resource, VmaAllocation allocation, VmaAllocator allocator);

    void reset();

    Resource get() const;

    void *map() const;

    void unmap() const;

private:

    Resource      resource_;
    VmaAllocation allocation_;
    VmaAllocator  allocator_;
};

using UniqueBuffer = UniqueResource<vk::Buffer>;
using UniqueImage  = UniqueResource<vk::Image>;

class ResourceAllocator : public agz::misc::uncopyable_t
{
public:

    ResourceAllocator();

    ResourceAllocator(
        vk::Instance       instance,
        vk::PhysicalDevice physical_device,
        vk::Device         device);

    ResourceAllocator(ResourceAllocator &&other) noexcept;

    ResourceAllocator &operator=(ResourceAllocator &&other) noexcept;

    ~ResourceAllocator();

    UniqueBuffer createBuffer(
        const vk::BufferCreateInfo &create_info, vma::MemoryUsage usage);

    UniqueImage createImage(
        const vk::ImageCreateInfo &create_info, vma::MemoryUsage usage);

private:

    void swap(ResourceAllocator &other) noexcept;

    VmaAllocator allocator_;
};


template<typename Resource>
UniqueResource<Resource>::UniqueResource()
    : UniqueResource(nullptr, nullptr, nullptr)
{
    
}

template<typename Resource>
UniqueResource<Resource>::UniqueResource(
    Resource resource, VmaAllocation allocation, VmaAllocator allocator)
    : resource_(resource), allocation_(allocation), allocator_(allocator)
{
    
}

template<typename Resource>
UniqueResource<Resource>::UniqueResource(UniqueResource &&other) noexcept
    : UniqueResource()
{
    swap(other);
}

template<typename Resource>
UniqueResource<Resource> &UniqueResource<Resource>::operator=(
    UniqueResource &&other) noexcept
{
    swap(other);
    return *this;
}

template<typename Resource>
UniqueResource<Resource>::~UniqueResource()
{
    reset();
}

template<typename Resource>
void UniqueResource<Resource>::swap(UniqueResource &other) noexcept
{
    std::swap(resource_, other.resource_);
    std::swap(allocation_, other.allocation_);
    std::swap(allocator_, other.allocator_);
}

template<typename Resource>
UniqueResource<Resource>::operator bool() const
{
    return !!resource_;
}

template<typename Resource>
void UniqueResource<Resource>::reset(
    Resource resource, VmaAllocation allocation, VmaAllocator allocator)
{
    if(resource_)
    {
        if constexpr(std::is_same_v<Resource, vk::Buffer>)
            vmaDestroyBuffer(allocator_, resource_, allocation_);
        else
            vmaDestroyImage(allocator_, resource_, allocation_);
    }
    resource_   = resource;
    allocation_ = allocation;
    allocator_  = allocator;
}

template<typename Resource>
void UniqueResource<Resource>::reset()
{
    reset(nullptr, nullptr, nullptr);
}

template<typename Resource>
Resource UniqueResource<Resource>::get() const
{
    return resource_;
}

template<typename Resource>
void *UniqueResource<Resource>::map() const
{
    static_assert(std::is_same_v<Resource, vk::Buffer>);
    void *ret;
    auto rt = vmaMapMemory(allocator_, allocation_, &ret);
    if(rt != VK_SUCCESS)
    {
        throw std::runtime_error(
            "failed to map vma buffer. error code is " +
            std::to_string(rt));
    }
    return ret;
}

template<typename Resource>
void UniqueResource<Resource>::unmap() const
{
    static_assert(std::is_same_v<Resource, vk::Buffer>);
    vmaUnmapMemory(allocator_, allocation_);
}

VKPT_END
