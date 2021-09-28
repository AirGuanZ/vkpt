#pragma once

#include <agz-utils/misc.h>

#include <vkpt/common.h>

VKPT_BEGIN

#define VKPT_VERTEX_BEGIN(NAME) struct NAME { AGZ_BEGIN_CLASS_RELECTION(NAME);
#define VKPT_VERTEX_END                                                         \
    public:                                                                     \
        static constexpr int getMemberCount()                                   \
        {                                                                       \
            return vkpt::detail::getVertexInputAttribCount<_agz_refl_self_t>(); \
        }                                                                       \
        static const auto &getVertexInputAttributes()                           \
        {                                                                       \
            static auto ret =                                                   \
                vkpt::detail::getVertexInputAttributesAux<_agz_refl_self_t>();  \
            return ret;                                                         \
        }                                                                       \
    };

#define VKPT_MEMBER(TYPE, NAME) TYPE NAME; AGZ_MEMBER_VARIABLE_REFLECTION(NAME);

namespace detail
{

    template<typename T>
    struct VertexAttribFormatTrait { };

#define VERTEX_ATTRIBUTE_FORMAT_TRAIT(TYPE, FORMAT)                             \
    template<>                                                                  \
    struct VertexAttribFormatTrait<TYPE>                                        \
    {                                                                           \
        static constexpr vk::Format format = FORMAT;                            \
    }

    VERTEX_ATTRIBUTE_FORMAT_TRAIT(float, vk::Format::eR32Sfloat);
    VERTEX_ATTRIBUTE_FORMAT_TRAIT(Vec2f, vk::Format::eR32G32Sfloat);
    VERTEX_ATTRIBUTE_FORMAT_TRAIT(Vec3f, vk::Format::eR32G32B32Sfloat);
    VERTEX_ATTRIBUTE_FORMAT_TRAIT(Vec4f, vk::Format::eR32G32B32A32Sfloat);

#undef VERTEX_ATTRIBUTE_FORMAT_TRAIT

    template<typename T>
    constexpr int getVertexInputAttribCount()
    {
        return agz::misc::refl::get_member_variable_count<T>();
    }

    template<typename T>
    auto getVertexInputAttributesAux()
    {
        std::vector<vk::VertexInputAttributeDescription> ret;
        ret.reserve(getVertexInputAttribCount<T>());
        agz::misc::refl::for_each_member_variable<T>(
            [&ret](auto member_ptr, auto name)
        {
            using Member = agz::rm_rcv_t<decltype(std::declval<T>().*member_ptr)>;

            const uint32_t   location = static_cast<uint32_t>(ret.size());
            const vk::Format format   = VertexAttribFormatTrait<Member>::format;
            const size_t     offset   = agz::byte_offset(member_ptr);

            ret.push_back(vk::VertexInputAttributeDescription{
                .location = location,
                .binding  = 0,
                .format   = format,
                .offset   = static_cast<uint32_t>(offset)
            });
        });
        return ret;
    }

} // namespace detail

VKPT_END
