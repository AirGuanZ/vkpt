#include <ranges>

#include <vkpt/graph/compiler.h>
#include <vkpt/graph/group_barrier_generator.h>
#include <vkpt/graph/group_barrier_optimizer.h>
#include <vkpt/graph/semaphore_signal_handler.h>
#include <vkpt/graph/semaphore_wait_handler.h>

VKPT_GRAPH_BEGIN

Compiler::Compiler()
    : compile_passes_(&memory_),
      sorted_compile_passes_(&memory_),
      compile_groups_(&memory_),
      closure_(nullptr),
      generated_pre_passes_(&memory_),
      generated_post_passes_(&memory_),
      resource_records_(memory_),
      buffer_final_states_(&memory_),
      image_final_states_(&memory_)
{

}

ExecutableGraph Compiler::compile(
    SemaphoreAllocator &semaphore_allocator,
    const Graph        &graph)
{
    initializeCompilePasses(graph);
    topologySortCompilePasses();
    buildTransitiveClosure();

    collectResourceUsages();

    {
        GroupBarrierOptimizer optimizer;
        optimizer.optimize(resource_records_);
    }

    {
        auto create_pre_pass = [&]
        {
            auto pass = arena_.create<CompilePass>(memory_);
            generated_pre_passes_.push_back(pass);
            return pass;
        };

        SemaphoreWaitHandler semaphore_wait_handler(
            memory_, this, closure_, create_pre_pass);

        semaphore_wait_handler.collectWaitingSemaphores(graph);
        semaphore_wait_handler.processWaitingSemaphores(resource_records_);
    }

    {
        auto create_post_pass = [&]
        {
            auto pass = arena_.create<CompilePass>(memory_);
            generated_post_passes_.push_back(pass);
            return pass;
        };

        SemaphoreSignalHandler semaphore_signal_handler(
            memory_, closure_,
            buffer_final_states_, image_final_states_,
            create_post_pass);

        semaphore_signal_handler.collectSignalingSemaphores(graph);
        semaphore_signal_handler.processSignalingSemaphores(
            resource_records_, graph);
    }

    for(auto &[buffer, _] : resource_records_.getBuffers())
        processUnwaitedFirstUsage(buffer);

    for(auto &[image_subrsc, _] : resource_records_.getImages())
        processUnwaitedFirstUsage(image_subrsc);

    for(auto &[buffer, _] : resource_records_.getBuffers())
        processUnsignaledFinalState(buffer);

    for(auto &[image_subrsc, _] : resource_records_.getImages())
        processUnsignaledFinalState(image_subrsc);

    mergeNeighboringReadOnlyUsages();

    mergeGeneratedPreAndPostPasses();

    {
        generateGroupFlags();
        auto unsorted_groups = generateGroups();
        fillInterGroupArcs(unsorted_groups);
        topologySortGroups(unsorted_groups);
    }

    for(auto group : compile_groups_)
        topologySortPassesInGroup(group);

    fillInterGroupSemaphores(semaphore_allocator);

    for(auto &record : std::views::values(resource_records_.getBuffers()))
        buildPassToUsage(record);

    for(auto &record : std::views::values(resource_records_.getImages()))
        buildPassToUsage(record);

    {
        GroupBarrierGenerator group_barrier_generator(memory_, resource_records_);
        for(auto group : compile_groups_)
            group_barrier_generator.fillBarriers(group);
    }

    {
        GroupBarrierOptimizer group_barrier_optimizer;
        for(auto group : compile_groups_)
            group_barrier_optimizer.optimize(group);
    }

    ExecutableGraph result(memory_);

    result.groups.resize(compile_groups_.size(), ExecutableGroup(memory_));
    for(size_t i = 0; i < compile_groups_.size(); ++i)
        fillExecutableGroup(*compile_groups_[i], result.groups[i]);

    result.buffer_final_states = std::move(buffer_final_states_);
    result.image_final_states  = std::move(image_final_states_);

    return result;
}

void Compiler::setMessenger(std::function<void(const std::string &)> func)
{
    if(func)
        msg_ = func;
    else
        msg_ = [](const std::string &) {};
}

