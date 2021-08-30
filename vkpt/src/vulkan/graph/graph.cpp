#include <stack>

#include <vkpt/vulkan/graph/executor.h>

#include "vkpt/vulkan/context.h"

VKPT_BEGIN

using namespace graph;

Pass::Pass(Type type, int index)
    : type_(type), index_(index)
{

}

void Pass::set_callback(Callback callback)
{
    callback_ = std::move(callback);
}

void Pass::use(vk::Buffer buffer, vk::PipelineStageFlags stages)
{
    assert(buffer_usages_.find(buffer) == buffer_usages_.end());
    buffer_usages_.insert({ buffer, { stages } });
}

void Pass::use(
    vk::Image                        image,
    vk::PipelineStageFlags           stages,
    vk::ImageLayout                  layout,
    const vk::ImageSubresourceRange &subrsc)
{
    assert(image_usages_.find({ image, subrsc }) == image_usages_.end());
    image_usages_.insert({ { image, subrsc }, { stages, layout } });
}

void Pass::wait(
    vk::ArrayProxy<const vk::Semaphore>          semaphores,
    vk::ArrayProxy<const vk::PipelineStageFlags> stages)
{
    std::copy(
        semaphores.begin(), semaphores.end(),
        std::back_inserter(wait_semaphores_));
    std::copy(
        stages.begin(), stages.end(),
        std::back_inserter(wait_stages_));
}

void Pass::signal(vk::ArrayProxy<const vk::Semaphore> semaphores)
{
    std::copy(
        semaphores.begin(), semaphores.end(),
        std::back_inserter(signal_semaphores_));
}

void Pass::signal(vk::Fence fence)
{
    signal_fence_ = fence;
}

void Pass::forceSubmit()
{
    force_submit_ = true;
}

Pass::Type Pass::getType() const
{
    return type_;
}

void Pass::_addHead(Pass *head, bool cut_edge)
{
    heads_.push_back({ head, cut_edge });
}

void Pass::_addTail(Pass *tail, bool cut_edge)
{
    tails_.push_back({ tail, cut_edge });
}

uint32_t Pass::_getHeadCount() const
{
    return static_cast<uint32_t>(heads_.size());
}

uint32_t Pass::_getTailCount() const
{
    return static_cast<uint32_t>(tails_.size());
}

Pass *RenderGraph::getPrePass()
{
    return &pre_pass_;
}

Pass *RenderGraph::getPostPass()
{
    return &post_pass_;
}

Pass *RenderGraph::addGraphicsPass()
{
    const int index = static_cast<int>(passes_.size());
    passes_.push_back(std::make_unique<Pass>(Pass::Type::Graphics, index));
    return passes_.back().get();
}

Pass *RenderGraph::addComputePass()
{
    const int index = static_cast<int>(passes_.size());
    passes_.push_back(std::make_unique<Pass>(Pass::Type::Compute, index));
    return passes_.back().get();
}

Pass *RenderGraph::addTransferPass()
{
    const int index = static_cast<int>(passes_.size());
    passes_.push_back(std::make_unique<Pass>(Pass::Type::Transfer, index));
    return passes_.back().get();
}

void RenderGraph::addDependency(Pass *head, Pass *tail, bool cut_edge)
{
    head->_addTail(tail, cut_edge);
    tail->_addHead(head, cut_edge);
}

void RenderGraph::clear()
{
    pre_pass_  = Pass(Pass::Type::Graphics, -1);
    post_pass_ = Pass(Pass::Type::Graphics, -1);
    passes_.clear();
}

void RenderGraph::execute(Context &context, ThreadPool &threads)
{
    std::vector<TempPass> temp_passes(passes_.size());
    buildInnerSectionDependencies(temp_passes);

    auto temp_sections = divideSections(temp_passes);
    for(auto &s : temp_sections)
        topologySortPassesInSection(*s);

    buildInterSectionDependeicies(temp_passes, temp_sections);
    topologySortSections(temp_sections);

    ExecutableRenderGraph exec_graph;
    generateExecutableGraphSkeleton(context, temp_sections, exec_graph);

    /*fillExecutableGraphFences(context, exec_graph);

    fillExecutableGraphSemaphores(context, exec_graph);

    fillExecutableGraphBarriers(context, exec_graph);*/
}

void RenderGraph::buildInnerSectionDependencies(
    std::vector<TempPass> &temp_passes)
{
    for(auto &pass : passes_)
    {
        temp_passes[pass->index_].should_submit = false;

        if(pass->force_submit_ || pass->signal_fence_)
        {
            temp_passes[pass->index_].should_submit = true;
            continue;
        }

        for(auto tail : pass->tails_)
        {
            if(tail.pass->getType() != pass->getType())
            {
                temp_passes[pass->index_].should_submit = true;
                break;
            }
        }
    }

    for(auto &pass : passes_)
    {
        pass->in_section_heads_.clear();
        pass->in_section_tails_.clear();
    }

    for(auto &pass : passes_)
    {
        if(!temp_passes[pass->index_].should_submit)
        {
            for(auto &tail : pass->tails_)
            {
                if(!tail.cut_edge)
                {
                    pass->in_section_tails_.push_back(tail.pass);
                    tail.pass->in_section_heads_.push_back(pass.get());
                }

            }
        }
    }
}

void RenderGraph::buildInterSectionDependeicies(
    std::vector<TempPass>                     &passes,
    std::vector<std::unique_ptr<TempSection>> &sections)
{
    for(auto &section : sections)
    {
        for(auto pass : section->passes)
        {
            auto pass_section = passes[pass->index_].section;

            for(auto tail : pass->tails_)
            {
                auto tail_section = passes[tail.pass->index_].section;
                if(tail_section == pass_section)
                    continue;

                pass_section->tails.push_back(tail_section);
                tail_section->heads.push_back(pass_section);
            }
        }
    }
}

