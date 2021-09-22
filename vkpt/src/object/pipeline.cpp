#include <agz-utils/file.h>

#include <vkpt/object/pipeline.h>
#include <vkpt/shader_compile.h>

VKPT_BEGIN

Pipeline Pipeline::build(vk::Device device, const PipelineDescription &desc)
{
    // shader

    vk::UniqueShaderModule temp_vertex_shader;
    vk::UniqueShaderModule temp_fragment_shader;

    vk::PipelineShaderStageCreateInfo shader_stages[2];

    match_variant(desc.vertex_shader,
        [&](const vk::PipelineShaderStageCreateInfo &info)
    {
        shader_stages[0] = info;
    },
        [&](const PipelineShaderSource &source)
    {
        std::string source_text = source.source;
        if(source_text.empty())
            source_text = agz::file::read_txt_file(source.source_name);

        auto byte_code = compileGLSLToSPIRV(
            source_text, source.source_name, source.macros,
            ShaderType::Vertex, source.optimize);

        temp_vertex_shader = device.createShaderModuleUnique(
            vk::ShaderModuleCreateInfo{
                .codeSize = byte_code.size() * sizeof(uint32_t),
                .pCode    = byte_code.data()
            });

        shader_stages[0] = vk::PipelineShaderStageCreateInfo{
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = temp_vertex_shader.get(),
            .pName  = source.entry_name.c_str()
        };
    });
    
    match_variant(desc.fragment_shader,
        [&](const vk::PipelineShaderStageCreateInfo &info)
    {
        shader_stages[1] = info;
    },
        [&](const PipelineShaderSource &source)
    {
        std::string source_text = source.source;
        if(source_text.empty())
            source_text = agz::file::read_txt_file(source.source_name);

        auto byte_code = compileGLSLToSPIRV(
            source_text, source.source_name, source.macros,
            ShaderType::Fragment, source.optimize);

        temp_fragment_shader = device.createShaderModuleUnique(
            vk::ShaderModuleCreateInfo{
                .codeSize = byte_code.size() * sizeof(uint32_t),
                .pCode    = byte_code.data()
            });

        shader_stages[1] = vk::PipelineShaderStageCreateInfo{
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = temp_fragment_shader.get(),
            .pName  = source.entry_name.c_str()
        };
    });

    // render pass

    RenderPass render_pass;

    if(auto desc_attachments = desc.render_pass.as_if<std::vector<Attachment>>())
    {
        render_pass = RenderPass::build(device, RenderPassDescription{
            .attachments = *desc_attachments
            });
    }
    else
        render_pass = desc.render_pass.as<RenderPass>();

    // pipeline layout

    std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
    descriptor_set_layouts.reserve(desc.layout.set_layouts.size());
    for(auto &l : desc.layout.set_layouts)
        descriptor_set_layouts.push_back(l.get());

    vk::PipelineLayoutCreateInfo pipeline_layout_create_info = {
        .flags                  = desc.layout.flags,
        .setLayoutCount         = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts            = descriptor_set_layouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(desc.layout.push_constant_ranges.size()),
        .pPushConstantRanges    = desc.layout.push_constant_ranges.data()
    };

    auto pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_create_info);

    // input assembly

    vk::PipelineVertexInputStateCreateInfo vertex_input = {
        .vertexBindingDescriptionCount   = static_cast<uint32_t>(desc.vertex_input_bindings.size()),
        .pVertexBindingDescriptions      = desc.vertex_input_bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.vertex_input_attributes.size()),
        .pVertexAttributeDescriptions    = desc.vertex_input_attributes.data()
    };

    vk::PipelineInputAssemblyStateCreateInfo input_assembly = {
        .topology = desc.input_topology
    };

    // viewport & scissor

    vk::PipelineViewportStateCreateInfo viewport_state = {
        .viewportCount = static_cast<uint32_t>(desc.viewports.size()),
        .pViewports    = desc.viewports.data(),
        .scissorCount  = static_cast<uint32_t>(desc.scissors.size()),
        .pScissors     = desc.scissors.data()
    };

    if(!viewport_state.viewportCount)
    {
        static const vk::Viewport empty_viewport = {};
        viewport_state.viewportCount = 1;
        viewport_state.pViewports    = &empty_viewport;
    }

    if(!viewport_state.scissorCount)
    {
        static const vk::Rect2D empty_scissor = {};
        viewport_state.scissorCount = 1;
        viewport_state.pScissors    = &empty_scissor;
    }

    // color blend

    vk::PipelineColorBlendStateCreateInfo color_blend = {
        .attachmentCount = static_cast<uint32_t>(desc.blend_attachments.size()),
        .pAttachments    = desc.blend_attachments.data(),
        .blendConstants  = desc.blend_constants
    };

    // dynamic state

    vk::PipelineDynamicStateCreateInfo dynamic_state = {
        .dynamicStateCount = static_cast<uint32_t>(desc.dynamic_states.size()),
        .pDynamicStates    = desc.dynamic_states.data()
    };

    // pipeline

    vk::GraphicsPipelineCreateInfo pipeline_create_info = {
        .flags               = desc.flags,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &desc.rasterization,
        .pMultisampleState   = &desc.multisample,
        .pDepthStencilState  = &desc.depth_stencil,
        .pColorBlendState    = &color_blend,
        .pDynamicState       = &dynamic_state,
        .layout              = pipeline_layout.get(),
        .renderPass          = render_pass.get(),
        .subpass             = 0
    };

    auto pipeline = device.createGraphicsPipelineUnique(
        nullptr, pipeline_create_info).value;

    auto result = Pipeline(
        std::move(pipeline),
        std::move(pipeline_layout),
        std::move(render_pass),
        desc.layout.set_layouts);

    return result;
}

Pipeline::operator bool() const
{
    return pipeline_.get();
}

vk::Pipeline Pipeline::getPipeline() const
{
    return pipeline_.get();
}

const RenderPass &Pipeline::getRenderPass() const
{
    return render_pass_;
}

Framebuffer Pipeline::createFramebuffer(std::vector<ImageView> image_views)
{
    return render_pass_.createFramebuffer(std::move(image_views));
}

Pipeline::Pipeline(
    vk::UniquePipeline               pipeline,
    vk::UniquePipelineLayout         layout,
    RenderPass                       render_pass,
    std::vector<DescriptorSetLayout> set_layouts)
    : pipeline_(std::move(pipeline)),
      layout_(std::move(layout)),
      render_pass_(std::move(render_pass)),
      set_layouts_(std::move(set_layouts))
{
    
}

VKPT_END
