#pragma once

#include <vkpt/graph/common.h>

VKPT_GRAPH_BEGIN

struct ResourceUsage
{
    vk::PipelineStageFlags2KHR stages;
    vk::AccessFlags2KHR        access;
    vk::ImageLayout            layout = vk::ImageLayout::eUndefined;

    auto operator<=>(const ResourceUsage &) const = default;
};

inline ResourceUsage operator|(const ResourceUsage &a, const ResourceUsage &b)
{
    assert(a.layout == b.layout);
    return ResourceUsage{
        .stages = a.stages | b.stages,
        .access = a.access | b.access,
        .layout = a.layout
    };
}

constexpr ResourceUsage USAGE_NIL = ResourceUsage{
    .stages = vk::PipelineStageFlagBits2KHR::eNone,
    .access = vk::AccessFlagBits2KHR::eNone,
    .layout = vk::ImageLayout::eUndefined
};

constexpr ResourceUsage USAGE_RENDER_TARGET = ResourceUsage{
    .stages = vk::PipelineStageFlagBits2KHR::eColorAttachmentOutput,
    .access = vk::AccessFlagBits2KHR::eColorAttachmentWrite,
    .layout = vk::ImageLayout::eColorAttachmentOptimal
};

VKPT_GRAPH_END
