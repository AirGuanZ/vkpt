#pragma once

#include <agz-utils/misc.h>

#include <vkpt/vulkan/descriptor_set.h>

VKPT_BEGIN

struct SubpassDescription
{
    vk::SubpassDescriptionFlags            flags = {};
    vk::PipelineBindPoint                  pipeline_bind_point = vk::PipelineBindPoint::eGraphics;
    std::vector<vk::AttachmentReference>   input_attachments;
    std::vector<vk::AttachmentReference>   color_attachments;
    std::vector<vk::AttachmentReference>   resolve_attachments;
    std::optional<vk::AttachmentReference> depth_stencil_attachment;
    std::vector<uint32_t>                  preserve_attachments;
};

struct RenderPassDescription
{
    vk::RenderPassCreateFlags              flags = {};
    std::vector<vk::AttachmentDescription> attachments;
    std::vector<SubpassDescription>        subpasses;
    std::vector<vk::SubpassDependency>     dependencies;
};

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
    using Shader = agz::misc::variant_t<
        PipelineShaderSource,
        vk::PipelineShaderStageCreateInfo>;

    vk::PipelineCreateFlags flags = {};

    Shader vertex_shader;
    Shader fragment_shader;

    RenderPassDescription     render_pass;
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

    vk::RenderPass getRenderPass() const;

    const vk::Viewport &getDefaultViewport() const;

    const vk::Rect2D &getDefaultRect() const;

private:

    Pipeline(
        vk::UniquePipeline               pipeline,
        vk::UniquePipelineLayout         layout,
        vk::UniqueRenderPass             render_pass,
        std::vector<DescriptorSetLayout> set_layouts);

    vk::UniquePipeline               pipeline_;
    vk::UniquePipelineLayout         layout_;
    vk::UniqueRenderPass             render_pass_;
    std::vector<DescriptorSetLayout> set_layouts_;

    vk::Viewport default_viewport_;
    vk::Rect2D   default_rect_;
};

VKPT_END
