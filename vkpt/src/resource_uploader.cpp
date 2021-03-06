#include <vkpt/resource_uploader.h>

VKPT_BEGIN

ResourceUploader::ResourceUploader(
    Queue *queue, ResourceAllocator &resource_allocator)
    : queue_(queue), resource_allocator_(resource_allocator)
{
    command_pool_ = queue_->getDevice().createCommandPoolUnique(
        vk::CommandPoolCreateInfo{
            .queueFamilyIndex = queue->getFamilyIndex()
        });

    command_buffer_ = std::move(queue_->getDevice().allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo{
            .commandPool        = command_pool_.get(),
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        }).front());

    command_buffer_->begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });

    sync_fence_ = queue_->getDevice().createFenceUnique({ });
}

ResourceUploader::~ResourceUploader()
{
    assert(buffer_memory_barriers_.empty());
    assert(staging_resources_.empty());
}

void ResourceUploader::uploadBuffer(
    Buffer      dst_buffer,
    const void *data,
    size_t      bytes)
{
    assert(dst_buffer.getState().is<FreeState>());

    is_dirty_ = true;

    if(bytes < 256)
    {
        command_buffer_->updateBuffer(dst_buffer, 0, bytes, data);
        return;
    }

    auto staging_buffer = resource_allocator_.createBuffer(
        vk::BufferCreateInfo{
            .size        = bytes,
            .usage       = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive
        },
        vma::MemoryUsage::eCPUToGPU);

    auto mapped_ptr = staging_buffer.map();
    std::memcpy(mapped_ptr, data, bytes);
    staging_buffer.unmap();

    vk::BufferCopy region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = bytes
    };
    command_buffer_->copyBuffer(
        staging_buffer, dst_buffer, std::array{ region });

    StagingBuffer staging_resource;
    staging_resource.buffer = std::move(staging_buffer);
    staging_resources_.push_back(
        std::make_unique<StagingBuffer>(std::move(staging_resource)));

    dst_buffer.getState() = UsingState{
        .queue  = queue_,
        .stages = vk::PipelineStageFlagBits2KHR::eNone,
        .access = vk::AccessFlagBits2KHR::eNone
    };
}

void ResourceUploader::submitAndSync()
{
    if(!is_dirty_)
        return;
    is_dirty_ = false;

    command_buffer_->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {},
        0, nullptr,
        static_cast<uint32_t>(buffer_memory_barriers_.size()),
        buffer_memory_barriers_.data(),
        0, nullptr);
    command_buffer_->end();

    vk::CommandBuffer submit_command_buffers[] = { command_buffer_.get() };

    queue_->getRaw().submit(std::array{ vk::SubmitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers    = submit_command_buffers
    } }, sync_fence_.get());

    (void)queue_->getDevice().waitForFences(
        std::array{ sync_fence_.get() }, true, UINT64_MAX);

    queue_->getDevice().resetFences(std::array{ sync_fence_.get() });
    queue_->getDevice().resetCommandPool(command_pool_.get());

    command_buffer_->begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });

    buffer_memory_barriers_.clear();
    staging_resources_.clear();
}

VKPT_END
