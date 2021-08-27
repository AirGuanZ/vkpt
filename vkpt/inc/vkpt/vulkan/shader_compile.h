#pragma once

#include <map>

#include <vkpt/common.h>

VKPT_BEGIN

enum class ShaderType
{
    Vertex,
    Fragment,
};

std::vector<uint32_t> compileGLSLToSPIRV(
    const std::string                        &source,
    const std::string                        &source_name,
    const std::map<std::string, std::string> &macros,
    ShaderType                                type,
    bool                                      optimize);

VKPT_END
