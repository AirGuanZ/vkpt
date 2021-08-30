#include <iostream>

#include <vkpt/vulkan/context.h>
#include <vkpt/vulkan/immediate_command_buffer.h>
#include <vkpt/vulkan/pipeline.h>
#include <vkpt/vulkan/resource_allocator.h>

using namespace vkpt;

struct Vertex
{
    Vec3f position;
    Vec3f color;
};

Pipeline createPipeline(Context &context)
{
    return context.createGraphicsPipeline(PipelineDescription{
        .vertex_shader = PipelineShaderSource{
            .source_name = "./asset/vertex.glsl",
            .entry_name = "main",
        },
        .fragment_shader = PipelineShaderSource{
            .source_name = "./asset/fragment.glsl",
            .entry_name = "main"
        },
        .render_pass = RenderPassDescription{
            .attachments = {
                {
                    .format         = context.getImageFormat(),
                    .samples        = vk::SampleCountFlagBits::e1,
                    .loadOp         = vk::AttachmentLoadOp::eClear,
                    .storeOp        = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout  = vk::ImageLayout::eColorAttachmentOptimal,
                    .finalLayout    = vk::ImageLayout::eColorAttachmentOptimal,
                }
            },
            .subpasses = {
                {
                    .pipeline_bind_point = vk::PipelineBindPoint::eGraphics,
                    .color_attachments = {
                        {
                            .attachment = 0,
                            .layout     = vk::ImageLayout::eColorAttachmentOptimal
                        }
                    }
                }
            }
        },
        .layout = {},
        .vertex_input_bindings = {
            {
                .binding   = 0,
                .stride    = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex
            }
        },
        .vertex_input_attributes = {
            {
                .location = 0,
                .binding  = 0,
                .format   = vk::Format::eR32G32B32Sfloat,
                .offset   = offsetof(Vertex, position)
            },
            {
                .location = 1,
                .binding  = 0,
                .format   = vk::Format::eR32G32B32Sfloat,
                .offset   = offsetof(Vertex, color)
            }
        },
        .input_topology = vk::PrimitiveTopology::eTriangleList,
        .viewports = { context.getDefaultFramebufferViewport() },
        .scissors = { context.getFramebufferRect() },
        .rasterization = {
            .depthClampEnable        = false,
            .rasterizerDiscardEnable = false,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eNone,
            .frontFace               = vk::FrontFace::eClockwise,
            .depthBiasEnable         = false,
            .lineWidth               = 1
        },
        .multisample = {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable  = false,
            .minSampleShading     = 1
        },
        .blend_attachments = {
            {
                .blendEnable    = false,
                .colorWriteMask = vk::ColorComponentFlagBits::eR |
                                  vk::ColorComponentFlagBits::eG |
                                  vk::ColorComponentFlagBits::eB |
                                  vk::ColorComponentFlagBits::eA
            }
        }
    });
}

std::vector<vk::UniqueFramebuffer> createFramebuffers(
    Context &context, const Pipeline &pipeline)
{
    std::vector<vk::UniqueFramebuffer> result;
    for(uint32_t i = 0; i < context.getImageCount(); ++i)
    {
        result.push_back(context.createFramebuffer(
            pipeline, { context.getImageView(i) }));
    }
    return result;
}

UniqueBuffer createTriangleVertexBuffer(Context &context)
{
    auto &alloc = context.getResourceAllocator();
    auto uploader = context.createGraphicsResourceUploader();

    const Vertex vertices[] = {
        { { -1, +1, 0 }, { 1, 0, 0 } },
        { { 0, -1, 0 }, { 0, 1, 0 } },
        { { +1, +1, 0 }, { 0, 0, 1 } }
    };
    constexpr size_t bytes = sizeof(vertices);

    auto buffer = alloc.createBuffer(vk::BufferCreateInfo{
            .size        = bytes,
            .usage       = vk::BufferUsageFlagBits::eVertexBuffer |
                           vk::BufferUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
        },
        vma::MemoryUsage::eGPUOnly);

    uploader.uploadBuffer(buffer.get(), vertices, bytes);
    uploader.submitAndSync();

    return buffer;
}

void run()
{
    Context context(Context::Description{
        .width       = 640,
        .height      = 480,
        .maximized   = true,
        .title       = "vkpt",
        .image_count = 3,
        .ray_tracing = false
    });
    
    auto pipeline = createPipeline(context);
    auto framebuffers = createFramebuffers(context, pipeline);

    auto render_semaphores = context.createSemaphores(context.getImageCount());

    auto vertex_buffer = createTriangleVertexBuffer(context);

    context.attach([&](const RecreateSwapchain &e)
    {
        if(!context.isMinimized())
        {
            pipeline = createPipeline(context);
            framebuffers = createFramebuffers(context, pipeline);
        }
    });

    AGZ_SCOPE_GUARD({ context.waitIdle(); });

    while(!context.getCloseFlag())
    {
        context.beginFrame();

        if(context.getInput()->isDown(KEY_ESCAPE))
            context.setCloseFlag(true);

        auto cmds = context.newFrameGraphicsCommandBuffer();

        cmds.begin();

        cmds.pipelineBarrier(
            {},
            {},
            vk::ImageMemoryBarrier2KHR{
                .srcStageMask        = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput,
                .dstStageMask        = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput,
                .dstAccessMask       = vk::AccessFlagBits2KHR::eColorAttachmentWrite,
                .oldLayout           = vk::ImageLayout::eUndefined,
                .newLayout           = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = context.getImage(),
                .subresourceRange    = vk::ImageSubresourceRange{
                .aspectMask          = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            });

        cmds.beginPipeline(
            pipeline,
            framebuffers[context.getImageIndex()].get(),
            { vk::ClearColorValue{ std::array{ 0.0f, 0.0f, 0.0f, 0.0f } } });

        cmds.bindVertexBuffers({ vertex_buffer.get() });

        cmds.draw(3, 1, 0, 0);

        cmds.endPipeline();
        
        cmds.pipelineBarrier(
            {},
            {},
            vk::ImageMemoryBarrier2KHR{
                .srcStageMask        = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput,
                .dstStageMask        = vk::PipelineStageFlagBits2KHR::eBottomOfPipe,
                .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout           = vk::ImageLayout::ePresentSrcKHR,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = context.getImage(),
                .subresourceRange    = vk::ImageSubresourceRange{
                .aspectMask          = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            });

        cmds.end();

        context.getGraphicsQueue().submit(
            { context.getImageAvailableSemaphore() },
            { vk::PipelineStageFlagBits::eColorAttachmentOutput },
            { render_semaphores[context.getImageIndex()].get() },
            { cmds },
            {});

        context.swapBuffers(
            render_semaphores[context.getImageIndex()].get());

        context.endFrame();
    }
}

int main()
{
    try
    {
        run();
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }
}
