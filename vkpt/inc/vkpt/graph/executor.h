#pragma once

#include <vkpt/graph/graph.h>

VKPT_GRAPH_BEGIN

struct ExecutablePass
{
    explicit ExecutablePass(std::pmr::memory_resource &memory);

    PassBase *pass;

    Vector<vk::MemoryBarrier2KHR>       pre_memory_barriers;
    Vector<vk::BufferMemoryBarrier2KHR> pre_buffer_barriers;
    Vector<vk::ImageMemoryBarrier2KHR>  pre_image_barriers;

    Vector<vk::MemoryBarrier2KHR>       post_memory_barriers;
    Vector<vk::BufferMemoryBarrier2KHR> post_buffer_barriers;
    Vector<vk::ImageMemoryBarrier2KHR>  post_image_barriers;
};

struct ExecutableGroup
{
    explicit ExecutableGroup(std::pmr::memory_resource &memory);

    const Queue           *queue;
    Vector<ExecutablePass> passes;

    Vector<vk::SemaphoreSubmitInfoKHR> wait_semaphores;
    Vector<vk::SemaphoreSubmitInfoKHR> signal_semaphores;
    Vector<vk::Fence>                  signal_fences;
};

struct ExecutableGraph
{
    explicit ExecutableGraph(std::pmr::memory_resource &memory);

    Vector<ExecutableGroup> groups;

    Map<Buffer, ResourceState>           buffer_final_states;
    Map<ImageSubresource, ResourceState> image_final_states;
};

class Executor
{
public:

    Executor();

    void record(
        CommandBufferAllocator &command_buffer_allocator,
        const ExecutableGraph  &graph);

    void submit();

private:

    struct GroupResult
    {
        const ExecutableGroup                 *group;
        Vector<vk::CommandBufferSubmitInfoKHR> command_buffers;
    };

    agz::alloc::memory_resource_arena_t memory_;
    Vector<GroupResult>                 group_results_;
};

VKPT_GRAPH_END
