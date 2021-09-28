#pragma once

#include <vk_mem_alloc.h>

#include <vkpt/resource/image.h>

VKPT_BEGIN

struct Image::Impl
{
    vk::Device device;
    vk::Image  image;

    // state_idx =
    //      layer * A +
    //      level * B +
    //      aspect.color   && C +
    //      aspect.depth   && D +
    //      aspect.stencil && E

    Description              description;
    std::unique_ptr<State[]> state;

    // C and E are always 0
    // see commented code in initializeStateIndices
    uint32_t A, B, /*C, */D/*, E*/;

    std::string name;

    void initializeStateIndices()
    {
        const vk::Format format = description.format;

        const bool has_depth_aspect =
            format == vk::Format::eD16Unorm         ||
            format == vk::Format::eX8D24UnormPack32 ||
            format == vk::Format::eD32Sfloat        ||
            format == vk::Format::eD16UnormS8Uint   ||
            format == vk::Format::eD24UnormS8Uint   ||
            format == vk::Format::eD32SfloatS8Uint;

        const bool has_stencil_aspect =
            format == vk::Format::eS8Uint         ||
            format == vk::Format::eD16UnormS8Uint ||
            format == vk::Format::eD24UnormS8Uint ||
            format == vk::Format::eD32SfloatS8Uint;

        D = (has_depth_aspect && has_stencil_aspect) ? 1 : 0;
        /*if(has_depth_aspect && has_stencil_aspect)
        {
            C = 0;
            D = 1;
            E = 0;
        }
        else if(has_depth_aspect)
        {
            C = 0;
            D = 0;
            E = 0;
        }
        else if(has_stencil_aspect)
        {
            C = 0;
            D = 0;
            E = 0;
        }
        else
        {
            C = 0;
            D = 0;
            E = 0;
        }*/

        B = (has_depth_aspect && has_stencil_aspect) ? 2 : 1;
        A = description.mip_levels * B;
    }

    uint32_t computeStateIndex(
        uint32_t layer, uint32_t level, vk::ImageAspectFlags aspect) const
    {
        return layer * A + level * B +
            /*(aspect == vk::ImageAspectFlagBits::eColor   ? C : 0) +*/
            (aspect == vk::ImageAspectFlagBits::eDepth   ? D : 0)/* +
            (aspect == vk::ImageAspectFlagBits::eStencil ? E : 0)*/;
    }
};

struct ImageView::Impl
{
    vk::UniqueImageView image_view;

    Image       image;
    Description description;
};

VKPT_END
