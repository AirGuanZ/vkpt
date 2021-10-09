#include <vkpt/object/pipeline.h>
#include <vkpt/command_buffer.h>

VKPT_BEGIN

CommandBuffer::CommandBuffer()
    : impl_(nullptr)
{
    
}

CommandBuffer::CommandBuffer(vk::CommandBuffer impl)
    : impl_(impl)
{
    
}

const vk::CommandBuffer CommandBuffer::getRaw() const
{
    return impl_;
}

void CommandBuffer::begin(bool one_time_submit)
{
    vk::CommandBufferUsageFlags flags = {};
    if(one_time_submit)
        flags |= vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    impl_.begin(vk::CommandBufferBeginInfo{
        .flags = flags
    });
}

void CommandBuffer::end()
{
    impl_.end();
}

void CommandBuffer::beginPipeline(
    const Pipeline                      &pipeline,
    const Framebuffer                   &framebuffer,
    vk::ArrayProxy<const vk::ClearValue> clear_values)
{
    const auto &fb_desc = framebuffer.getDescription();

    const vk::Rect2D render_area = {
        .offset = { 0, 0 },
        .extent = { fb_desc.width, fb_desc.height }
    };

    impl_.beginRenderPass(vk::RenderPassBeginInfo{
        .renderPass      = pipeline.getRenderPass(),
        .framebuffer     = framebuffer,
        .renderArea      = render_area,
        .clearValueCount = clear_values.size(),
        .pClearValues    = clear_values.data()
    }, vk::SubpassContents::eInline);

    impl_.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
}

void CommandBuffer::endPipeline()
{
    impl_.endRenderPass();
}

void CommandBuffer::setViewport(vk::ArrayProxy<const vk::Viewport> viewports)
{
    impl_.setViewport(0, viewports.size(), viewports.data());
}

void CommandBuffer::setScissor(vk::ArrayProxy<const vk::Rect2D> scissors)
{
    impl_.setScissor(0, scissors.size(), scissors.data());
}

void CommandBuffer::bindVertexBuffers(
    vk::ArrayProxy<const vk::Buffer> vertex_buffers,
    vk::ArrayProxy<size_t>           vertex_offsets)
{
    assert(vertex_buffers.size() == vertex_offsets.size() ||
           vertex_offsets.empty());

    if(vertex_offsets.empty())
    {
        assert(vertex_buffers.size() < 32);
        std::array<size_t, 32> offsets = {};
        impl_.bindVertexBuffers(
            0, vertex_buffers.size(),
            vertex_buffers.data(), offsets.data());
    }
    else
    {
        impl_.bindVertexBuffers(
            0, vertex_buffers.size(),
            vertex_buffers.data(), vertex_offsets.data());
    }
}

void CommandBuffer::draw(
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t first_vertex,
    uint32_t first_instance)
{
    impl_.draw(vertex_count, instance_count, first_vertex, first_instance);
}

void CommandBuffer::pipelineBarrier(
    vk::PipelineStageFlags                        src_stage,
    vk::PipelineStageFlags                        dst_stage,
    vk::ArrayProxy<const vk::MemoryBarrier>       memory_barriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier> buffer_barriers,
    vk::ArrayProxy<const vk::ImageMemoryBarrier>  image_barriers)
{
    impl_.pipelineBarrier(
        src_stage, dst_stage, {},
        memory_barriers, buffer_barriers, image_barriers);
}

void CommandBuffer::pipelineBarrier(
    vk::ArrayProxy<const vk::MemoryBarrier2KHR>       memory_barriers,
    vk::ArrayProxy<const vk::BufferMemoryBarrier2KHR> buffer_barriers,
    vk::ArrayProxy<const vk::ImageMemoryBarrier2KHR>  image_barriers)
{
    impl_.pipelineBarrier2KHR(vk::DependencyInfoKHR{
        .memoryBarrierCount       = memory_barriers.size(),
        .pMemoryBarriers          = memory_barriers.data(),
        .bufferMemoryBarrierCount = buffer_barriers.size(),
        .pBufferMemoryBarriers    = buffer_barriers.data(),
        .imageMemoryBarrierCount  = image_barriers.size(),
        .pImageMemoryBarriers     = image_barriers.data()
    });
}

VKPT_END
