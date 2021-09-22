#include <iostream>

#include <vkpt/graph/compiler.h>
#include <vkpt/object/pipeline.h>
#include <vkpt/context.h>
#include <vkpt/resource_allocator.h>
#include <vkpt/vertex.h>

using namespace vkpt;

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
        .render_pass = std::vector{
            Attachment{
                .format   = context.getImageFormat(),
                .samples  = vk::SampleCountFlagBits::e1,
                .load_op  = vk::AttachmentLoadOp::eClear,
                .store_op = vk::AttachmentStoreOp::eStore,
                .layout   = vk::ImageLayout::eColorAttachmentOptimal
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
        },
        .dynamic_states = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        }
    });
}

std::vector<Framebuffer> createFramebuffers(
    Context &context, const Pipeline &pipeline)
{
    std::vector<Framebuffer> result;
    for(uint32_t i = 0; i < context.getImageCount(); ++i)
    {
        result.push_back(pipeline.getRenderPass()
            .createFramebuffer({ context.getImageView(i) }));
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
    auto framebuffers = createFramebuffers(context, pipeline);

    auto vertex_buffer = createTriangleVertexBuffer(context);

    context.attach([&](const RecreateSwapchain &e)
    {
        if(!context.isMinimized())
            framebuffers = createFramebuffers(context, pipeline);
    });

    AGZ_SCOPE_GUARD({ context.waitIdle(); });

    auto &imgui = context.getImGuiIntegration();

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

        auto acquire_pass = context.addAcquireImagePass(graph);

        auto triangle_pass = graph.addPass(context.getGraphicsQueue());
        triangle_pass->use(context.getImage(), rg::state::RenderTargetColor);
        triangle_pass->setCallback([&](CommandBuffer &command_buffer)
        {
            command_buffer.beginPipeline(
                pipeline, framebuffers[context.getImageIndex()],
                { vk::ClearColorValue{ std::array{ 0.0f, 0.0f, 0.0f, 0.0f } } });

            command_buffer.setViewport(
                { context.getDefaultFramebufferViewport() });
            command_buffer.setScissor(
                { context.getFramebufferRect() });

            command_buffer.bindVertexBuffers({ vertex_buffer });
            command_buffer.draw(3, 1, 0, 0);

            command_buffer.endPipeline();
        });

        auto imgui_pass = imgui.addToGraph(context.getImageView(), graph);

        auto present_pass = context.addPresentImagePass(graph);

        graph.addDependency(
            acquire_pass, triangle_pass, imgui_pass, present_pass);

        graph.finalize();
        graph.execute(frame_resources);

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
