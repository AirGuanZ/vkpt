#include <vkpt/graph/executor.h>

VKPT_GRAPH_BEGIN

ExecutablePass::ExecutablePass(std::pmr::memory_resource &memory)
    : pre_memory_barriers(&memory),
      pre_buffer_barriers(&memory),
      pre_image_barriers(&memory),
      post_memory_barriers(&memory),
      post_buffer_barriers(&memory),
      post_image_barriers(&memory)
{
    
}

ExecutableGroup::ExecutableGroup(std::pmr::memory_resource &memory)
    : passes(&memory),
      wait_semaphores(&memory),
      signal_semaphores(&memory),
      signal_fences(&memory)
{
    
}

ExecutableGraph::ExecutableGraph(std::pmr::memory_resource &memory)
    : groups(&memory),
      buffer_final_states(&memory),
      image_final_states(&memory)
{
    
}

Executor::Executor()
    : group_results_(&memory_)
{
    
}

void Executor::record(
    CommandBufferAllocator &command_buffer_allocator,
    const ExecutableGraph  &graph)
{
    assert(group_results_.empty());
    group_results_.resize(graph.groups.size());

    for(size_t i = 0; i < graph.groups.size(); ++i)
    {
        auto &group = graph.groups[i];
        auto &result = group_results_[i];

        result.group = &group;
        result.command_buffers =
            Vector<vk::CommandBufferSubmitInfoKHR>(&memory_);

        PassContext context(
            group.queue->getType(),
            command_buffer_allocator,
            result.command_buffers);

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
    }

    for(auto &[buffer, state] : graph.buffer_final_states)
    {
        auto b = buffer;
        b.getState() = state;
    }

    for(auto &[image, state] : graph.image_final_states)
    {
        auto i = image;
        i.getState() = state;
    }
}

void Executor::submit()
{
    for(auto &result : group_results_)
    {
        auto &group = *result.group;

        result.command_buffers.back().commandBuffer.end();

        group.queue->submit(
            group.wait_semaphores,
            group.signal_semaphores,
            result.command_buffers,
            group.signal_fences.empty() ?
                nullptr : group.signal_fences.front());

        for(size_t i = 1; i < group.signal_fences.size(); ++i)
        {
            group.queue->submit(
                {}, {}, {}, group.signal_fences[i]);
        }
    }
}

VKPT_GRAPH_END
