#pragma once

#include <span>

#include <vkpt/graph/compile_internal.h>

VKPT_GRAPH_BEGIN

class ResourceRecords
{
public:

    explicit ResourceRecords(std::pmr::memory_resource &memory);

    void build(std::span<CompilePass *> sorted_passes);

    Map<Buffer, CompileBuffer> &getBuffers();

    Map<ImageSubresource, CompileImage> &getImages();

    CompileBuffer &getRecord(const Buffer &buffer);

    CompileImage &getRecord(const ImageSubresource &image_subrsc);

    const CompileBuffer &getRecord(const Buffer &buffer) const;

    const CompileImage &getRecord(const ImageSubresource &image_subrsc) const;

private:

    std::pmr::memory_resource &memory_;

    Map<Buffer, CompileBuffer>          compile_buffers_;
    Map<ImageSubresource, CompileImage> compile_images_;
};

VKPT_GRAPH_END
