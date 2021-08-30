#pragma once

#include <compare>
#include <stdexcept>

#include <agz-utils/math.h>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

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

    using runtime_error::runtime_error;
};

VKPT_END
