#pragma once

#include <format>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

#include <agz-utils/math.h>
#include <vulkan/vulkan.hpp>

#define VKPT_BEGIN namespace vkpt {
#define VKPT_END   }

#if defined(DEBUG) || defined(_DEBUG)
#define VKPT_DEBUG
#endif

VKPT_BEGIN

using Vec2f = agz::math::vec2f;
using Vec3f = agz::math::vec3f;
using Vec4f = agz::math::vec4f;

using Mat4f   = agz::math::mat4f_c;
using Trans4f = Mat4f::left_transform;

class VKPTException : public std::runtime_error
{
public:

    template<typename...Args>
    explicit VKPTException(std::string_view fmt, Args &&...args)
        : runtime_error(std::format(fmt, std::forward<Args>(args)...))
    {
        
    }
};

template<typename T>
using Vector = std::pmr::vector<T>;

template<typename T>
using List = std::pmr::list<T>;

template<typename K, typename V>
using Map = std::pmr::map<K, V>;

template<typename T>
using Set = std::pmr::set<T>;

template<typename T>
using PmrQueue = std::queue<T, std::pmr::deque<T>>;

template<typename F>
auto for_each_subrsc(const vk::ImageSubresourceRange &range, const F &f)
{
    for(uint32_t i = 0; i < range.layerCount; ++i)
    {
        for(uint32_t j = 0; j < range.levelCount; ++j)
        {
            const vk::ImageSubresource subrsc = {
                .aspectMask = range.aspectMask,
                .mipLevel   = range.baseMipLevel + j,
                .arrayLayer = range.baseArrayLayer + i
            };

            if constexpr(std::is_void_v<decltype(f(vk::ImageSubresource{}))>)
            {
                f(subrsc);
            }
            else
            {
                if(!f(subrsc))
                    return false;
            }
        }
    }

    if constexpr(!std::is_void_v<decltype(f(vk::ImageSubresource{})) > )
    {
        return true;
    }
}

constexpr bool isDepthStencilFormat(vk::Format format)
{
    return
        format == vk::Format::eS8Uint           ||
        format == vk::Format::eD16Unorm         ||
        format == vk::Format::eX8D24UnormPack32 ||
        format == vk::Format::eD32Sfloat        ||
        format == vk::Format::eD16UnormS8Uint   ||
        format == vk::Format::eD24UnormS8Uint   ||
        format == vk::Format::eD32SfloatS8Uint;
}

VKPT_END
