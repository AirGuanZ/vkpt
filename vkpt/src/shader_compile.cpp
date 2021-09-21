#include <iostream>

#include <shaderc/shaderc.hpp>

#include <vkpt/shader_compile.h>

VKPT_BEGIN

std::vector<uint32_t> compileGLSLToSPIRV(
    const std::string                        &source,
    const std::string                        &source_name,
    const std::map<std::string, std::string> &macros,
    ShaderType                                type,
    bool                                      optimize)
{
    shaderc_shader_kind shader_kind = {};
    switch(type)
    {
    case ShaderType::Vertex:
        shader_kind = shaderc_glsl_vertex_shader;
        break;
    case ShaderType::Fragment:
        shader_kind = shaderc_glsl_fragment_shader;
        break;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    for(auto &p : macros)
        options.AddMacroDefinition(p.first, p.second);

    if(optimize)
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
    else
    {
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetGenerateDebugInfo();
    }

    auto result = compiler.CompileGlslToSpv(
        source, shader_kind, source_name.c_str(), options);
    if(result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
#ifdef VKPT_DEBUG
        std::cerr << result.GetErrorMessage() << std::endl;
#endif
        throw VKPTException(result.GetErrorMessage());
    }

    return { result.begin(), result.end() };
}

VKPT_END
