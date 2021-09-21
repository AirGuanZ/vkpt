#pragma once

#include <agz-utils/alloc.h>

#include <vkpt/frame_resources.h>
#include <vkpt/resource_allocator.h>

#define VKPT_RENDER_GRAPH_BEGIN VKPT_BEGIN namespace rg {
#define VKPT_RENDER_GRAPH_END   } VKPT_END

VKPT_BEGIN

class Context;

VKPT_END

VKPT_RENDER_GRAPH_BEGIN

class Compiler;
class Graph;
class Optimizer;
class PassContext;

struct BufferState
{
    vk::PipelineStageFlags2KHR stages;
    vk::AccessFlags2KHR        access;
};

struct ImageState
{
    vk::PipelineStageFlags2KHR stages;
    vk::AccessFlags2KHR        access;
    vk::ImageLayout            layout;
};

inline BufferState operator|(const BufferState &lhs, const BufferState &rhs)
{
    return BufferState{
        .stages = lhs.stages | rhs.stages,
        .access = lhs.access | rhs.access
    };
}

inline ImageState operator|(const ImageState &lhs, const ImageState &rhs)
{
    assert(lhs.layout == rhs.layout);
    return ImageState{
        .stages = lhs.stages | rhs.stages,
        .access = lhs.access | rhs.access,
        .layout = lhs.layout
    };
}

namespace state
{

    constexpr inline ImageState RenderTargetColor = {
        .stages = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput,
        .access = vk::AccessFlagBits2KHR::eColorAttachmentWrite,
        .layout = vk::ImageLayout::eColorAttachmentOptimal
    };

    constexpr inline ImageState RenderTargetDepthStencil = {
        .stages = vk::PipelineStageFlagBits2KHR::eEarlyFragmentTests |
                  vk::PipelineStageFlagBits2KHR::eLateFragmentTests,
        .access = vk::AccessFlagBits2KHR::eDepthStencilAttachmentRead |
                  vk::AccessFlagBits2KHR::eDepthStencilAttachmentWrite,
        .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
    };

    constexpr inline ImageState RenderTargetDepthStencilReadOnly = {
        .stages = vk::PipelineStageFlagBits2KHR::eEarlyFragmentTests |
                  vk::PipelineStageFlagBits2KHR::eLateFragmentTests,
        .access = vk::AccessFlagBits2KHR::eDepthStencilAttachmentRead,
        .layout = vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal
    };

} // namespace state

class Pass : public agz::misc::uncopyable_t
{
public:

    using Callback  = std::function<void(PassContext &)>;
    using Callback2 = std::function<void(CommandBuffer &)>;

    using ImageUsageKey = std::pair<Image, vk::ImageSubresourceRange>;

    struct BufferUsage
    {
        vk::PipelineStageFlags2KHR stages;
        vk::AccessFlags2KHR        access;
    };

    struct ImageUsage
    {
        vk::PipelineStageFlags2KHR stages;
        vk::AccessFlags2KHR        access;
        vk::ImageLayout            layout;
    };

    void setCallback(Callback callback);

    void setCallback(Callback2 callback);

    void use(
        const Buffer              &buffer,
        vk::PipelineStageFlags2KHR stages,
        vk::AccessFlags2KHR        access);

    void use(
        const Image                     &image,
        const vk::ImageSubresourceRange &subrsc,
        vk::PipelineStageFlags2KHR       stages,
        vk::AccessFlags2KHR              access,
        vk::ImageLayout                  layout);

    void use(
        const Image                     &image,
        vk::ImageAspectFlags             aspect,
        vk::PipelineStageFlags2KHR       stages,
        vk::AccessFlags2KHR              access,
        vk::ImageLayout                  layout);

    void use(const Image &image, const ImageState &usage);

    void wait(vk::Semaphore semaphore, vk::PipelineStageFlags2KHR stage);

    void signal(vk::Semaphore semaphore, vk::PipelineStageFlagBits2KHR stage);

    void signal(vk::Fence fence);

private:

    Pass(
        Queue                     *queue,
        int                        index,
        std::pmr::memory_resource &memory);

    static vk::ImageAspectFlags inferImageAspect(
        vk::Format format, const ImageState &usage);

    friend class agz::alloc::object_releaser_t;
    friend class Compiler;
    friend class Graph;
    friend class Optimizer;

    Queue *queue_;
    int    index_;

    Callback callback_;

    Map<Buffer, BufferUsage>       buffer_usages_;
    Map<ImageUsageKey, ImageUsage> image_usages_;

    List<vk::SemaphoreSubmitInfoKHR> signal_semaphores_;
    List<vk::SemaphoreSubmitInfoKHR> wait_semaphores_;

    List<vk::MemoryBarrier2KHR>       pre_memory_barriers_;
    List<vk::BufferMemoryBarrier2KHR> pre_buffer_barriers_;
    List<vk::ImageMemoryBarrier2KHR>  pre_image_barriers_;

    List<vk::MemoryBarrier2KHR>       post_memory_barriers_;
    List<vk::BufferMemoryBarrier2KHR> post_buffer_barriers_;
    List<vk::ImageMemoryBarrier2KHR>  post_image_barriers_;

    Set<Pass *> heads_;
    Set<Pass *> tails_;

    vk::Fence signal_fence_;
};

class Graph
{
public:

    Graph();

    Pass *addPass(Queue *queue, Pass::Callback callback = {});

    template<typename...Args>
    void addDependency(Args...passes);

    void validate();

    void optimize();

    void execute(
        SemaphoreAllocator          &semaphore_allocator,
        CommandBufferAllocator      &command_buffer_allocator,
        const std::function<void()> &after_record_callback = {});
    
    void execute(
        FrameResources              &frame_resource,
        const std::function<void()> &after_record_callback = {});

private:

    void addDependency(std::initializer_list<Pass *> passes);

    friend class Compiler;
    friend class Optimizer;

    agz::alloc::memory_resource_arena_t memory_arena_;
    agz::alloc::object_releaser_t       object_arena_;

    List<Pass *> passes_;
};

template<typename...Args>
void Graph::addDependency(Args...passes)
{
    static_assert((std::is_same_v<Args, Pass*> && ...));
    this->addDependency({ passes... });
}

VKPT_RENDER_GRAPH_END
