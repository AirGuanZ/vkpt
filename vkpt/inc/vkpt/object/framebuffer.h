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

class Framebuffer : public Object<
    vk::Framebuffer, vk::UniqueFramebuffer, FramebufferDescription>
{
public:

    using Object::Object;

    VKPT_USING_OBJECT_METHODS(vk::Framebuffer);

    auto operator<=>(const Framebuffer &) const = default;

    vk::Viewport getFullViewport(
        float min_depth = 0, float max_depth = 1) const;

    vk::Rect2D getFullScissor() const;
};

inline vk::Viewport Framebuffer::getFullViewport(
    float min_depth, float max_depth) const
{
    return vk::Viewport{
        .x        = 0,
        .y        = 0,
        .width    = static_cast<float>(getDescription().width),
        .height   = static_cast<float>(getDescription().height),
        .minDepth = min_depth,
        .maxDepth = max_depth
    };
}

inline vk::Rect2D Framebuffer::getFullScissor() const
{
    return vk::Rect2D{
        .offset = { 0, 0 },
        .extent = {
            .width  = getDescription().width,
            .height = getDescription().height
        }
    };
}

VKPT_END