void Compiler::initializeCompilePasses(const Graph &graph)
{
    Vector<CompilePass *> raw_to_compile(&memory_);
    raw_to_compile.reserve(graph.passes_.size());

    // create compile passes

    for(auto raw_pass : graph.passes_)
    {
        assert(raw_pass->index_ == static_cast<int>(raw_to_compile.size()));

        auto compile_pass = arena_.create<CompilePass>(memory_);

        compile_pass->raw_pass = raw_pass;
        compile_pass->queue    = raw_pass->getPassQueue();
        if(!compile_pass->queue)
            fatal("pass {}'s queue is nil", raw_pass->getPassName());

        compile_passes_.insert(compile_pass);
        raw_to_compile.push_back(compile_pass);

    }

    // copy heads/tails

    for(auto compile_pass : compile_passes_)
    {
        for(auto tail : compile_pass->raw_pass->tails_)
        {
            auto compile_tail = raw_to_compile[tail->index_];
            compile_pass->tails.insert(compile_tail);
            compile_tail->heads.insert(compile_pass);
        }
    }
}

void Compiler::topologySortCompilePasses()
{
    PmrQueue<CompilePass *> next_passes(&memory_);

    for(auto pass : compile_passes_)
    {
        pass->unprocessed_head_count =
            static_cast<uint32_t>(pass->heads.size());
        if(!pass->unprocessed_head_count)
            next_passes.push(pass);
    }

    sorted_compile_passes_.clear();
    sorted_compile_passes_.reserve(compile_passes_.size());

    while(!next_passes.empty())
    {
        auto pass = next_passes.front();
        next_passes.pop();

        assert(!pass->unprocessed_head_count);
        sorted_compile_passes_.push_back(pass);

        for(auto tail : pass->tails)
        {
            if(!--tail->unprocessed_head_count)
                next_passes.push(tail);
        }
    }

    assert(sorted_compile_passes_.size() == compile_passes_.size());
    for(size_t i = 0; i < sorted_compile_passes_.size(); ++i)
        sorted_compile_passes_[i]->sorted_index = static_cast<int>(i);
}

void Compiler::buildTransitiveClosure()
{
    closure_ = arena_.create<DAGTransitiveClosure>(
        static_cast<int>(compile_passes_.size()), memory_);

    for(auto pass : compile_passes_)
    {
        for(auto tail : pass->tails)
            closure_->addArc(pass->sorted_index, tail->sorted_index);
    }

    closure_->computeClosure();
}

void Compiler::collectResourceUsages()
{
    resource_records_.build(sorted_compile_passes_);

#ifdef VKPT_DEBUG

    for(auto &[buffer, record] : resource_records_.getBuffers())
    {
        for(auto it = std::next(record.usages.begin());
            it != record.usages.end(); ++it)
        {
            auto prev = std::prev(it);
            if(!closure_->isReachable(
                prev->pass->sorted_index, it->pass->sorted_index))
            {
                fatal(
                    "users of buffer {} are not ordered", buffer.getName());
            }
        }
    }

    for(auto &[image_subrsc, record] : resource_records_.getImages())
    {
        for(auto it = std::next(record.usages.begin());
            it != record.usages.end(); ++it)
        {
            auto prev = std::prev(it);
            if(!closure_->isReachable(
                prev->pass->sorted_index, it->pass->sorted_index))
            {
                fatal(
                    "users of image {} are not ordered",
                    image_subrsc.image.getName());
            }
        }
    }

#endif
}

