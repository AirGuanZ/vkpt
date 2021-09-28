#include <vkpt/graph/compile_internal.h>

VKPT_GRAPH_BEGIN

CompilePass::CompilePass(std::pmr::memory_resource &memory)
    : raw_pass(nullptr),
      queue(nullptr),
      group(nullptr),
      unprocessed_head_count(0),
      sorted_index(-1),
      sorted_index_in_group(-1),
      group_flags(0),
      tails(&memory),
      heads(&memory),
      pre_ext_buffer_barriers(&memory),
      pre_ext_image_barriers(&memory),
      post_ext_buffer_barriers(&memory),
      post_ext_image_barriers(&memory),
      pre_buffer_barriers(&memory),
      pre_image_barriers(&memory),
      wait_semaphores(&memory),
      signal_semaphores(&memory),
      generated_buffer_usages_(&memory),
      generated_image_usages_(&memory)
{
    
}

CompileBufferUsage::operator Pass::BufferUsage() const
{
    return Pass::BufferUsage{
        .stages     = stages,
        .access     = access,
        .end_stages = end_stages,
        .end_access = end_access
    };
}

CompileImageUsage::operator Pass::ImageUsage() const
{
    return Pass::ImageUsage{
        .stages     = stages,
        .access     = access,
        .layout     = layout,
        .end_stages = end_stages,
        .end_access = end_access,
        .end_layout = end_layout
    };
}

CompileGroup::CompileGroup(std::pmr::memory_resource &memory)
    : queue(nullptr),
      passes(&memory),
      need_tail_semaphore(false),
      tails(&memory),
      heads(&memory),
      unprocessed_head_count(0)
{
    
}

CompileBuffer::CompileBuffer(std::pmr::memory_resource &memory)
    : usages(&memory), pass_to_usage(&memory)
{
    
}

CompileImage::CompileImage(std::pmr::memory_resource &memory)
    : usages(&memory), pass_to_usage(&memory)
{
    
}

VKPT_GRAPH_END
