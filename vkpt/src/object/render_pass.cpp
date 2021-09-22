#include <vkpt/object/render_pass.h>

VKPT_BEGIN

RenderPass RenderPass::build(
    vk::Device device, RenderPassDescription desc)
{
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<vk::AttachmentReference>   color_references;
    vk::AttachmentReference                depth_stencil_reference;
    
    for(auto &a : desc.attachments)
    {
        const uint32_t index = static_cast<uint32_t>(attachments.size());

        attachments.push_back(vk::AttachmentDescription{
            .format         = a.format,
            .samples        = a.samples,
            .loadOp         = a.load_op,
            .storeOp        = a.store_op,
            .stencilLoadOp  = a.stencil_load_op,
            .stencilStoreOp = a.stencil_store_op,
            .initialLayout  = a.layout,
            .finalLayout    = a.layout
        });

        auto attachment_reference = vk::AttachmentReference{
            .attachment = index,
            .layout     = a.layout
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
            color_references.size() == attachments.size() ?
            nullptr : &depth_stencil_reference
    };

    vk::RenderPassCreateInfo render_pass_create_info = {
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments    = attachments.data(),
        .subpassCount    = 1,
        .pSubpasses      = &subpass_desc
    };

    auto render_pass = device.createRenderPassUnique(render_pass_create_info);
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

    auto device = image_views[0].getImage().getDescription().device;
    auto framebuffer = device.createFramebufferUnique(create_info);

    FramebufferDescription description = {
        .attachments = std::move(image_views),
        .width       = extent.width,
        .height      = extent.height
    };
    return Framebuffer(std::move(framebuffer), std::move(description));
}

VKPT_END
