#pragma once

#include <vkpt/object/image.h>
#include <vkpt/object/object.h>

VKPT_BEGIN

struct FramebufferDescription
{
    std::vector<ImageView> attachments;
    uint32_t               width;
    uint32_t               height;
};

using Framebuffer =
    Object<vk::Framebuffer, vk::UniqueFramebuffer, FramebufferDescription>;

VKPT_END
