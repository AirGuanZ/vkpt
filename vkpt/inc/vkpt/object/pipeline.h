#pragma once

#include <agz-utils/misc.h>

#include <vkpt/object/render_pass.h>
#include <vkpt/descriptor_set.h>

VKPT_BEGIN

class Pipeline;

struct PipelineLayoutDescription
{
    vk::PipelineLayoutCreateFlags      flags = {};
    std::vector<DescriptorSetLayout>   set_layouts;
    std::vector<vk::PushConstantRange> push_constant_ranges;
};

struct PipelineShaderSource
{
    std::string                        source;
    std::string                        source_name;
    std::string                        entry_name;
    std::map<std::string, std::string> macros;
    bool                               optimize = false;
};

struct PipelineDescription
{
    using InternalShader = agz::misc::variant_t<
        PipelineShaderSource,
        vk::PipelineShaderStageCreateInfo>;

    using InternalRenderPass = agz::misc::variant_t<
        std::vector<Attachment>,
        RenderPass>;

    vk::PipelineCreateFlags flags = {};

    InternalShader vertex_shader;
    InternalShader fragment_shader;
    
    InternalRenderPass render_pass;

    PipelineLayoutDescription layout;

    std::vector<vk::VertexInputBindingDescription>   vertex_input_bindings;
    std::vector<vk::VertexInputAttributeDescription> vertex_input_attributes;
    vk::PrimitiveTopology                            input_topology = vk::PrimitiveTopology::eTriangleList;

    std::vector<vk::Viewport> viewports;
    std::vector<vk::Rect2D>   scissors;

    vk::PipelineRasterizationStateCreateInfo rasterization;
    vk::PipelineMultisampleStateCreateInfo   multisample;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;

    std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
    std::array<float, 4>                               blend_constants = {};

    std::vector<vk::DynamicState> dynamic_states;
};

class Pipeline : public agz::misc::uncopyable_t
{
public:

    static Pipeline build(vk::Device device, const PipelineDescription &desc);

    Pipeline() = default;

    Pipeline(Pipeline &&other) noexcept = default;

    Pipeline &operator=(Pipeline &&other) noexcept = default;

    operator bool() const;

    vk::Pipeline getPipeline() const;

    const RenderPass &getRenderPass() const;

    Framebuffer createFramebuffer(std::vector<ImageView> image_views);

private:

    Pipeline(
        vk::UniquePipeline               pipeline,
        vk::UniquePipelineLayout         layout,
        RenderPass                       render_pass,
        std::vector<DescriptorSetLayout> set_layouts);

    vk::UniquePipeline               pipeline_;
    vk::UniquePipelineLayout         layout_;
    RenderPass                       render_pass_;
    std::vector<DescriptorSetLayout> set_layouts_;
};

VKPT_END
