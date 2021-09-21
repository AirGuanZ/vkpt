#pragma once

#include <vkpt/graph/graph.h>

VKPT_RENDER_GRAPH_BEGIN

struct ExecutablePass
{
    Pass::Callback *callback = nullptr;

    Vector<vk::MemoryBarrier2KHR>       pre_memory_barriers;
    Vector<vk::BufferMemoryBarrier2KHR> pre_buffer_barriers;
    Vector<vk::ImageMemoryBarrier2KHR>  pre_image_barriers;

    Vector<vk::MemoryBarrier2KHR>       post_memory_barriers;
    Vector<vk::BufferMemoryBarrier2KHR> post_buffer_barriers;
    Vector<vk::ImageMemoryBarrier2KHR>  post_image_barriers;
};

struct ExecutableGroup
{
    Queue *queue = nullptr;

    Vector<vk::CommandBufferSubmitInfoKHR> command_buffers;
    Vector<ExecutablePass>                 passes;

    Vector<vk::SemaphoreSubmitInfoKHR> wait_semaphores;
    Vector<vk::SemaphoreSubmitInfoKHR> signal_semaphores;
    Vector<vk::Fence>                  signal_fences;
};

struct ExecutableGraph
{
    Vector<ExecutableGroup> groups;
};

class PassContext
{
public:

    PassContext(
        Queue::Type             type,
        ExecutableGroup        &group,
        CommandBufferAllocator &command_buffer_allocator);

    CommandBuffer getCommandBuffer();

    CommandBuffer newCommandBuffer();

private:

    Queue::Type             type_;
    ExecutableGroup        &exec_group_;
    CommandBufferAllocator &command_buffer_allocator_;
};

class Executor
{
public:

    void record(
        CommandBufferAllocator &command_buffer_allocator,
        ExecutableGraph        &graph);

    void submit(ExecutableGraph &graph);
};

VKPT_RENDER_GRAPH_END
