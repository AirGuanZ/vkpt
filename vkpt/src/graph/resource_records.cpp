#include <vkpt/graph/resource_records.h>

VKPT_GRAPH_BEGIN

ResourceRecords::ResourceRecords(std::pmr::memory_resource &memory)
    : memory_(memory), compile_buffers_(&memory), compile_images_(&memory)
{
    
}

void ResourceRecords::build(std::span<CompilePass*> sorted_passes)
{
    assert(compile_buffers_.empty() && compile_images_.empty());

    auto create_compile_buffer = agz::misc::lazy_construct(
        [this] { return CompileBuffer(memory_); });

    auto create_compile_image = agz::misc::lazy_construct(
        [this] { return CompileImage(memory_); });

    for(auto pass : sorted_passes)
    {
        for(auto &[buffer, usage] : pass->raw_pass->_bufferUsages())
        {
            auto &compile_buffer = compile_buffers_.try_emplace(
                buffer, create_compile_buffer).first->second;

            compile_buffer.usages.push_back(CompileBufferUsage{
                .pass       = pass,
                .stages     = usage.stages,
                .access     = usage.access,
                .end_stages = usage.stages,
                .end_access = usage.access
            });
        }

        for(auto &[image_subrsc, usage] : pass->raw_pass->_imageUsages())
        {
            auto &compile_image = compile_images_.try_emplace(
                image_subrsc, create_compile_image).first->second;

            compile_image.usages.push_back(CompileImageUsage{
                .pass       = pass,
                .stages     = usage.stages,
                .access     = usage.access,
                .layout     = usage.layout,
                .end_stages = usage.stages,
                .end_access = usage.access,
                .end_layout = usage.layout
            });
        }
    }
}

Map<Buffer, CompileBuffer> &ResourceRecords::getBuffers()
{
    return compile_buffers_;
}

Map<ImageSubresource, CompileImage> &ResourceRecords::getImages()
{
    return compile_images_;
}

CompileBuffer &ResourceRecords::getRecord(const Buffer &buffer)
{
    return compile_buffers_.at(buffer);
}

CompileImage &ResourceRecords::getRecord(const ImageSubresource &image_subrsc)
{
    return compile_images_.at(image_subrsc);
}

const CompileBuffer &ResourceRecords::getRecord(const Buffer &buffer) const
{
    return compile_buffers_.at(buffer);
}

const CompileImage &ResourceRecords::getRecord(
    const ImageSubresource &image_subrsc) const
{
    return compile_images_.at(image_subrsc);
}

VKPT_GRAPH_END
