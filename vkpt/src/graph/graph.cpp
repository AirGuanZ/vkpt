#include <vkpt/graph/compiler.h>

VKPT_GRAPH_BEGIN

void PassContext::newCommandBuffer()
{
    assert(!command_buffers_.empty());
    command_buffers_.back().commandBuffer.end();

    auto new_command_buffer =
        command_buffer_allocator_.newCommandBuffer(queue_type_);
    new_command_buffer.begin(true);
    command_buffers_.push_back(vk::CommandBufferSubmitInfoKHR{
        .commandBuffer = new_command_buffer.getRaw()
    });
}

CommandBuffer PassContext::getCommandBuffer()
{
    assert(!command_buffers_.empty());
    return CommandBuffer(command_buffers_.back().commandBuffer);
}

PassContext::PassContext(
    Queue::Type                             queue_type,
    CommandBufferAllocator                 &command_buffer_allocator,
    Vector<vk::CommandBufferSubmitInfoKHR> &command_buffers)
    : queue_type_(queue_type),
      command_buffer_allocator_(command_buffer_allocator),
      command_buffers_(command_buffers)
{
    auto new_command_buffer =
        command_buffer_allocator_.newCommandBuffer(queue_type_);
    new_command_buffer.begin(true);
    command_buffers_.push_back(vk::CommandBufferSubmitInfoKHR{
        .commandBuffer = new_command_buffer.getRaw()
    });
}

void Pass::use(
    const Image                &image,
    const vk::ImageSubresource &subrsc,
    const ResourceUsage        &usage,
    const ResourceUsage        &exit_usage)
{
    if(exit_usage == USAGE_NIL)
        use(image, subrsc, usage, usage);
    else
    {
        image_usages_.insert(
            {
                { image, subrsc },
                {
                    usage.stages, usage.access, usage.layout,
                    exit_usage.stages, exit_usage.access, exit_usage.layout
                }
            });
    }
}

void Pass::use(
    const Image         &image,
    const ResourceUsage &usage,
    const ResourceUsage &exit_usage)
{
    auto &desc = image.getDescription();

    const bool has_depth   = hasDepthAspect(desc.format);
    const bool has_stencil = hasStencilAspect(desc.format);
    const bool has_color   = !has_depth && !has_stencil;

    if(has_depth)
        use(image, vk::ImageAspectFlagBits::eDepth, usage, exit_usage);
    
    if(has_stencil)
        use(image, vk::ImageAspectFlagBits::eStencil, usage, exit_usage);

    if(has_color)
        use(image, vk::ImageAspectFlagBits::eColor, usage, exit_usage);
}

void Pass::use(
    const Image         &image,
    vk::ImageAspectFlags aspect,
    const ResourceUsage &usage,
    const ResourceUsage &exit_usage)
{
    auto &desc = image.getDescription();
    use(image, vk::ImageSubresourceRange{
        .aspectMask     = aspect,
        .baseMipLevel   = 0,
        .levelCount     = desc.mip_levels,
        .baseArrayLayer = 0,
        .layerCount     = desc.array_layers
    }, usage, exit_usage);
}

void Pass::use(
    const Image                     &image,
    const vk::ImageSubresourceRange &range,
    const ResourceUsage             &usage,
    const ResourceUsage             &exit_usage)
{
    foreachSubrsc(range, [&](const vk::ImageSubresource &subrsc)
    {
        use(image, subrsc, usage, exit_usage);
    });
}

void Pass::use(
    const Buffer        &buffer,
    const ResourceUsage &usage,
    const ResourceUsage &exit_usage)
{
    if(exit_usage == USAGE_NIL)
        use(buffer, usage, usage);
    else
    {
        buffer_usages_.insert(
            {
                buffer,
                {
                    usage.stages, usage.access,
                    exit_usage.stages, exit_usage.access
                }
            });
    }
}

void Pass::signal(vk::Fence fence)
{
    signal_fences_.push_back(fence);
}

void Pass::setCallback(Callback callback)
{
    callback_.swap(callback);
}

void Pass::setName(std::string name)
{
    name_.swap(name);
}

const std::string &Pass::getName() const
{
    return name_;
}

Pass::Pass(std::pmr::memory_resource &memory)
    : queue_(nullptr),
      index_(-1),
      buffer_usages_(&memory),
      image_usages_(&memory),
      signal_fences_(&memory),
      tails_(&memory),
      heads_(&memory)
{
    
}

Graph::Graph()
    : arena_(memory_),
      passes_(&memory_),
      buffer_waits_(&memory_),
      image_waits_(&memory_),
      buffer_signals_(&memory_),
      image_signals_(&memory_)
{
    
}

Pass *Graph::addPass(const Queue *queue)
{
    Pass *pass = arena_.create<Pass>(memory_);
    pass->queue_ = queue;
    pass->index_ = static_cast<int>(passes_.size());
    passes_.push_back(pass);
    return pass;
}

void Graph::waitBeforeFirstUsage(
    const Buffer &buffer,
    Semaphore     semaphore)
{
    assert(!buffer_waits_.contains(buffer));
    buffer_waits_.insert({ buffer, semaphore });
}

void Graph::signalAfterLastUsage(
    const Buffer &buffer,
    Semaphore     semaphore,
    const Queue  *next_queue,
    bool          release_only)
{
    assert(!buffer_signals_.contains(buffer));
    buffer_signals_.insert({
        buffer,
        { semaphore, next_queue, vk::ImageLayout::eUndefined, release_only }
    });
}

