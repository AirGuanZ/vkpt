#pragma once

#include <agz-utils/alloc.h>

#include <vkpt/vulkan/graph/graph.h>
#include <vkpt/vulkan/command_buffer_allocator.h>
#include <vkpt/vulkan/queue.h>

VKPT_BEGIN

namespace graph
{

    struct ExecutablePass
    {
        struct ExecutionBarrier
        {
            vk::PipelineStageFlags src;
            vk::PipelineStageFlags dst;

            std::vector<vk::BufferMemoryBarrier> buffers;
            std::vector<vk::ImageMemoryBarrier>  images;
        };

        std::optional<ExecutionBarrier> pre_barrier;
        std::optional<ExecutionBarrier> post_barrier;

        Pass::Callback *callback = nullptr;
    };

    struct ExecutableSection
    {
        std::vector<vk::Semaphore>          wait_semaphores;
        std::vector<vk::PipelineStageFlags> wait_stages;
        std::vector<vk::Semaphore>          signal_semaphores;

        vk::Fence signal_fence;
        
        std::vector<ExecutablePass> passes;

        Queue queue;
    };

    struct ExecutableRenderGraph
    {
        std::vector<ExecutableSection> sections;
    };

    class RenderGraphExecutor
    {
    public:
        
        void execute(
            RenderGraph::ThreadPool &threads,
            uint32_t                 thread_count,
            CommandBufferAllocator  &command_buffer_allocator,
            ExecutableRenderGraph   &graph);

    private:

        static void executeSection(
            ExecutableSection &section,
            CommandBuffer     &command_buffer);
    };

} // namespace graph

VKPT_END
