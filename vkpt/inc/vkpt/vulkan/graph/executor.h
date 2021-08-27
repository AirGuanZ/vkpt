#pragma once

#include <agz-utils/alloc.h>

#include <vkpt/vulkan/graph/graph.h>
#include <vkpt/vulkan/queue.h>

VKPT_BEGIN

struct ExecutablePass
{
    struct ExecutionBarrier
    {
        vk::PipelineStageFlags src;
        vk::PipelineStageFlags dst;
    };

    std::vector<ExecutionBarrier>        execution_barriers;
    std::vector<vk::BufferMemoryBarrier> buffer_barriers;
    std::vector<vk::ImageMemoryBarrier>  image_barriers;

    uint32_t section_index = 0;

    RenderGraph::PassCallback *callback = nullptr;
};

struct ExecutableSection
{
    std::vector<vk::Semaphore>          wait_external_semaphores;
    std::vector<vk::PipelineStageFlags> wait_external_stages;
    std::vector<vk::Semaphore>          signal_external_semaphores;

    std::vector<uint32_t>               wait_internal_semaphores;
    std::vector<vk::PipelineStageFlags> wait_internal_stages;
    std::vector<uint32_t>               signal_internal_semaphores;

    uint32_t command_buffer_count = 1;

    Queue queue;
};

struct ExecutableRenderGraph
{
    uint32_t thread_count = 1;

    std::vector<ExecutablePass>    passes;
    std::vector<ExecutableSection> sections;
};

VKPT_END
