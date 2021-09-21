#pragma once

#include <vkpt/object/object.h>

VKPT_BEGIN

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


};

VKPT_END