template<typename Rsc>
void Compiler::processUnwaitedFirstUsage(const Rsc &resource)
{
    constexpr bool is_buffer = std::is_same_v<Rsc, Buffer>;

    auto &record = resource_records_.getRecord(resource);
    if(record.has_wait_semaphore)
        return;

    assert(!record.usages.empty());
    auto &first_usage = record.usages.front();
    CompilePass *first_pass = first_usage.pass;

    const ResourceState &state = resource.getState();
    state.match(
        [&](const FreeState &s)
    {
        if constexpr(!is_buffer)
        {
            if(s.layout != first_usage.layout)
            {
                first_pass->pre_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = vk::PipelineStageFlagBits2KHR::eBottomOfPipe,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .oldLayout           = s.layout,
                        .newLayout           = first_usage.layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image               = resource.get(),
                        .subresourceRange    = subrscToRange(resource.subrsc)
                    });
            }
        }
    },
        [&](const UsingState &s)
    {
        if(s.queue == first_usage.pass->queue)
        {
            if constexpr(is_buffer)
            {
                first_pass->pre_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = s.stages,
                        .srcAccessMask       = s.access,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer              = resource.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });
            }
            else
            {
                first_pass->pre_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = s.stages,
                        .srcAccessMask       = s.access,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .oldLayout           = s.layout,
                        .newLayout           = first_usage.layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image               = resource.get(),
                        .subresourceRange    = subrscToRange(resource.subrsc)
                    });
            }
        }
        else
        {
            auto dummy_pass = arena_.create<CompilePass>(memory_);
            dummy_pass->queue = s.queue;
            generated_pre_passes_.push_back(dummy_pass);

            dummy_pass->tails.insert(first_pass);
            first_pass->heads.insert(dummy_pass);

            if constexpr(is_buffer)
            {
                record.usages.push_front(CompileBufferUsage{
                    .pass   = dummy_pass,
                    .stages = s.stages,
                    .access = s.access
                });
                dummy_pass->generated_buffer_usages_[resource] =
                    record.usages.front();
            }
            else
            {
                record.usages.push_front(CompileImageUsage{
                    .pass        = dummy_pass,
                    .stages      = s.stages,
                    .access      = s.access,
                    .layout      = s.layout,
                    .exit_layout = s.layout
                });
                dummy_pass->generated_image_usages_[resource] =
                    record.usages.front();
            }
        }
    },
        [&](const ReleasedState &s)
    {
        fatal(
            "{} {} is in released state but doesn't have a waiting semaphore",
            is_buffer ? "buffer" : "image", resource.getName());
    });
}

template<typename Rsc>
void Compiler::processUnsignaledFinalState(const Rsc &rsc)
{
    constexpr bool is_buffer = std::is_same_v<Rsc, Buffer>;

    auto &record = resource_records_.getRecord(rsc);
    if(record.has_signal_semaphore)
        return;

    assert(!record.usages.empty());
    auto &last_usage = record.usages.back();

    if constexpr(is_buffer)
    {
        buffer_final_states_[rsc] = UsingState{
            .queue  = last_usage.pass->queue,
            .stages = last_usage.stages,
            .access = last_usage.access
        };
    }
    else
    {
        image_final_states_[rsc] = UsingState{
            .queue  = last_usage.pass->queue,
            .stages = last_usage.stages,
            .access = last_usage.access,
            .layout = last_usage.exit_layout
        };
    }
}

void Compiler::mergeNeighboringReadOnlyUsages()
{
    // TODO
}

void Compiler::mergeGeneratedPreAndPostPasses()
{
    std::copy(
        generated_pre_passes_.begin(),
        generated_pre_passes_.end(),
        agz::misc::direct_inserter(compile_passes_));

    std::copy(
        generated_post_passes_.begin(),
        generated_post_passes_.end(),
        agz::misc::direct_inserter(compile_passes_));

    Vector<CompilePass *> new_sorted_compile_passes(&memory_);
    new_sorted_compile_passes.reserve(
        generated_pre_passes_.size() +
        sorted_compile_passes_.size() +
        generated_post_passes_.size());

    std::copy(
        generated_pre_passes_.begin(),
        generated_pre_passes_.end(),
        std::back_inserter(new_sorted_compile_passes));

    std::copy(
        sorted_compile_passes_.begin(),
        sorted_compile_passes_.end(),
        std::back_inserter(new_sorted_compile_passes));

    std::copy(
        generated_post_passes_.begin(),
        generated_post_passes_.end(),
        std::back_inserter(new_sorted_compile_passes));

    sorted_compile_passes_ = std::move(new_sorted_compile_passes);
    for(size_t i = 0; i < sorted_compile_passes_.size(); ++i)
        sorted_compile_passes_[i]->sorted_index = static_cast<int>(i);
}

template<bool Reverse>
void Compiler::addGroupFlag(CompilePass *entry, size_t bit_index)
{
    PmrQueue<CompilePass *> next_passes(&memory_);
    next_passes.push(entry);

    while(!next_passes.empty())
    {
        auto pass = next_passes.front();
        next_passes.pop();
        if(pass->group_flags.test(bit_index))
            continue;

        auto &succs = Reverse ? pass->heads : pass->tails;
        for(auto succ : succs)
        {
            if(succ->queue != pass->queue)
                continue;
            if(succ->group_flags == pass->group_flags)
                next_passes.push(succ);
        }

        pass->group_flags.set(bit_index, true);
    }
}

