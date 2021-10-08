#pragma once

#include <vkpt/object/framebuffer.h>
#include <vkpt/resource/image.h>

VKPT_BEGIN

// TODO: support multiple subpasses and resolve attachments

struct Attachment
{
    vk::Format              format;
    vk::SampleCountFlagBits samples;
    vk::AttachmentLoadOp    load_op;
    vk::AttachmentStoreOp   store_op;
    vk::AttachmentLoadOp    stencil_load_op = vk::AttachmentLoadOp::eDontCare;
    vk::AttachmentStoreOp   stencil_store_op = vk::AttachmentStoreOp::eDontCare;
    vk::ImageLayout         layout;
};

struct RenderPassDescription
{
    std::vector<Attachment> attachments;
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
