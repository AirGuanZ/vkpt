#include <agz-utils/container.h>

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <vkpt/context.h>
#include <vkpt/imgui.h>
#include <vkpt/immediate_command_buffer.h>

VKPT_BEGIN

namespace
{

    vk::UniqueDescriptorPool createDescriptorPool(vk::Device device)
    {
        vk::DescriptorPoolSize pool_sizes[] = {
            { vk::DescriptorType::eSampler,              1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eSampledImage,         1000 },
            { vk::DescriptorType::eStorageImage,         1000 },
            { vk::DescriptorType::eUniformTexelBuffer,   1000 },
            { vk::DescriptorType::eStorageTexelBuffer,   1000 },
            { vk::DescriptorType::eUniformBuffer,        1000 },
            { vk::DescriptorType::eStorageBuffer,        1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment,      1000 }
        };

        vk::DescriptorPoolCreateInfo create_info = {
            .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets       = 1000 * agz::array_size<uint32_t>(pool_sizes),
            .poolSizeCount = agz::array_size<uint32_t>(pool_sizes),
            .pPoolSizes    = pool_sizes
        };

        return device.createDescriptorPoolUnique(create_info);
    }

    vk::UniqueRenderPass createRenderPass(
        vk::Device device, vk::Format format, vk::SampleCountFlagBits samples)
    {
        vk::AttachmentDescription attachment = {
            .format         = format,
            .samples        = samples,
            .loadOp         = vk::AttachmentLoadOp::eLoad,
            .storeOp        = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout  = vk::ImageLayout::eColorAttachmentOptimal,
            .finalLayout    = vk::ImageLayout::eColorAttachmentOptimal
        };

        vk::AttachmentReference attachment_reference = {
            .attachment = 0,
            .layout     = vk::ImageLayout::eColorAttachmentOptimal
        };

        vk::SubpassDescription subpass = {
            .pipelineBindPoint    = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachment_reference
        };

        vk::RenderPassCreateInfo create_info = {
            .attachmentCount = 1,
            .pAttachments    = &attachment,
            .subpassCount    = 1,
            .pSubpasses      = &subpass
        };

        return device.createRenderPassUnique(create_info);
    }

} // namespace anonymous

struct ImGuiIntegration::Impl
{
    using Framebuffers =
        agz::container::linked_map_t<vk::ImageView, vk::UniqueFramebuffer>;

    vk::Device               device;
    Queue                   *queue;
    vk::UniqueDescriptorPool descriptor_pool;
    ImGuiContext            *context = nullptr;
    vk::UniqueRenderPass     render_pass;

    uint32_t           image_count = 0;
    Framebuffers       framebuffers;
};

ImGuiIntegration::ImGuiIntegration(
    GLFWwindow             *window,
    vk::Instance            instance,
    vk::PhysicalDevice      physical_device,
    vk::Device              device,
    Queue                  *queue,
    uint32_t                image_count,
    const ImageDescription &image_desc)
{
    impl_ = std::make_unique<Impl>();
    impl_->device      = device;
    impl_->queue       = queue;
    impl_->image_count = image_count;

    auto context = ImGui::CreateContext();
    impl_->context = context;

    ImGui_ImplGlfw_InitForVulkan(window, false);

    impl_->descriptor_pool = createDescriptorPool(device);
    impl_->render_pass = createRenderPass(
        device, image_desc.format, image_desc.samples);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance        = instance;
    init_info.PhysicalDevice  = physical_device;
    init_info.Device          = device;
    init_info.QueueFamily     = queue->getFamilyIndex();
    init_info.Queue           = queue->getRaw();
    init_info.DescriptorPool  = impl_->descriptor_pool.get();
    init_info.ImageCount      = image_count;
    init_info.CheckVkResultFn = [](VkResult result)
    {
        if(result != VK_SUCCESS)
            throw VKPTException("failed to initialize imgui vulkan backend");
    };
    ImGui_ImplVulkan_Init(&init_info, impl_->render_pass.get());

    ImmediateCommandBuffer command_buffer(queue);
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer.getRaw());
    command_buffer.submitAndSync();
    device.waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    ImGui::StyleColorsLight();
}

ImGuiIntegration::ImGuiIntegration(ImGuiIntegration &&other) noexcept
{
    swap(other);
}

ImGuiIntegration &ImGuiIntegration::operator=(ImGuiIntegration &&other) noexcept
{
    swap(other);
    return *this;
}

ImGuiIntegration::~ImGuiIntegration()
{
    if(!impl_)
        return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    impl_->descriptor_pool.reset();
    impl_->render_pass.reset();
    impl_->framebuffers.clear();

    if(impl_->context)
        ImGui::DestroyContext(impl_->context);
}

void ImGuiIntegration::swap(ImGuiIntegration &other) noexcept
{
    std::swap(impl_, other.impl_);
}

void ImGuiIntegration::newFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiIntegration::render(
    const ImageView &image_view, CommandBuffer &command_buffer)
{
    const uint32_t width  = image_view.getImage().getDescription().extent.width;
    const uint32_t height = image_view.getImage().getDescription().extent.height;

    auto framebuffer = impl_->framebuffers.find_and_erase(image_view);
    if(!framebuffer)
    {
        vk::ImageView raw_image_view = image_view;
        framebuffer = impl_->device.createFramebufferUnique(
            vk::FramebufferCreateInfo{
                .renderPass      = impl_->render_pass.get(),
                .attachmentCount = 1,
                .pAttachments    = &raw_image_view,
                .width           = width,
                .height          = height,
                .layers          = 1
            });
    }
    assert(framebuffer);

    command_buffer.getRaw().beginRenderPass(vk::RenderPassBeginInfo{
        .renderPass  = impl_->render_pass.get(),
        .framebuffer = framebuffer->get(),
        .renderArea  = vk::Rect2D{
            .offset = { 0, 0 },
            .extent = { width, height }
        }
    }, vk::SubpassContents::eInline);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(
        ImGui::GetDrawData(), command_buffer.getRaw());

    command_buffer.getRaw().endRenderPass();

    impl_->framebuffers.push_front(image_view, std::move(*framebuffer));
    if(impl_->framebuffers.size() > impl_->image_count)
        impl_->framebuffers.pop_back();
}

rg::Pass *ImGuiIntegration::addToGraph(
    const ImageView &image_view, rg::Graph &graph)
{
    auto pass = graph.addPass(impl_->queue);
    pass->use(image_view.getImage(), rg::state::RenderTargetColor);
    pass->setCallback([image_view, this](CommandBuffer &command_buffer)
    {
        render(image_view, command_buffer);
    });
    return pass;
}

void ImGuiIntegration::clearFramebufferCache()
{
    impl_->framebuffers.clear();
}

VKPT_END