void Compiler::generateGroupFlags()
{
    Set<CompilePass *> entry_passes(&memory_), reverse_entry_passes(&memory_);

    for(auto pass : sorted_compile_passes_)
    {
        bool reverse = !pass->signal_semaphores.empty();
        for(auto tail : pass->tails)
        {
            if(pass->queue != tail->queue)
            {
                entry_passes.insert(tail);
                reverse = true;
            }
        }
        if(!pass->wait_semaphores.empty())
            entry_passes.insert(pass);
        if(reverse)
            reverse_entry_passes.insert(pass);
    }

    size_t bit_index = 0;
    for(auto p : entry_passes)
        addGroupFlag<false>(p, bit_index++);
    for(auto p : reverse_entry_passes)
        addGroupFlag<true>(p, bit_index++);
}

List<CompileGroup *> Compiler::generateGroups()
{
    List<CompileGroup *> result(&memory_);
    PmrQueue<CompilePass *> next_passes(&memory_);

    for(auto entry : sorted_compile_passes_)
    {
        if(entry->group)
            continue;

        auto new_group = arena_.create<CompileGroup>(memory_);
        new_group->queue = entry->queue;
        result.push_back(new_group);

        assert(next_passes.empty());
        next_passes.push(entry);
        while(!next_passes.empty())
        {
            auto pass = next_passes.front();
            next_passes.pop();
            if(pass->group)
                continue;

            pass->group = new_group;
            new_group->passes.push_back(pass);

            for(auto head : pass->heads)
            {
                if(!head->group &&
                    head->queue == pass->queue &&
                    head->group_flags == pass->group_flags)
                    next_passes.push(head);
            }

            for(auto tail : pass->tails)
            {
                if(!tail->group &&
                    tail->queue == pass->queue &&
                    tail->group_flags == pass->group_flags)
                    next_passes.push(tail);
            }
        }
    }

    return result;
}

void Compiler::fillInterGroupArcs(const List<CompileGroup *> &groups)
{
    for(auto group : groups)
    {
        for(auto pass : group->passes)
        {
            for(auto tail : pass->tails)
            {
                if(tail->queue == pass->queue)
                    continue;

                if(tail->group->heads.contains(pass->group))
                    continue;
                
                pass->group->need_tail_semaphore = true;
                pass->group->tails.insert(tail->group);
                tail->group->heads.insert({ pass->group, {} });
            }
        }
    }
}

void Compiler::topologySortGroups(const List<CompileGroup *> &groups)
{
    assert(compile_groups_.empty());
    compile_groups_.reserve(groups.size());

    PmrQueue<CompileGroup *> next_groups(&memory_);
    for(auto group : groups)
    {
        group->unprocessed_head_count = static_cast<int>(group->heads.size());
        if(!group->unprocessed_head_count)
            next_groups.push(group);
    }

    while(!next_groups.empty())
    {
        auto group = next_groups.front();
        next_groups.pop();
        assert(!group->unprocessed_head_count);

        compile_groups_.push_back(group);
        for(auto &tail : group->tails)
        {
            if(!--tail->unprocessed_head_count)
                next_groups.push(tail);
        }
    }

    assert(compile_groups_.size() == groups.size());
}

void Compiler::topologySortPassesInGroup(CompileGroup *group)
{
    PmrQueue<CompilePass *> next_passes(&memory_);
    for(auto pass : group->passes)
    {
        int head_count = 0;
        for(auto head : pass->heads)
            head_count += head->group == group;
        pass->unprocessed_head_count = head_count;

        if(!head_count)
            next_passes.push(pass);
    }

    Vector<CompilePass *> sorted_passes(&memory_);
    sorted_passes.reserve(group->passes.size());

    while(!next_passes.empty())
    {
        auto pass = next_passes.front();
        next_passes.pop();

        assert(!pass->unprocessed_head_count);
        sorted_passes.push_back(pass);

        for(auto tail : pass->tails)
        {
            if(tail->group == group && !--tail->unprocessed_head_count)
                next_passes.push(tail);
        }
    }

    for(size_t i = 0; i < sorted_passes.size(); ++i)
        sorted_passes[i]->sorted_index_in_group = static_cast<int>(i);

    assert(sorted_passes.size() == group->passes.size());
    group->passes.swap(sorted_passes);
}

