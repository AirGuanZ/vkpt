#pragma once

#include <agz-utils/thread.h>

#include <vkpt/vulkan/command_buffer.h>
#include <vkpt/vulkan/queue.h>
#include <vkpt/vulkan/resource_allocator.h>

VKPT_BEGIN

class Context;

namespace graph
{

    struct ExecutableRenderGraph;

    class Pass
    {
    public:

        using Type = Queue::Type;

        using Callback = std::function<void(CommandBuffer &)>;

        Pass(Type type, int index);

        void set_callback(Callback callback);

        void use(vk::Buffer buffer, vk::PipelineStageFlags stages);

        void use(
            vk::Image                        image,
            vk::PipelineStageFlags           stages,
            vk::ImageLayout                  layout,
            const vk::ImageSubresourceRange &subrsc);

        void wait(
            vk::ArrayProxy<const vk::Semaphore>          semaphores,
            vk::ArrayProxy<const vk::PipelineStageFlags> stages);
        
        void signal(vk::ArrayProxy<const vk::Semaphore> semaphores);

        void signal(vk::Fence fence);

        void forceSubmit();

        Type getType() const;

        void _addHead(Pass *head, bool cut_edge);

        void _addTail(Pass *tail, bool cut_edge);

        uint32_t _getHeadCount() const;

        uint32_t _getTailCount() const;

    private:

        friend class RenderGraph;

        struct BufferUsage
        {
            vk::PipelineStageFlags stages;
        };

        struct ImageUsage
        {
            vk::PipelineStageFlags stages;
            vk::ImageLayout        layout;
        };

        using BufferUsageKey = vk::Buffer;

        struct ImageUsageKey
        {
            vk::Image                 image;
            vk::ImageSubresourceRange subrsc;

            auto operator<=>(const ImageUsageKey &) const = default;
        };

        struct PassDependency
        {
            Pass *pass     = nullptr;
            bool  cut_edge = false;
        };

        Type type_;
        int  index_;

        Callback callback_;

        std::map<BufferUsageKey, BufferUsage> buffer_usages_;
        std::map<ImageUsageKey, ImageUsage>   image_usages_;

        std::vector<vk::Semaphore>          wait_semaphores_;
        std::vector<vk::PipelineStageFlags> wait_stages_;
        std::vector<vk::Semaphore>          signal_semaphores_;

        vk::Fence signal_fence_;

        bool force_submit_ = false;

        std::vector<PassDependency> heads_;
        std::vector<PassDependency> tails_;

        // compile-time vars

        uint32_t compile_flag_ = 0;

        std::vector<Pass *> in_section_heads_;
        std::vector<Pass *> in_section_tails_;
    };

    class RenderGraph
    {
    public:

        using ThreadPool = agz::thread::thread_group_t;

        Pass *getPrePass();

        Pass *getPostPass();

        Pass *addGraphicsPass();

        Pass *addComputePass();

        Pass *addTransferPass();

        void addDependency(Pass *head, Pass *tail, bool cut_edge = false);

        void clear();

        void execute(Context &context, ThreadPool &threads);

    private:

        struct TempSection
        {
            std::vector<Pass *> passes;

            std::vector<TempSection*> heads;
            std::vector<TempSection*> tails;

            uint32_t unaccessed_head_count = 0;
        };

        struct TempPass
        {
            bool         should_submit = false;
            TempSection *section       = nullptr;
        };

        struct CompleteBufferUsage
        {
            vk::Buffer             buffer;
            vk::PipelineStageFlags head_stages;
            vk::PipelineStageFlags tail_stages;
        };

        struct CompleteImageUsage
        {
            vk::Image                 image;
            vk::ImageSubresourceRange subrsc;
            vk::PipelineStageFlags    head_stages;
            vk::ImageLayout           head_layout;
            vk::PipelineStageFlags    tail_stages;
            vk::ImageLayout           tail_layout;
        };

        void buildInnerSectionDependencies(std::vector<TempPass> &temp_passes);

        void buildInterSectionDependeicies(
            std::vector<TempPass>                     &temp_passes,
            std::vector<std::unique_ptr<TempSection>> &sections);

        std::vector<std::unique_ptr<TempSection>>
            divideSections(std::vector<TempPass> &passes);

        void topologySortPassesInSection(TempSection &section);

        void topologySortSections(
            std::vector<std::unique_ptr<TempSection>> &sections);

        void findCommonBufferUsages(
            Pass                             *head,
            Pass                             *tail,
            std::vector<CompleteBufferUsage> &common_buffer_usages);

        void findCommonImageUsages(
            Pass                            *head,
            Pass                            *tail,
            std::vector<CompleteImageUsage> &common_image_usages);

        void generateExecutableGraphSkeleton(
            Context                                   &context,
            std::vector<std::unique_ptr<TempSection>> &temp_sections,
            ExecutableRenderGraph                     &graph);

        void fillExecutableGraphFences(
            Context               &context,
            ExecutableRenderGraph &graph);

        void fillExecutableGraphSemaphores(
            Context               &context,
            ExecutableRenderGraph &graph);

        void fillExecutableGraphBarriers(
            Context               &context,
            ExecutableRenderGraph &graph);

        Pass pre_pass_  = Pass(Pass::Type::Graphics, -1);
        Pass post_pass_ = Pass(Pass::Type::Graphics, -1);

        std::vector<std::unique_ptr<Pass>> passes_;
    };

} // namespace graph

VKPT_END
