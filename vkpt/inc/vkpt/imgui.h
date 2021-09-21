#pragma once

#include <imgui/imgui.h>

#include <vkpt/object/queue.h>
#include <vkpt/graph/graph.h>
#include <vkpt/command_buffer.h>
#include <vkpt/resource_allocator.h>

struct GLFWwindow;

VKPT_BEGIN

class Context;

class ImGuiIntegration : public agz::misc::uncopyable_t
{
public:
    
    ImGuiIntegration(
        GLFWwindow             *window,
        vk::Instance            instance,
        vk::PhysicalDevice      physical_device,
        vk::Device              device,
        Queue                  *queue,
        uint32_t                image_count,
        const ImageDescription &image_desc);

    ImGuiIntegration(ImGuiIntegration &&other) noexcept;

    ImGuiIntegration &operator=(ImGuiIntegration &&other) noexcept;

    ~ImGuiIntegration();

    void swap(ImGuiIntegration &other) noexcept;

    void newFrame();

    void render(const ImageView &image_view, CommandBuffer &command_buffer);

    rg::Pass *addToGraph(const ImageView &image_view, rg::Graph &graph);

    void clearFramebufferCache();

private:

    struct Impl;

    std::unique_ptr<Impl> impl_;
};

VKPT_END
