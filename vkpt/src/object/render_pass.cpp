#include <vkpt/object/render_pass.h>

VKPT_BEGIN

RenderPass RenderPass::build(
    vk::Device device, RenderPassDescription desc)
{
    if(desc.subpasses.empty())
    {
        assert(desc.dependencies.empty());

        std::vector<vk::AttachmentReference>   color_references;
        vk::AttachmentReference                depth_stencil_reference;

        uint32_t index = 0;
        for(auto &a : desc.attachments)
        {
            auto attachment_reference = vk::AttachmentReference{
                .attachment = index++,
                .layout     = a.initialLayout
            };
            if(isDepthStencilFormat(a.format))
                depth_stencil_reference = attachment_reference;
            else
                color_references.push_back(attachment_reference);
        }

        vk::SubpassDescription subpass_desc = {
            .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount    = static_cast<uint32_t>(color_references.size()),
            .pColorAttachments       = color_references.data(),
            .pDepthStencilAttachment =
                color_references.size() == desc.attachments.size() ?
                nullptr : &depth_stencil_reference
        };

        vk::RenderPassCreateInfo render_pass_create_info = {
            .attachmentCount = static_cast<uint32_t>(desc.attachments.size()),
            .pAttachments    = desc.attachments.data(),
            .subpassCount    = 1,
            .pSubpasses      = &subpass_desc
        };

        auto render_pass = device.createRenderPassUnique(render_pass_create_info);
        return RenderPass(std::move(render_pass), std::move(desc));
    }

    std::vector<vk::SubpassDescription> subpasses;
    subpasses.reserve(desc.subpasses.size());

    for(auto &s : desc.subpasses)
    {
        subpasses.push_back(vk::SubpassDescription{
            .flags                   = {},
            .pipelineBindPoint       = s.bind_point,
            .inputAttachmentCount    = static_cast<uint32_t>(s.input_attachments.size()),
            .pInputAttachments       = s.input_attachments.data(),
            .colorAttachmentCount    = static_cast<uint32_t>(s.color_attachments.size()),
            .pColorAttachments       = s.color_attachments.data(),
            .pResolveAttachments     = s.resolve_attachments.data(),
            .pDepthStencilAttachment = s.depth_stencil_attachments ? &*s.depth_stencil_attachments : nullptr,
            .preserveAttachmentCount = static_cast<uint32_t>(s.preserve_attachments.size()),
            .pPreserveAttachments    = s.preserve_attachments.data()
        });
    }

    auto render_pass = device.createRenderPassUnique(vk::RenderPassCreateInfo{
        .flags           = {},
        .attachmentCount = static_cast<uint32_t>(desc.attachments.size()),
        .pAttachments    = desc.attachments.data(),
        .subpassCount    = static_cast<uint32_t>(subpasses.size()),
        .pSubpasses      = subpasses.data(),
        .dependencyCount = static_cast<uint32_t>(desc.dependencies.size()),
        .pDependencies   = desc.dependencies.data()
    });
    return RenderPass(std::move(render_pass), std::move(desc));
}

Framebuffer RenderPass::createFramebuffer(
    std::vector<ImageView> image_views) const
{
    assert(!image_views.empty());

    auto &desc = getDescription();
    assert(image_views.size() == desc.attachments.size());

    std::vector<vk::ImageView> attachments(desc.attachments.size());
    for(size_t i = 0; i < attachments.size(); ++i)
        attachments[i] = image_views[i];

    const vk::Extent3D extent =
        image_views[0].getImage().getDescription().extent;
    const vk::FramebufferCreateInfo create_info = {
        .renderPass      = get(),
        .attachmentCount = static_cast<uint32_t>(getDescription().attachments.size()),
        .pAttachments    = attachments.data(),
        .width           = extent.width,
        .height          = extent.height,
        .layers          = 1
    };

    auto device = image_views[0].getImage().getDevice();
    auto framebuffer = device.createFramebufferUnique(create_info);

    FramebufferDescription description = {
        .attachments = std::move(image_views),
        .width       = extent.width,
        .height      = extent.height
    };
    return Framebuffer(std::move(framebuffer), std::move(description));
}

VKPT_END
