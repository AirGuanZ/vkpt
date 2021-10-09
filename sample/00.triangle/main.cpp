#include <iostream>

#include <vkpt/frame/transient_images.h>
#include <vkpt/graph/graph.h>
#include <vkpt/object/pipeline.h>
#include <vkpt/context.h>
#include <vkpt/utility/vertex.h>

using namespace vkpt;

constexpr auto SAMPLE_COUNT = vk::SampleCountFlagBits::e4;

VKPT_VERTEX_BEGIN(Vertex)
    VKPT_MEMBER(Vec3f, position)
    VKPT_MEMBER(Vec3f, color)
VKPT_VERTEX_END

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
            .attachments = std::vector<vk::AttachmentDescription>{
                {
                    .format         = context.getImageFormat(),
                    .samples        = vk::SampleCountFlagBits::e1,
                    .loadOp         = vk::AttachmentLoadOp::eDontCare,
                    .storeOp        = vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout  = vk::ImageLayout::eColorAttachmentOptimal,
                    .finalLayout    = vk::ImageLayout::eColorAttachmentOptimal
                },
                {
                    .format         = context.getImageFormat(),
                    .samples        = SAMPLE_COUNT,
                    .loadOp         = vk::AttachmentLoadOp::eClear,
                    .storeOp        = vk::AttachmentStoreOp::eDontCare,
                    .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout  = vk::ImageLayout::eColorAttachmentOptimal,
                    .finalLayout    = vk::ImageLayout::eColorAttachmentOptimal
                }
            },
            .subpasses = std::vector<SubpassDescription>{
                {
                    .bind_point          = vk::PipelineBindPoint::eGraphics,
                    .color_attachments   = { { 1, vk::ImageLayout::eColorAttachmentOptimal } },
                    .resolve_attachments = { { 0, vk::ImageLayout::eColorAttachmentOptimal } }
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
        .vertex_input_attributes = Vertex::getVertexInputAttributes(),
        .input_topology = vk::PrimitiveTopology::eTriangleList,
        .rasterization = {
            .depthClampEnable        = false,
            .rasterizerDiscardEnable = false,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eBack,
            .frontFace               = vk::FrontFace::eClockwise,
            .depthBiasEnable         = false,
            .lineWidth               = 1
        },
        .multisample = {
            .rasterizationSamples = SAMPLE_COUNT,
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
        },
        .dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        }
    });
}

std::vector<Framebuffer> createFramebuffers(
    Context        &context,
    const Pipeline &pipeline,
    ImageView       color_render_target_view)
{
    std::vector<Framebuffer> result;
    for(uint32_t i = 0; i < context.getImageCount(); ++i)
    {
        result.push_back(pipeline.getRenderPass().createFramebuffer(
            { context.getImageView(i), color_render_target_view }));
    }
    return result;
}

Buffer createTriangleVertexBuffer(Context &context)
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

    uploader.uploadBuffer(buffer, vertices, bytes);
    uploader.submitAndSync();

    return buffer;
}

ImageView createColorRenderTarget(Context &context)
{
    auto color_render_target = context.getResourceAllocator().createImage(
        vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format    = context.getImageFormat(),
            .extent    = {
                context.getFramebufferSizeX(),
                context.getFramebufferSizeY(),
                1
            },
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = SAMPLE_COUNT,
            .usage         = vk::ImageUsageFlagBits::eColorAttachment,
            .sharingMode   = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined
        }, vma::MemoryUsage::eGPUOnly);

    auto color_render_target_view = color_render_target.createView(
        vk::ImageViewType::e2D,
        vk::ImageSubresourceRange{
            .aspectMask     = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        },
        vk::ComponentMapping{
            .r = vk::ComponentSwizzle::eIdentity,
            .g = vk::ComponentSwizzle::eIdentity,
            .b = vk::ComponentSwizzle::eIdentity,
            .a = vk::ComponentSwizzle::eIdentity,
        });
    return color_render_target_view;
}

void run()
{
    Context context(Context::Description{
        .width       = 640,
        .height      = 480,
        .maximized   = true,
        .title       = "vkpt-main",
        .image_count = 3
    });

    auto frame_resources = context.createFrameResources();

    auto pipeline = createPipeline(context);
    auto color_render_target = createColorRenderTarget(context);
    auto framebuffers = createFramebuffers(
        context, pipeline, color_render_target);

    auto vertex_buffer = createTriangleVertexBuffer(context);

    context.attach([&](const RecreateSwapchain &e)
    {
        if(!context.isMinimized())
        {
            color_render_target = createColorRenderTarget(context);
            framebuffers = createFramebuffers(
                context, pipeline, color_render_target);
        }
    });

    auto &imgui = context.getImGuiIntegration();

    AGZ_SCOPE_EXIT{ context.waitIdle(); };

    while(!context.getCloseFlag())
    {
        context.doEvents();
        if(context.isMinimized() || !context.acquireNextImage())
            continue;

        if(context.getInput()->isDown(KEY_ESCAPE))
            context.setCloseFlag(true);

        frame_resources.beginFrame();
        imgui.newFrame();
        
        if(ImGui::Begin("vkpt", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("hello, world!");
        }
        ImGui::End();

        rg::Graph graph;

        graph.waitBeforeFirstUsage(
            context.getImage(), context.getImageAvailableSemaphore());

        graph.signalAfterLastUsage(
            context.getImage(), context.getPresentAvailableSemaphore(),
            context.getPresentQueue(), vk::ImageLayout::ePresentSrcKHR, false);

        auto triangle_pass = graph.addPass(context.getGraphicsQueue());
        triangle_pass->use(context.getImage(), rg::USAGE_RENDER_TARGET);
        triangle_pass->use(color_render_target.getImage(), rg::USAGE_RENDER_TARGET);
        triangle_pass->setCallback([&](rg::PassContext &pass_context)
        {
            auto command_buffer = pass_context.getCommandBuffer();

            command_buffer.beginPipeline(
                pipeline, framebuffers[context.getImageIndex()],
                {
                    vk::ClearColorValue{ std::array{ 0.0f, 0.0f, 0.0f, 0.0f } },
                    vk::ClearColorValue{ std::array{ 0.0f, 0.0f, 0.0f, 0.0f } }
                });

            command_buffer.setViewport(
                { context.getDefaultFramebufferViewport() });
            command_buffer.setScissor(
                { context.getFramebufferRect() });

            command_buffer.bindVertexBuffers({ vertex_buffer });
            command_buffer.draw(3, 1, 0, 0);

            command_buffer.endPipeline();
        });

        auto imgui_pass = imgui.addToGraph(context.getImageView(), graph);

        graph.addDependency(triangle_pass, imgui_pass);

        graph.execute(frame_resources, frame_resources);

        context.swapBuffers();

        frame_resources.endFrame({ context.getGraphicsQueue() });
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
