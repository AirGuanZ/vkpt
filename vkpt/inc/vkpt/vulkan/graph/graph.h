#pragma once

#include <agz-utils/thread.h>

#include <vkpt/vulkan/command_buffer.h>
#include <vkpt/vulkan/resource_allocator.h>

VKPT_BEGIN

class Context;

enum class ResourceState
{
    DepthRead,    // depth test
    DepthWrite,   // depth test & write
    ColorWrite,   // color output
    GraphicsRead, // graphics shader resource
    ComputeRead,  // compute shader resource
    ComputeWrite, // compute shader write
    TransferSrc,  // transfer source
    TransferDst,  // transfer destination
    VertexRead,   // vertex buffer
    IndexRead,    // index buffer
    Present       // present image
};

class Pass
{
public:

    void use(vk::Buffer buffer, ResourceState usage);

    void use(vk::Image image, ResourceState usage);

    void wait(
        vk::ArrayProxy<const vk::Semaphore>        external_semaphores,
        vk::ArrayProxy<const vk::ShaderStageFlags> wait_stages);

    void wait(vk::Fence external_fence);

    void signal(vk::ArrayProxy<const vk::Semaphore> external_semaphores);

    void signal(vk::Fence external_fence);
};

class RenderGraph
{
public:

    using PassCallback = std::function<void(CommandBuffer *)>;

    using ThreadPool = agz::thread::thread_group_t;

    Pass *getPrePass();

    Pass *getPostPass();

    Pass *addGraphicsPass(PassCallback callback);

    Pass *addComputePass(PassCallback callback);

    Pass *addTransferPass(PassCallback callback);

    void addDependency(Pass *head, Pass *tail);

    void clear();

    void execute(Context &context, ThreadPool &threads);
};

VKPT_END
