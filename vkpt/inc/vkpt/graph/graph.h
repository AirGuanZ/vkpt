#pragma once

#include <agz-utils/alloc.h>

#include <vkpt/allocator/command_buffer_allocator.h>
#include <vkpt/allocator/semaphore_allocator.h>
#include <vkpt/graph/usage.h>
#include <vkpt/object/semaphore.h>
#include <vkpt/resource/buffer.h>
#include <vkpt/resource/image.h>

VKPT_GRAPH_BEGIN

class Graph;
class Compiler;

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

class PassBase
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

    PassBase() = default;

    explicit PassBase(std::pmr::memory_resource &memory);

    virtual ~PassBase() = default;

    virtual std::string getPassName() const = 0;

    virtual const Queue *getPassQueue() const = 0;

    virtual void onPassRender(PassContext &context) = 0;

    const Set<PassBase *> &_getTails() const { return tails_; }
    const Set<PassBase *> &_getHeads() const { return heads_; }

    const Map<Buffer, BufferUsage>            &_getBufferUsages() const { return buffer_usages_; }
    const Map<ImageSubresource, ImageUsage>   &_getImageUsages()  const { return image_usages_; }

    const List<vk::Fence> &_getFences() const { return fences_; }

protected:

    void addBufferUsage(const Buffer &buffer, const BufferUsage &usage);

    void addImageUsage(const ImageSubresource &image, const ImageUsage &usage);

    void addFence(vk::Fence fence);

private:

    friend class Graph;
    friend class Compiler;

    int index_ = -1;

    Set<PassBase *> tails_;
    Set<PassBase *> heads_;

    Map<Buffer, BufferUsage>          buffer_usages_;
    Map<ImageSubresource, ImageUsage> image_usages_;

    List<vk::Fence> fences_;
};

class Pass : public PassBase
{
public:

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

    void setQueue(const Queue *queue);

    std::string getPassName() const override;

    const Queue *getPassQueue() const override;

    void onPassRender(PassContext &context) override;

private:

    friend class agz::alloc::object_releaser_t;

    explicit Pass(std::pmr::memory_resource &memory);

    const Queue *queue_;
    std::string  name_;
    Callback     callback_;
};

class Graph
{
public:

    Graph();

    void registerPass(PassBase *pass);

    Pass *addPass();

    void waitBeforeFirstUsage(
        const Buffer &buffer,
        Semaphore     semaphore);

    void signalAfterLastUsage(
        const Buffer &buffer,
        Semaphore     semaphore,
        const Queue  *next_queue,
        bool          release_only);
    
    void waitBeforeFirstUsage(
        const Image                &image,
        const vk::ImageSubresource &subrsc,
        Semaphore                   semaphore);
    
    void waitBeforeFirstUsage(
        const Image                     &image,
        const vk::ImageSubresourceRange &range,
        Semaphore                        semaphore);

    void waitBeforeFirstUsage(
        const Image         &image,
        vk::ImageAspectFlags aspect,
        Semaphore            semaphore);

    void waitBeforeFirstUsage(
        const Image &image,
        Semaphore    semaphore);

    void signalAfterLastUsage(
        const Image                &image,
        const vk::ImageSubresource &subrsc,
        Semaphore                   semaphore,
        const Queue                *next_queue,
        vk::ImageLayout             next_layout,
        bool                        release_only);

    void signalAfterLastUsage(
        const Image                     &image,
        const vk::ImageSubresourceRange &range,
        Semaphore                        semaphore,
        const Queue                     *next_queue,
        vk::ImageLayout                  next_layout,
        bool                             release_only);

    void signalAfterLastUsage(
        const Image         &image,
        vk::ImageAspectFlags aspect,
        Semaphore            semaphore,
        const Queue         *next_queue,
        vk::ImageLayout      next_layout,
        bool                 release_only);

    void signalAfterLastUsage(
        const Image     &image,
        Semaphore        semaphore,
        const Queue     *next_queue,
        vk::ImageLayout  next_layout,
        bool             release_only);

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

    void addDependency(std::initializer_list<PassBase *> passes);

    agz::alloc::memory_resource_arena_t memory_;
    agz::alloc::object_releaser_t       arena_;

    List<PassBase *> passes_;

    Map<Buffer, Semaphore>           buffer_waits_;
    Map<ImageSubresource, Semaphore> image_waits_;

    Map<Buffer, Signal>           buffer_signals_;
    Map<ImageSubresource, Signal> image_signals_;
};

template<typename...Args>
void Graph::addDependency(Args...passes)
{
    static_assert(
        (std::is_base_of_v<PassBase, std::remove_pointer_t<Args>> && ...));
    this->addDependency({ passes... });
}

VKPT_GRAPH_END
