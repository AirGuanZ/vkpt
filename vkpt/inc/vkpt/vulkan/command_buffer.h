#pragma once

#include <vkpt/vulkan/pipeline.h>

VKPT_BEGIN

class CommandBuffer
{
public:

    CommandBuffer();

    CommandBuffer(vk::CommandBuffer impl);

    const vk::CommandBuffer getRaw() const;

    void begin(bool one_time_submit = true);

    void end();

    void beginPipeline(
        const Pipeline                      &pipeline,
        vk::Framebuffer                      framebuffer,
        vk::ArrayProxy<const vk::ClearValue> clear_values);

    void endPipeline();

    void bindVertexBuffers(
        vk::ArrayProxy<const vk::Buffer> vertex_buffers,
        vk::ArrayProxy<size_t>           vertex_offsets = {});

    void draw(
        uint32_t vertex_count,
        uint32_t instance_count = 1,
        uint32_t first_vertex   = 0,
        uint32_t first_instance = 0);

    void pipelineBarriers(
        vk::PipelineStageFlags                        src_stage,
        vk::PipelineStageFlags                        dst_stage,
        vk::ArrayProxy<const vk::MemoryBarrier>       memory_barriers,
        vk::ArrayProxy<const vk::BufferMemoryBarrier> buffer_barriers,
        vk::ArrayProxy<const vk::ImageMemoryBarrier>  image_barriers);

protected:

    vk::CommandBuffer impl_;
};

VKPT_END
