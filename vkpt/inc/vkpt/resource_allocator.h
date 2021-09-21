#pragma once

#include <vk_mem_alloc.h>

#include <vkpt/object/buffer.h>
#include <vkpt/object/image.h>

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

    Buffer createBuffer(
        const vk::BufferCreateInfo &create_info, vma::MemoryUsage usage);

    Image createImage(
        const vk::ImageCreateInfo &create_info, vma::MemoryUsage usage);

private:

    void swap(ResourceAllocator &other) noexcept;

    vk::Device   device_;
    VmaAllocator allocator_;
};

VKPT_END