std::vector<std::unique_ptr<RenderGraph::TempSection>>
    RenderGraph::divideSections(std::vector<TempPass> &temp_passes)
{
    std::stack<Pass *> undivided_passes, dfs_stack;
    for(auto &p : passes_)
    {
        p->compile_flag_ = 0;
        undivided_passes.push(p.get());
    }

    std::vector<std::unique_ptr<TempSection>> sections;
    while(!undivided_passes.empty())
    {
        auto entry = undivided_passes.top();
        undivided_passes.pop();
        if(entry->compile_flag_)
            continue;
        
        sections.emplace_back();

        assert(dfs_stack.empty());
        dfs_stack.push(entry);

        while(!dfs_stack.empty())
        {
            auto p = dfs_stack.top();
            dfs_stack.pop();
            if(p->compile_flag_)
                continue;

            p->compile_flag_ = 1;
            temp_passes[p->index_].section = sections.back().get();
            sections.back()->passes.push_back(p);

            for(auto head : p->in_section_heads_)
                dfs_stack.push(head);

            for(auto tail : p->in_section_tails_)
                dfs_stack.push(tail);
        }
    }

    return sections;
}

void RenderGraph::topologySortPassesInSection(TempSection &section)
{
    std::queue<Pass *> next_passes;
    for(auto p : section.passes)
    {
        p->compile_flag_ = static_cast<int>(p->in_section_heads_.size());
        if(!p->compile_flag_)
            next_passes.push(p);
    }

    std::vector<Pass *> sorted_passes;
    sorted_passes.reserve(section.passes.size());
    while(!next_passes.empty())
    {
        auto p = next_passes.front();
        next_passes.pop();

        sorted_passes.push_back(p);
        for(auto tail : p->in_section_tails_)
        {
            if(!--tail->compile_flag_)
                next_passes.push(tail);
        }
    }

    assert(sorted_passes.size() == section.passes.size());
    section.passes.swap(sorted_passes);
}

void RenderGraph::topologySortSections(
    std::vector<std::unique_ptr<TempSection>> &sections)
{
    std::vector<TempSection*> sorted_sections;
    sorted_sections.reserve(sections.size());

    std::queue<TempSection *> sort_queue;
    for(auto &s : sections)
    {
        s->unaccessed_head_count = static_cast<uint32_t>(s->heads.size());
        if(!s->unaccessed_head_count)
            sort_queue.push(s.get());
    }

    while(!sort_queue.empty())
    {
        auto section = sort_queue.front();
        sort_queue.pop();

        sorted_sections.push_back(section);

        for(auto tail : section->tails)
        {
            if(!--tail->unaccessed_head_count)
                sort_queue.push(tail);
        }
    }

    assert(sorted_sections.size() == sections.size());
    for(size_t i = 0; i < sections.size(); ++i)
    {
        (void)sections[i].release();
        sections[i].reset(sorted_sections[i]);
    }
}

void RenderGraph::findCommonBufferUsages(
    Pass                             *head,
    Pass                             *tail,
    std::vector<CompleteBufferUsage> &common_buffer_usages)
{
    auto lit  = head->buffer_usages_.begin();
    auto lend = head->buffer_usages_.end();

    auto rit  = tail->buffer_usages_.begin();
    auto rend = tail->buffer_usages_.end();

    while(lit != lend && rit != rend)
    {
        if(lit->first == rit->first)
        {
            common_buffer_usages.push_back(
                { lit->first, lit->second.stages, rit->second.stages });
        }
        else if(lit->first < rit->first)
            ++lit;
        else
            ++rit;
    }
}

void RenderGraph::findCommonImageUsages(
    Pass                             *head,
    Pass                             *tail,
    std::vector<CompleteImageUsage>  &common_image_usages)
{
    auto lit  = head->image_usages_.begin();
    auto lend = head->image_usages_.end();
    
    auto rit  = tail->image_usages_.begin();
    auto rend = tail->image_usages_.end();
    
    while(lit != lend && rit != rend)
    {
        if(lit->first == rit->first)
        {
            common_image_usages.push_back(
                {
                    lit->first.image, lit->first.subrsc,
                    lit->second.stages, lit->second.layout,
                    rit->second.stages, rit->second.layout
                });
        }
        else if(lit->first < rit->first)
            ++lit;
        else
            ++rit;
    }
}

void RenderGraph::generateExecutableGraphSkeleton(
    Context                                   &context,
    std::vector<std::unique_ptr<TempSection>> &temp_sections,
    ExecutableRenderGraph                     &graph)
{
    auto get_queue = [&context](Queue::Type type)
    {
        assert(type != Queue::Type::Present);
        switch(type)
        {
        case Queue::Type::Graphics:
            return context.getGraphicsQueue();
        case Queue::Type::Compute:
            return context.getComputeQueue();
        case Queue::Type::Transfer:
            return context.getTransferQueue();
        default:
            agz::misc::unreachable();
        }
    };

    ExecutableRenderGraph exec_graph;
    exec_graph.sections.resize(temp_sections.size());

    for(size_t i = 0; i < temp_sections.size(); ++i)
    {
        auto &temp_section = *temp_sections[i];
        auto &exec_section = exec_graph.sections[i];

        exec_section.passes.resize(temp_section.passes.size());
        for(size_t j = 0; j < temp_section.passes.size(); ++i)
        {
            exec_section.passes[i].callback =
                &temp_section.passes[i]->callback_;
        }

        exec_section.queue = get_queue(temp_section.passes.front()->getType());
    }
}

void RenderGraph::fillExecutableGraphFences(
    Context               &context,
    ExecutableRenderGraph &graph)
{

}

VKPT_END