void Compiler::fillInterGroupSemaphores(SemaphoreAllocator &allocator)
{
    for(auto group : compile_groups_)
    {
        if(group->need_tail_semaphore)
        {
            group->tail_semaphore = allocator.newTimelineSemaphore();
            group->tail_semaphore.nextSignalValue();
        }
    }
}

void Compiler::buildPassToUsage(CompileBuffer &record)
{
    assert(record.pass_to_usage.empty());
    for(auto it = record.usages.begin(); it != record.usages.end(); ++it)
        record.pass_to_usage.insert({ it->pass, it });
}

void Compiler::buildPassToUsage(CompileImage &record)
{
    assert(record.pass_to_usage.empty());
    for(auto it = record.usages.begin(); it != record.usages.end(); ++it)
        record.pass_to_usage.insert({ it->pass, it });
}

void Compiler::fillExecutableGroup(
    const CompileGroup &group, ExecutableGroup &output)
{
    output.queue = group.queue;
    output.passes.reserve(group.passes.size());

    for(auto pass : group.passes)
    {
        output.passes.emplace_back(memory_);
        auto &output_pass = output.passes.back();

        output_pass.pass = pass->raw_pass;

        output_pass.pre_buffer_barriers.reserve(
            pass->pre_buffer_barriers.size() +
            pass->pre_ext_buffer_barriers.size());
        std::ranges::copy(
            std::views::values(pass->pre_buffer_barriers),
            std::back_inserter(output_pass.pre_buffer_barriers));
        std::copy(
            pass->pre_ext_buffer_barriers.begin(),
            pass->pre_ext_buffer_barriers.end(),
            std::back_inserter(output_pass.pre_buffer_barriers));

        output_pass.pre_image_barriers.reserve(
            pass->pre_image_barriers.size() +
            pass->pre_ext_image_barriers.size());
        std::ranges::copy(
            std::views::values(pass->pre_image_barriers),
            std::back_inserter(output_pass.pre_image_barriers));
        std::copy(
            pass->pre_ext_image_barriers.begin(),
            pass->pre_ext_image_barriers.end(),
            std::back_inserter(output_pass.pre_image_barriers));

        output_pass.post_buffer_barriers.reserve(
            pass->post_ext_buffer_barriers.size());
        std::copy(
            pass->post_ext_buffer_barriers.begin(),
            pass->post_ext_buffer_barriers.end(),
            std::back_inserter(output_pass.post_buffer_barriers));

        output_pass.post_image_barriers.reserve(
            pass->post_ext_image_barriers.size());
        std::copy(
            pass->post_ext_image_barriers.begin(),
            pass->post_ext_image_barriers.end(),
            std::back_inserter(output_pass.post_image_barriers));
        
        for(auto &[_, submit] : pass->wait_semaphores)
            output.wait_semaphores.push_back(submit);

        for(auto &[_, submit] : pass->signal_semaphores)
        {
            output.signal_semaphores.push_back(vk::SemaphoreSubmitInfoKHR{
                .semaphore = submit.semaphore,
                .value     = submit.value,
                .stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands
            });
        }

        if(pass->raw_pass)
        {
            std::copy(
                pass->raw_pass->_getFences().begin(),
                pass->raw_pass->_getFences().end(),
                std::back_inserter(output.signal_fences));
        }
    }

    for(auto &[head, stages] : group.heads)
    {
        if(head->queue == group.queue)
            continue;
        assert(head->tail_semaphore);
        output.wait_semaphores.push_back(vk::SemaphoreSubmitInfoKHR{
            .semaphore = head->tail_semaphore,
            .value     = head->tail_semaphore.getLastSignalValue(),
            .stageMask = stages
        });
    }

    if(group.tail_semaphore)
    {
        output.signal_semaphores.push_back(vk::SemaphoreSubmitInfoKHR{
            .semaphore = group.tail_semaphore,
            .value     = group.tail_semaphore.getLastSignalValue(),
            .stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands
        });
    }
}

VKPT_GRAPH_END
