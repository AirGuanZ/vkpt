#pragma once

#include <bitset>
#include <iostream>

#include <vkpt/graph/graph.h>

VKPT_GRAPH_BEGIN

class Messenger : public agz::misc::uncopyable_t
{
public:

    template<typename...Args>
    [[noreturn]] void fatal(const char *fmt, Args &&...args)
    {
        auto msg = std::format(fmt, std::forward<Args>(args)...);
        msg_(msg);
        throw VKPTException("{}", msg);
    }

    template<typename...Args>
    void warning(const char *fmt, Args &&...args)
    {
        msg_(std::format(fmt, std::forward<Args>(args)...));
    }

protected:

    std::function<void(const std::string &)> msg_ =
        [](const std::string &msg) { std::cerr << msg << std::endl; };
};

struct CompileGroup;

struct CompilePass
{
    explicit CompilePass(std::pmr::memory_resource &memory);

    PassBase    *raw_pass;
    const Queue *queue;

    CompileGroup *group;

    uint32_t unprocessed_head_count;
    int      sorted_index;
    int      sorted_index_in_group;

    std::bitset<64> group_flags;

    Set<CompilePass *> tails;
    Set<CompilePass *> heads;
    
    Set<vk::BufferMemoryBarrier2KHR> pre_ext_buffer_barriers;
    Set<vk::ImageMemoryBarrier2KHR>  pre_ext_image_barriers;
    Set<vk::BufferMemoryBarrier2KHR> post_ext_buffer_barriers;
    Set<vk::ImageMemoryBarrier2KHR>  post_ext_image_barriers;

    Map<Buffer, vk::BufferMemoryBarrier2KHR>          pre_buffer_barriers;
    Map<ImageSubresource, vk::ImageMemoryBarrier2KHR> pre_image_barriers;

    Map<Semaphore, vk::SemaphoreSubmitInfoKHR> wait_semaphores;
    Map<Semaphore, vk::SemaphoreSubmitInfoKHR> signal_semaphores;

    Map<Buffer, Pass::BufferUsage>          generated_buffer_usages_;
    Map<ImageSubresource, Pass::ImageUsage> generated_image_usages_;
};

struct CompileGroup
{
    explicit CompileGroup(std::pmr::memory_resource &memory);

    const Queue          *queue;
    Vector<CompilePass *> passes;

    bool              need_tail_semaphore;
    TimelineSemaphore tail_semaphore;

    Set<CompileGroup *>                                   tails;
    Map<const CompileGroup *, vk::PipelineStageFlags2KHR> heads;

    int unprocessed_head_count;
};

struct CompileBufferUsage
{
    CompilePass               *pass;
    vk::PipelineStageFlags2KHR stages;
    vk::AccessFlags2KHR        access;

    operator Pass::BufferUsage() const;
};

struct CompileImageUsage
{
    CompilePass               *pass;
    vk::PipelineStageFlags2KHR stages;
    vk::AccessFlags2KHR        access;
    vk::ImageLayout            layout;
    vk::ImageLayout            exit_layout;

    operator Pass::ImageUsage() const;
};

struct CompileBuffer
{
    explicit CompileBuffer(std::pmr::memory_resource &memory);

    List<CompileBufferUsage> usages;
    bool              has_wait_semaphore   = false;
    bool              has_signal_semaphore = false;

    Map<CompilePass *, List<CompileBufferUsage>::iterator> pass_to_usage;
};

struct CompileImage
{
    explicit CompileImage(std::pmr::memory_resource &memory);

    List<CompileImageUsage> usages;
    bool             has_wait_semaphore = false;
    bool             has_signal_semaphore = false;

    Map<CompilePass *, List<CompileImageUsage>::iterator> pass_to_usage;
};

using CompileResource = agz::misc::variant_t<Buffer, ImageSubresource>;

struct GlobalGroupDependency
{
    const CompileGroup *start_exit_tail;
    const CompileGroup *end_entry_head;
};

VKPT_GRAPH_END
