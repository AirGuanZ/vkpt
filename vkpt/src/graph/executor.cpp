#include <vkpt/graph/executor.h>

VKPT_RENDER_GRAPH_BEGIN

PassContext::PassContext(
    Queue::Type             type,
    ExecutableGroup        &group,
    CommandBufferAllocator &command_buffer_allocator)
    : type_(type),
      exec_group_(group),
      command_buffer_allocator_(command_buffer_allocator)
{
    
}

CommandBuffer PassContext::getCommandBuffer()
{
    assert(!exec_group_.command_buffers.empty());
    return CommandBuffer(exec_group_.command_buffers.back().commandBuffer);
}

CommandBuffer PassContext::newCommandBuffer()
{
    auto raw = command_buffer_allocator_.newCommandBuffer(type_).getRaw();
    exec_group_.command_buffers.push_back(vk::CommandBufferSubmitInfoKHR{
        .commandBuffer = raw
    });
    return getCommandBuffer();
}

void Executor::record(
    CommandBufferAllocator &command_buffer_allocator,
    ExecutableGraph        &graph)
{
    for(auto &group : graph.groups)
    {
        assert(group.command_buffers.empty());
        PassContext context(
            group.queue->getType(), group, command_buffer_allocator);
        context.newCommandBuffer();
    
        context.getCommandBuffer().begin(true);

        for(auto &pass : group.passes)
        {
            if(!pass.pre_memory_barriers.empty() ||
               !pass.pre_buffer_barriers.empty() ||
               !pass.pre_image_barriers.empty())
            {
                context.getCommandBuffer().pipelineBarrier(
                    pass.pre_memory_barriers,
                    pass.pre_buffer_barriers,
                    pass.pre_image_barriers);
            }
    
            if(pass.callback)
                (*pass.callback)(context);

            if(!pass.post_memory_barriers.empty() ||
               !pass.post_buffer_barriers.empty() ||
               !pass.post_image_barriers.empty())
            {
                context.getCommandBuffer().pipelineBarrier(
                    pass.post_memory_barriers,
                    pass.post_buffer_barriers,
                    pass.post_image_barriers);
            }
        }
    
        context.getCommandBuffer().end();
    }
}

void Executor::submit(ExecutableGraph &graph)
{
    for(auto &group : graph.groups)
    {
        group.queue->submit(
            group.wait_semaphores,
            group.signal_semaphores,
            group.command_buffers,
            group.signal_fences.empty() ? nullptr : group.signal_fences[0]);
        for(size_t i = 1; i < group.signal_fences.size(); ++i)
            group.queue->submit({}, {}, {}, group.signal_fences[i]);
        group.command_buffers.clear();
    }
}

VKPT_RENDER_GRAPH_END
