#pragma once

#include <vkpt/object/framebuffer.h>
#include <vkpt/resource/image.h>

VKPT_BEGIN

// TODO: support multiple subpasses and resolve attachments

struct SubpassDescription
{
    vk::PipelineBindPoint                  bind_point;
    std::vector<vk::AttachmentReference>   input_attachments;
    std::vector<vk::AttachmentReference>   color_attachments;
    std::vector<vk::AttachmentReference>   resolve_attachments;
    std::optional<vk::AttachmentReference> depth_stencil_attachments;
    std::vector<uint32_t>                  preserve_attachments;
};

struct RenderPassDescription
{
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<SubpassDescription>        subpasses;
    std::vector<vk::SubpassDependency>     dependencies;
};

class RenderPass :
    public Object<vk::RenderPass, vk::UniqueRenderPass, RenderPassDescription>
{
public:

    static RenderPass build(vk::Device device, RenderPassDescription desc);

    using Object::Object;

    VKPT_USING_OBJECT_METHODS(vk::RenderPass);

    Framebuffer createFramebuffer(std::vector<ImageView> image_views) const;
};

VKPT_END
