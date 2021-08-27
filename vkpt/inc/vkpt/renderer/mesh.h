#pragma once

#include <vkpt/vulkan/context.h>

VKPT_BEGIN

class Mesh
{
public:

    struct Vertex
    {
        Vec3f position;
        Vec3f normal;
        Vec2f tex_coord;
    };

    void loadFromFile(
        Context           &context,
        ResourceUploader  &uploader,
        const std::string &obj_filename,
        const std::string &albedo_filename);

private:

    UniqueBuffer vertex_buffer_;
    UniqueBuffer index_buffer_;
    UniqueImage  albedo_;
};

VKPT_END