void Graph::waitBeforeFirstUsage(
    const Image                &image,
    const vk::ImageSubresource &subrsc,
    Semaphore                   semaphore)
{
    ImageSubresource image_subrsc = { image, subrsc };
    assert(!image_waits_.contains(image_subrsc));
    image_waits_.insert({ image_subrsc, semaphore });
}

void Graph::waitBeforeFirstUsage(const Image &image, Semaphore semaphore)
{
    auto &desc = image.getDescription();
    
    const bool has_depth   = hasDepthAspect(desc.format);
    const bool has_stencil = hasStencilAspect(desc.format);
    const bool has_color   = !has_depth && !has_stencil;

    if(has_depth)
    {
        waitBeforeFirstUsage(
            image, vk::ImageAspectFlagBits::eDepth, semaphore);
    }

    if(has_stencil)
    {
        waitBeforeFirstUsage(
            image, vk::ImageAspectFlagBits::eStencil, semaphore);
    }

    if(has_color)
    {
        waitBeforeFirstUsage(
            image, vk::ImageAspectFlagBits::eColor, semaphore);
    }
}

void Graph::waitBeforeFirstUsage(
    const Image &image, vk::ImageAspectFlags aspect, Semaphore semaphore)
{
    auto &desc = image.getDescription();
    waitBeforeFirstUsage(
        image, vk::ImageSubresourceRange{
            .aspectMask     = aspect,
            .baseMipLevel   = 0,
            .levelCount     = desc.mip_levels,
            .baseArrayLayer = 0,
            .layerCount     = desc.array_layers
        }, semaphore);
}

void Graph::waitBeforeFirstUsage(
    const Image                     &image,
    const vk::ImageSubresourceRange &range,
    Semaphore                        semaphore)
{
    foreachSubrsc(range, [&](const vk::ImageSubresource &subrsc)
    {
        waitBeforeFirstUsage(image, subrsc, semaphore);
    });
}

void Graph::signalAfterLastUsage(
    const Image                &image,
    const vk::ImageSubresource &subrsc,
    Semaphore                   semaphore,
    const Queue                *next_queue,
    vk::ImageLayout             next_layout,
    bool                        release_only)
{
    ImageSubresource image_subrsc = { image, subrsc };
    assert(!image_signals_.contains(image_subrsc));
    image_signals_.insert({
        image_subrsc,
        { semaphore, next_queue, next_layout, release_only }
    });
}

void Graph::signalAfterLastUsage(
    const Image    &image,
    Semaphore       semaphore,
    const Queue    *next_queue,
    vk::ImageLayout next_layout,
    bool            release_only)
{
    auto &desc = image.getDescription();
    
    const bool has_depth   = hasDepthAspect(desc.format);
    const bool has_stencil = hasStencilAspect(desc.format);
    const bool has_color   = !has_depth && !has_stencil;

    if(has_depth)
    {
        signalAfterLastUsage(
            image, vk::ImageAspectFlagBits::eDepth, semaphore,
            next_queue, next_layout, release_only);
    }

    if(has_stencil)
    {
        signalAfterLastUsage(
            image, vk::ImageAspectFlagBits::eStencil, semaphore,
            next_queue, next_layout, release_only);
    }

    if(has_color)
    {
        signalAfterLastUsage(
            image, vk::ImageAspectFlagBits::eColor, semaphore,
            next_queue, next_layout, release_only);
    }
}

void Graph::signalAfterLastUsage(
    const Image         &image,
    vk::ImageAspectFlags aspect,
    Semaphore            semaphore,
    const Queue         *next_queue,
    vk::ImageLayout      next_layout,
    bool                 release_only)
{
    auto &desc = image.getDescription();
    const vk::ImageSubresourceRange range = {
        .aspectMask     = aspect,
        .baseMipLevel   = 0,
        .levelCount     = desc.mip_levels,
        .baseArrayLayer = 0,
        .layerCount     = desc.array_layers
    };
    signalAfterLastUsage(
        image, range, semaphore, next_queue, next_layout, release_only);
}

void Graph::signalAfterLastUsage(
    const Image                     &image,
    const vk::ImageSubresourceRange &range,
    Semaphore                        semaphore,
    const Queue                     *next_queue,
    vk::ImageLayout                  next_layout,
    bool                             release_only)
{
    foreachSubrsc(range, [&](const vk::ImageSubresource &subrsc)
    {
        signalAfterLastUsage(
            image, subrsc, semaphore, next_queue, next_layout, release_only);
    });
}

void Graph::execute(
    SemaphoreAllocator          &semaphore_allocator,
    CommandBufferAllocator      &command_buffer_allocator,
    const std::function<void()> &after_record_callback)
{
    Compiler compiler;
    auto exec = compiler.compile(semaphore_allocator, *this);

    Executor executor;
    executor.record(command_buffer_allocator, exec);

    if(after_record_callback)
        after_record_callback();

    executor.submit();
}

void Graph::addDependency(std::initializer_list<Pass*> passes)
{
    auto ptr = passes.begin();
    for(size_t i = 1; i < passes.size(); ++i)
    {
        auto head = ptr[i - 1];
        auto tail = ptr[i];
        head->tails_.insert(tail);
        tail->heads_.insert(head);
    }
}

VKPT_GRAPH_END
