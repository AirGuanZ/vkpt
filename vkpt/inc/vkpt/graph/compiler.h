#pragma once

#include <bitset>
#include <span>

#include <vkpt/graph/executor.h>
#include <vkpt/graph/resource_tracer.h>

VKPT_RENDER_GRAPH_BEGIN

class Compiler
{
public:

    Compiler();

    ExecutableGraph compile(
        std::pmr::memory_resource &memory,
        SemaphoreAllocator        &semaphore_allocator,
        Graph                     &graph);
    
    ExecutableGraph compile(
        SemaphoreAllocator        &semaphore_allocator,
        Graph                     &graph);

private:

    struct CompileGroup;

    struct CompilePass
    {
        explicit CompilePass(std::pmr::memory_resource &memory);

        Pass *raw_pass = nullptr;

        std::bitset<64> group_flags;
        CompileGroup   *group = nullptr;

        int unprocessed_head_count = 0;

        Set<vk::MemoryBarrier2KHR>       pre_memory_barriers;
        Set<vk::BufferMemoryBarrier2KHR> pre_buffer_barriers;
        Set<vk::ImageMemoryBarrier2KHR>  pre_image_barriers;
        Set<vk::MemoryBarrier2KHR>       post_memory_barriers;
        Set<vk::BufferMemoryBarrier2KHR> post_buffer_barriers;
        Set<vk::ImageMemoryBarrier2KHR>  post_image_barriers;
    };

    struct CompileGroup
    {
        explicit CompileGroup(std::pmr::memory_resource &memory);

        Vector<CompilePass *> passes;
        
        struct GroupDependency
        {
            vk::Semaphore              semaphore;
            //vk::PipelineStageFlags2KHR signal_stages = {};
            vk::PipelineStageFlags2KHR wait_stages   = {};
        };

        Map<CompileGroup *, GroupDependency *> heads;
        Map<CompileGroup *, GroupDependency *> tails;

        int unprocessed_head_count = 0;
    };

    using ImageStateKey = std::pair<vk::Image, vk::ImageSubresource>;

    template<bool Reverse>
    void newGroupFlag(CompilePass *compile_entry, size_t bit_index);

    void generateGroupFlags();

    void generateGroups();

    void fillGroupArcs();

    void sortGroups();

    void sortPasses();

    void fillInterGroupSemaphores(SemaphoreAllocator &semaphore_allocator);

    void fillResourceStateTracers();

    CompileGroup::GroupDependency *findHeadDependency(
        CompileGroup *src, CompileGroup *dst) const;

    void processBufferDependency(
        Pass                    *pass,
        const Buffer            &buffer,
        const Pass::BufferUsage &usage);

    void processImageDependency(
        Pass                       *pass,
        const Image                &image,
        const vk::ImageSubresource &subrsc,
        const Pass::ImageUsage     &usage);

    void generateBarriers();

    void fillExecutableGroup(
        std::pmr::memory_resource &memory,
        ExecutableGroup           &group,
        CompileGroup              &compile);

    void fillExecutablePass(
        std::pmr::memory_resource &memory,
        ExecutablePass            &pass,
        CompilePass               &compile);

    agz::alloc::memory_resource_arena_t memory_arena_;
    agz::alloc::object_releaser_t       object_arena_;

    Vector<CompilePass *>  compile_passes_;
    Vector<CompileGroup *> compile_groups_;

    Map<vk::Buffer, BufferStateTracer>   buffer_states_;
    Map<ImageStateKey, ImageStateTracer> image_states_;
};

VKPT_RENDER_GRAPH_END
