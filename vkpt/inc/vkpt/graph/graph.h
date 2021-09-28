#pragma once

#include <agz-utils/alloc.h>

#include <vkpt/allocator/command_buffer_allocator.h>
#include <vkpt/allocator/semaphore_allocator.h>
#include <vkpt/graph/usage.h>
#include <vkpt/object/semaphore.h>
#include <vkpt/resource/buffer.h>
#include <vkpt/resource/image.h>

VKPT_GRAPH_BEGIN

class PassContext
{
public:

    void newCommandBuffer();

    CommandBuffer getCommandBuffer();

private:

    friend class Executor;

    PassContext(
        Queue::Type                             queue_type,
        CommandBufferAllocator                 &command_buffer_allocator,
        Vector<vk::CommandBufferSubmitInfoKHR> &command_buffers);

    Queue::Type                             queue_type_;
    CommandBufferAllocator                 &command_buffer_allocator_;
    Vector<vk::CommandBufferSubmitInfoKHR> &command_buffers_;
};

class Pass
{
public:

    struct BufferUsage
    {
        vk::PipelineStageFlags2KHR stages;
        vk::AccessFlags2KHR        access;

        vk::PipelineStageFlags2KHR end_stages;
        vk::AccessFlags2KHR        end_access;
    };

    struct ImageUsage
    {
        vk::PipelineStageFlags2KHR stages;
        vk::AccessFlags2KHR        access;
        vk::ImageLayout            layout;

        vk::PipelineStageFlags2KHR end_stages;
        vk::AccessFlags2KHR        end_access;
        vk::ImageLayout            end_layout;
    };

    using Callback = std::function<void(PassContext &)>;

    void use(
        const Image                &image,
        const vk::ImageSubresource &subrsc,
        const ResourceUsage        &usage,
        const ResourceUsage        &exit_usage = USAGE_NIL);
    
    void use(
        const Image         &image,
        const ResourceUsage &usage,
        const ResourceUsage &exit_usage = USAGE_NIL);
    
    void use(
        const Image         &image,
        vk::ImageAspectFlags aspect,
        const ResourceUsage &usage,
        const ResourceUsage &exit_usage = USAGE_NIL);
    
    void use(
        const Image                     &image,
        const vk::ImageSubresourceRange &range,
        const ResourceUsage             &usage,
        const ResourceUsage             &exit_usage = USAGE_NIL);

    void use(
        const Buffer        &buffer,
        const ResourceUsage &usage,
        const ResourceUsage &exit_usage = USAGE_NIL);

    void signal(vk::Fence fence);

    void setCallback(Callback callback);

    void setName(std::string name);

    const std::string &getName() const;

    auto &_bufferUsages() const { return buffer_usages_; }
    auto &_imageUsages()  const { return image_usages_; }

private:

    friend class agz::alloc::object_releaser_t;
    friend class Compiler;
    friend class Graph;
    friend class GroupBarrierGenerator;

    explicit Pass(std::pmr::memory_resource &memory);

    const Queue *queue_;
    int          index_;
    std::string  name_;

    Callback callback_;

    Map<Buffer, BufferUsage>          buffer_usages_;
    Map<ImageSubresource, ImageUsage> image_usages_;

    List<vk::Fence> signal_fences_;

    Set<Pass *> tails_;
    Set<Pass *> heads_;
};

class Graph
{
public:

    Graph();

    Pass *addPass(const Queue *queue);

    void waitBeforeFirstUsage(
        const Buffer &buffer,
        Semaphore     semaphore);

    void signalAfterLastUsage(
        const Buffer &buffer,
        Semaphore     semaphore,
        const Queue  *next_queue,
        bool          release_only);

    void waitBeforeFirstUsage(
        const ImageSubresource &image_subrsc,
        Semaphore               wait_semaphore);

    void signalAfterLastUsage(
        const ImageSubresource &image_subrsc,
        Semaphore               semaphore,
        const Queue            *next_queue,
        vk::ImageLayout         next_layout,
        bool                    release_only);

    template<typename...Args>
    void addDependency(Args...passes);

    void execute(
        SemaphoreAllocator          &semaphore_allocator,
        CommandBufferAllocator      &command_buffer_allocator,
        const std::function<void()> &after_record_callback = {});

private:

    friend class Compiler;
    friend class SemaphoreSignalHandler;
    friend class SemaphoreWaitHandler;

    struct Signal
    {
        mutable Semaphore semaphore;
        const Queue      *queue;
        vk::ImageLayout   layout;
        bool              release_only;
    };

    void addDependency(std::initializer_list<Pass *> passes);

    agz::alloc::memory_resource_arena_t memory_;
    agz::alloc::object_releaser_t       arena_;

    List<Pass *> passes_;

    Map<Buffer, Semaphore>           buffer_waits_;
    Map<ImageSubresource, Semaphore> image_waits_;

    Map<Buffer, Signal>           buffer_signals_;
    Map<ImageSubresource, Signal> image_signals_;
};

template<typename...Args>
void Graph::addDependency(Args...passes)
{
    static_assert((std::is_same_v<Args, Pass*> && ...));
    this->addDependency({ passes... });
}

VKPT_GRAPH_END
