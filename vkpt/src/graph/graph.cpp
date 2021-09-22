#include <vkpt/graph/compiler.h>
#include <vkpt/graph/finalizer.h>
#include <vkpt/context.h>

VKPT_RENDER_GRAPH_BEGIN

void Pass::setCallback(Callback callback)
{
    callback_ = std::move(callback);
}

void Pass::setCallback(Callback2 callback)
{
    callback_ = [c = std::move(callback)](PassContext &ctx)
    {
        auto command_buffer = ctx.getCommandBuffer();
        c(command_buffer);
    };
}

void Pass::use(
    const Buffer              &buffer,
    vk::PipelineStageFlags2KHR stages,
    vk::AccessFlags2KHR        access)
{
    buffer_usages_.insert({ buffer, { stages, access } });
}

void Pass::use(
    const Image                     &image,
    const vk::ImageSubresourceRange &subrsc,
    vk::PipelineStageFlags2KHR       stages,
    vk::AccessFlags2KHR              access,
    vk::ImageLayout                  layout)
{
    image_usages_.insert({ { image, subrsc }, { stages, access, layout } });
}

void Pass::use(
    const Image               &image,
    vk::ImageAspectFlags       aspect,
    vk::PipelineStageFlags2KHR stages,
    vk::AccessFlags2KHR        access,
    vk::ImageLayout            layout)
{
    const vk::ImageSubresourceRange range = {
        .aspectMask     = aspect,
        .baseMipLevel   = 0,
        .levelCount     = image.getDescription().mip_levels,
        .baseArrayLayer = 0,
        .layerCount     = image.getDescription().array_layers
    };
    use(image, range, stages, access, layout);
}

void Pass::use(const Image &image, const ImageState &usage)
{
    const auto aspect = inferImageAspect(image.getDescription().format, usage);
    use(image, aspect, usage.stages, usage.access, usage.layout);
}

void Pass::wait(vk::Semaphore semaphore, vk::PipelineStageFlags2KHR stage)
{
    wait_semaphores_.push_back(vk::SemaphoreSubmitInfoKHR{
        .semaphore = semaphore,
        .stageMask = stage
    });
}

void Pass::signal(vk::Semaphore semaphore, vk::PipelineStageFlagBits2KHR stage)
{
    signal_semaphores_.push_back(vk::SemaphoreSubmitInfoKHR{
        .semaphore = semaphore,
        .stageMask = stage
    });
}

void Pass::signal(vk::Fence fence)
{
    signal_fence_ = fence;
}

Pass::Pass(
    Queue                     *queue,
    int                        index,
    std::pmr::memory_resource &memory)
    : queue_(queue), index_(index),
      buffer_usages_(&memory),
      image_usages_(&memory),
      signal_semaphores_(&memory),
      wait_semaphores_(&memory),
      pre_memory_barriers_(&memory),
      pre_buffer_barriers_(&memory),
      pre_image_barriers_(&memory),
      post_memory_barriers_(&memory),
      post_buffer_barriers_(&memory),
      post_image_barriers_(&memory),
      heads_(&memory),
      tails_(&memory)
{

}

vk::ImageAspectFlags Pass::inferImageAspect(
    vk::Format format, const ImageState &usage)
{
    const bool has_depth_aspect =
        format == vk::Format::eD16Unorm ||
        format == vk::Format::eX8D24UnormPack32 ||
        format == vk::Format::eD32Sfloat ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD32SfloatS8Uint;

    const bool has_stencil_aspect =
        format == vk::Format::eS8Uint ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD32SfloatS8Uint;

    if(!has_depth_aspect && !has_stencil_aspect)
        return vk::ImageAspectFlagBits::eColor;

    if(has_depth_aspect && has_stencil_aspect)
    {
        return vk::ImageAspectFlagBits::eDepth |
               vk::ImageAspectFlagBits::eStencil;
    }

    if(has_depth_aspect)
        return vk::ImageAspectFlagBits::eDepth;

    assert(has_stencil_aspect);
    return vk::ImageAspectFlagBits::eStencil;
}

Graph::Graph()
    : object_arena_(memory_arena_), passes_(&memory_arena_)
{
    
}

Pass *Graph::addPass(Queue *queue, Pass::Callback callback)
{
    const int index = static_cast<int>(passes_.size());
    auto pass = object_arena_.create<Pass>(queue, index, memory_arena_);
    if(callback)
        pass->setCallback(std::move(callback));
    passes_.push_back(pass);
    return pass;
}

void Graph::finalize()
{
    Finalizer finalizer;
    finalizer.finalize(*this);
}

void Graph::execute(
    SemaphoreAllocator          &semaphore_allocator,
    CommandBufferAllocator      &command_buffer_allocator,
    const std::function<void()> &after_record_callback)
{
    agz::alloc::memory_resource_arena_t memory;

    Compiler compiler;
    auto exec_graph = compiler.compile(memory, semaphore_allocator, *this);

    Executor executor;
    executor.record(command_buffer_allocator, exec_graph);
    if(after_record_callback)
        after_record_callback();

    executor.submit(exec_graph);
}

void Graph::execute(
    FrameResources              &frame_resource,
    const std::function<void()> &after_record_callback)
{
    execute(frame_resource, frame_resource, after_record_callback);
}

void Graph::addDependency(std::initializer_list<Pass*> passes)
{
    auto p = passes.begin();
    for(size_t i = 1; i < passes.size(); ++i)
    {
        p[i - 1]->tails_.insert(p[i]);
        p[i]->heads_.insert(p[i - 1]);
    }
}

VKPT_RENDER_GRAPH_END
