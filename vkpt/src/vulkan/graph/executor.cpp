#include <vkpt/vulkan/graph/executor.h>

VKPT_BEGIN

using namespace graph;

void RenderGraphExecutor::execute(
    RenderGraph::ThreadPool &threads,
    uint32_t                 thread_count,
    CommandBufferAllocator  &command_buffer_allocator,
    ExecutableRenderGraph   &graph)
{
    std::atomic<uint32_t> next_section_index = 0;

    std::vector<CommandBuffer> command_buffers;
    command_buffers.resize(graph.sections.size());

    for(size_t i = 0; i < graph.sections.size(); ++i)
    {
        auto &dst = command_buffers[i];
        switch(graph.sections[i].queue.getType())
        {
        case Queue::Type::Graphics:
            dst = command_buffer_allocator.newGraphicsCommandBuffer();
            break;
        case Queue::Type::Compute:
            dst = command_buffer_allocator.newComputeCommandBuffer();
            break;
        case Queue::Type::Transfer:
            dst = command_buffer_allocator.newTransferCommandBuffer();
            break;
        default:
            agz::misc::unreachable();
        }
    }

    auto thread_func = [&](int)
    {
        const uint32_t section_index = next_section_index++;
        if(section_index >= graph.sections.size())
            return;

        auto &section        = graph.sections[section_index];
        auto &command_buffer = command_buffers[section_index];

        executeSection(section, command_buffer);
    };

    threads.run(thread_count, std::move(thread_func));

    for(size_t i = 0; i < graph.sections.size(); ++i)
    {
        auto &section        = graph.sections[i];
        auto &command_buffer = command_buffers[i];

        section.queue.submit(
            section.wait_semaphores,
            section.wait_stages,
            section.signal_semaphores,
            { command_buffer },
            section.signal_fence);
    }
}

void RenderGraphExecutor::executeSection(
    ExecutableSection &section,
    CommandBuffer     &command_buffer)
{
    command_buffer.begin(true);

    for(auto &pass : section.passes)
    {
        if(pass.pre_barrier)
        {
            command_buffer.pipelineBarrier(
                pass.pre_barrier->src,
                pass.pre_barrier->dst,
                {},
                pass.pre_barrier->buffers,
                pass.pre_barrier->images);
        }

        assert(pass.callback);
        (*pass.callback)(command_buffer);

        if(pass.post_barrier)
        {
            command_buffer.pipelineBarrier(
                pass.post_barrier->src,
                pass.post_barrier->dst,
                {},
                pass.post_barrier->buffers,
                pass.post_barrier->images);
        }
    }

    command_buffer.end();
}

VKPT_END
