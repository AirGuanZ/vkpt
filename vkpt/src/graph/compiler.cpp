#include <queue>

#include <vkpt/graph/compiler.h>

VKPT_RENDER_GRAPH_BEGIN

Compiler::Compiler()
    : object_arena_(memory_arena_),
      compile_passes_(&memory_arena_),
      buffer_states_(&memory_arena_),
      image_states_(&memory_arena_)
{
    
}

ExecutableGraph Compiler::compile(
    std::pmr::memory_resource &memory,
    SemaphoreAllocator        &semaphore_allocator,
    Graph                     &graph)
{
    /* 1. 构建pass间的完整依赖关系
       2. 在基本拓扑序的约束下对pass进行排序，排序以“同queue中pass最近依赖最少”为原则
       3. 如何确定semaphore？
       4. 按拓扑序遍历所有pass，构建每个resource的历史踪迹 
       5. 对每个pass中的每个资源使用
            a. 若上一个使用者在同一个queue，那么：
                (1) 对buffer，看看和上一个pass之间有没有合适的global memory barrier，没有的话就插入一个
                (2) 对image，直接插个barrier
            b. 上一个使用者在另一个queue，那么：
                (1) 找到最近的、通往该queue的semaphore
                (2) 在上一个pass末尾插入一个release barrier
                (3) 在这个pass开头插入一个acquire barrier
                (4) 给semaphore的signal stages加入上个pass的stages
                (5) 给semaphore的wait stages加入这个pass的stages
    */

    /*
        需要把pass分成多组，组内pass都属于同一个queue，且从任意一个组出发均不能到达自己

        为此，在原DAG的基础上构建grouping graph，其每个连通分量被划分到同一个组中

        将queue q的pass A记作Aq

        我们在原图基础上删除一些边来得到grouping graph
        0. 令G = 原图
        1. 删除G中所有的Ax -> By
        2. 若原图中存在Ax -> By，记G中所有从B可达的pass集合为R[B]，删除R[B]与V[G]/R[B]间的所有边
        3. 若原图中存在Ax -> By，记G中所有从A反向可达的pass集合为T[A]，删除T[A]与V[G]/T[A]间的所有边
    */

    compile_passes_.resize(graph.passes_.size());
    auto pass_it = graph.passes_.begin();
    for(auto &compile_pass : compile_passes_)
    {
        compile_pass = object_arena_.create<CompilePass>(memory_arena_);
        compile_pass->raw_pass = *pass_it++;
    }

    generateGroups();

    fillGroupArcs();

    generateGroupFlags();

    sortGroups();

    sortPasses();

    fillInterGroupSemaphores(semaphore_allocator);

    fillResourceStateTracers();

    generateBarriers();

    ExecutableGraph result;
    result.groups = Vector<ExecutableGroup>(compile_groups_.size(), &memory);
    for(size_t i = 0; i < compile_groups_.size(); ++i)
        fillExecutableGroup(memory, result.groups[i], *compile_groups_[i]);

    return result;
}

ExecutableGraph Compiler::compile(
    SemaphoreAllocator &semaphore_allocator,
    Graph              &graph)
{
    return compile(memory_arena_, semaphore_allocator, graph);
}

Compiler::CompilePass::CompilePass(std::pmr::memory_resource &memory)
    : pre_memory_barriers(&memory),
      pre_buffer_barriers(&memory),
      pre_image_barriers(&memory),
      post_memory_barriers(&memory),
      post_buffer_barriers(&memory),
      post_image_barriers(&memory)
{
    
}

Compiler::CompileGroup::CompileGroup(std::pmr::memory_resource &memory)
    : passes(&memory), heads(&memory), tails(&memory)
{
    
}

template<bool Reverse>
void Compiler::newGroupFlag(CompilePass *compile_entry, size_t bit_index)
{
    PmrQueue<CompilePass *> next_passes(&memory_arena_);
    next_passes.push(compile_entry);

    while(!next_passes.empty())
    {
        auto pass = next_passes.front();
        next_passes.pop();
        if(pass->group_flags.test(bit_index))
            continue;

        auto raw_pass = pass->raw_pass;
        auto &succs = Reverse ? raw_pass->heads_ : raw_pass->tails_;
        for(auto succ : succs)
        {
            if(succ->queue_ != pass->raw_pass->queue_)
                continue;

            auto compile_succ = compile_passes_[succ->index_];
            if(compile_succ->group_flags == pass->group_flags)
                next_passes.push(compile_succ);
        }

        pass->group_flags.set(bit_index, true);
    }
}

void Compiler::generateGroupFlags()
{
    size_t next_flag_index = 0;
    for(auto compile_pass : compile_passes_)
    {
        for(auto tail : compile_pass->raw_pass->tails_)
        {
            auto compile_tail = compile_passes_[tail->index_];
            newGroupFlag<false>(compile_tail, next_flag_index++);
            newGroupFlag<true>(compile_pass, next_flag_index++);
        }

        if(!compile_pass->raw_pass->wait_semaphores_.empty())
            newGroupFlag<true>(compile_pass, next_flag_index++);
    }
}

void Compiler::generateGroups()
{
    std::pmr::unsynchronized_pool_resource pass_queue_pool;
    PmrQueue<CompilePass *> next_passes(&pass_queue_pool);

    for(auto entry : compile_passes_)
    {
        if(entry->group)
            continue;

        assert(next_passes.empty());
        next_passes.push(entry);

        auto new_group = object_arena_.create<CompileGroup>(memory_arena_);
        compile_groups_.push_back(new_group);

        while(!next_passes.empty())
        {
            auto pass = next_passes.front();
            next_passes.pop();
            if(pass->group)
                continue;

            pass->group = new_group;
            new_group->passes.push_back(pass);

            for(auto head : pass->raw_pass->heads_)
            {
                auto compile_head = compile_passes_[head->index_];

                if(!compile_head->group &&
                    head->queue_ == pass->raw_pass->queue_ &&
                    compile_head->group_flags == pass->group_flags)
                    next_passes.push(compile_head);
            }

            for(auto tail : pass->raw_pass->tails_)
            {
                auto compile_tail = compile_passes_[tail->index_];

                if(!compile_tail->group &&
                    tail->queue_ == pass->raw_pass->queue_ &&
                    compile_tail->group_flags == pass->group_flags)
                    next_passes.push(compile_tail);
            }
        }
    }
}

void Compiler::fillGroupArcs()
{
    for(auto group : compile_groups_)
    {
        for(auto pass : group->passes)
        {
            for(auto tail : pass->raw_pass->tails_)
            {
                if(tail->queue_ == pass->raw_pass->queue_)
                    continue;

                auto tail_group = compile_passes_[tail->index_]->group;
                assert(group->tails.contains(tail_group) ==
                       tail_group->heads.contains(group));
                if(group->tails.contains(tail_group))
                    continue;

                auto dep = object_arena_.create<CompileGroup::GroupDependency>();
                group->tails.insert({ tail_group, dep });
                tail_group->heads.insert({ group, dep });
            }
        }
    }
}

void Compiler::sortGroups()
{
    Vector<CompileGroup *> sorted_groups(&memory_arena_);
    sorted_groups.reserve(compile_groups_.size());
    
    PmrQueue<CompileGroup *> next_groups(&memory_arena_);

    for(auto group : compile_groups_)
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

        sorted_groups.push_back(group);
        for(auto &tail : group->tails)
        {
            if(!--tail.first->unprocessed_head_count)
                next_groups.push(tail.first);
        }
    }

    assert(sorted_groups.size() == compile_groups_.size());
    compile_groups_ = std::move(sorted_groups);
}

void Compiler::sortPasses()
{
    PmrQueue<CompilePass *> next_passes(&memory_arena_);

    auto is_in_same_group = [&](CompilePass *pass1, Pass *pass2)
    {
        return pass1->group == compile_passes_[pass2->index_]->group;
    };

    for(auto group : compile_groups_)
    {
        assert(next_passes.empty());
        for(auto pass : group->passes)
        {
            pass->unprocessed_head_count = 0;
            for(auto head : pass->raw_pass->heads_)
            {
                if(is_in_same_group(pass, head))
                    ++pass->unprocessed_head_count;
            }
            if(!pass->unprocessed_head_count)
                next_passes.push(pass);
        }

        Vector<CompilePass *> sorted_passes(&memory_arena_);
        sorted_passes.reserve(group->passes.size());

        while(!next_passes.empty())
        {
            auto pass = next_passes.front();
            next_passes.pop();
            assert(!pass->unprocessed_head_count);

            sorted_passes.push_back(pass);
            for(auto tail : pass->raw_pass->tails_)
            {
                if(is_in_same_group(pass, tail))
                {
                    auto compile_tail = compile_passes_[tail->index_];
                    if(!--compile_tail->unprocessed_head_count)
                        next_passes.push(compile_tail);
                }
            }
        }

        assert(sorted_passes.size() == group->passes.size());
        group->passes.swap(sorted_passes);
    }
}

void Compiler::fillInterGroupSemaphores(SemaphoreAllocator &semaphore_allocator)
{
    for(auto group : compile_groups_)
    {
        for(auto &pair : group->tails)
            pair.second->semaphore = semaphore_allocator.newSemaphore();
    }
}

void Compiler::fillResourceStateTracers()
{
    auto buffer_tracer = [&](vk::Buffer buffer) -> BufferStateTracer&
    {
        return buffer_states_.try_emplace(
            buffer, AGZ_LAZY(BufferStateTracer(memory_arena_))).first->second;
    };

    auto image_tracer = [&](vk::Image image, const vk::ImageSubresource &subrsc)
        -> ImageStateTracer&
    {
        return image_states_.try_emplace(
            { image, subrsc },
            AGZ_LAZY(ImageStateTracer(memory_arena_))).first->second;
    };
    
    for(auto group : compile_groups_)
    {
        for(auto pass : group->passes)
        {
            for(auto &[buffer, usage] : pass->raw_pass->buffer_usages_)
            {
                buffer_tracer(buffer).addUsage(pass->raw_pass, usage);
            }

            for(auto &[image_key, usage] : pass->raw_pass->image_usages_)
            {
                for_each_subrsc(image_key.second, [&](const vk::ImageSubresource &subrsc)
                {
                    auto &tracer = image_tracer(image_key.first.get(), subrsc);
                    tracer.addUsage(pass->raw_pass, usage);
                });
            }
        }
    }
}

Compiler::CompileGroup::GroupDependency *Compiler::findHeadDependency(
    CompileGroup *src, CompileGroup *dst) const
{
    for(auto &pair : src->heads)
    {
        if(pair.first == dst)
            return pair.second;
        if(auto result = findHeadDependency(pair.first, dst))
            return result;
    }
    return nullptr;
}

void Compiler::processBufferDependency(
    Pass                    *pass,
    const Buffer            &buffer,
    const Pass::BufferUsage &usage)
{
    auto &state_tracer = buffer_states_.at(buffer);
    if(state_tracer.isInitialUsage(pass))
        return;

    auto &last_usage = state_tracer.getLastUsage(pass);
    auto last_pass = last_usage.pass;

    auto compile_pass      = compile_passes_[pass->index_];
    auto last_compile_pass = compile_passes_[last_pass->index_];

    if(last_pass->queue_ == pass->queue_)
    {
        // IMPTOVE: remove redundant barrier
        compile_pass->pre_memory_barriers.insert(
            vk::MemoryBarrier2KHR{
                .srcStageMask  = last_usage.stages,
                .srcAccessMask = last_usage.access,
                .dstStageMask  = usage.stages,
                .dstAccessMask = usage.access
            });
    }
    else
    {
        auto dependency = findHeadDependency(
            compile_pass->group, last_compile_pass->group);

        //dependency->signal_stages |= last_usage.stages;
        dependency->wait_stages |= usage.stages;

        const uint32_t last_family = last_pass->queue_->getFamilyIndex();
        const uint32_t this_family = pass->queue_->getFamilyIndex();

        const bool is_different_queue_family = last_family != this_family;
        const bool is_exclusive =
            buffer.getDescription().sharing_mode == vk::SharingMode::eExclusive;

        if(is_different_queue_family && is_exclusive)
        {
            last_compile_pass->post_buffer_barriers.insert(
                vk::BufferMemoryBarrier2KHR{
                    .srcStageMask        = last_usage.stages,
                    .srcAccessMask       = last_usage.access,
                    //.dstStageMask        = usage.stages,
                    //.dstAccessMask       = usage.access,
                    .srcQueueFamilyIndex = last_family,
                    .dstQueueFamilyIndex = this_family,
                    .buffer              = buffer.get(),
                    .offset              = 0,
                    .size                = VK_WHOLE_SIZE
                });
            compile_pass->pre_buffer_barriers.insert(
                vk::BufferMemoryBarrier2KHR{
                    .srcStageMask        = usage.stages,
                    //.srcStageMask        = last_usage.stages,
                    //.srcAccessMask       = last_usage.access,
                    .dstStageMask        = usage.stages,
                    //.dstAccessMask       = usage.access,
                    .srcQueueFamilyIndex = last_family,
                    .dstQueueFamilyIndex = this_family,
                    .buffer              = buffer.get(),
                    .offset              = 0,
                    .size                = VK_WHOLE_SIZE
                });
        }
        else
        {
            compile_pass->pre_memory_barriers.insert(
                vk::MemoryBarrier2KHR{
                    .srcStageMask  = usage.stages,
                    //.srcAccessMask = last_usage.access,
                    .dstStageMask  = usage.stages,
                    .dstAccessMask = usage.access
                });
        }
    }
}

void Compiler::processImageDependency(
    Pass                       *pass,
    const Image                &image,
    const vk::ImageSubresource &subrsc,
    const Pass::ImageUsage     &usage)
{
    auto &state_tracer = image_states_.at({ image.get(), subrsc });
    if(state_tracer.isInitialUsage(pass))
        return;

    auto &last_usage = state_tracer.getLastUsage(pass);
    auto last_pass = last_usage.pass;

    auto compile_pass = compile_passes_[pass->index_];
    auto last_compile_pass = compile_passes_[last_pass->index_];

    if(last_pass->queue_ == pass->queue_)
    {
        compile_pass->pre_image_barriers.insert(
            vk::ImageMemoryBarrier2KHR{
                .srcStageMask        = last_usage.stages,
                .srcAccessMask       = last_usage.access,
                .dstStageMask        = usage.stages,
                .dstAccessMask       = usage.access,
                .oldLayout           = last_usage.layout,
                .newLayout           = usage.layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = image.get(),
                .subresourceRange    = vk::ImageSubresourceRange{
                    .aspectMask     = subrsc.aspectMask,
                    .baseMipLevel   = subrsc.mipLevel,
                    .levelCount     = 1,
                    .baseArrayLayer = subrsc.arrayLayer,
                    .layerCount     = 1
                }
            });
    }
    else
    {
        auto dependency = findHeadDependency(
            compile_pass->group, last_compile_pass->group);

        //dependency->signal_stages |= last_usage.stages;
        dependency->wait_stages |= usage.stages;

        const uint32_t last_family = last_pass->queue_->getFamilyIndex();
        const uint32_t this_family = pass->queue_->getFamilyIndex();

        const bool is_different_queue_family = last_family != this_family;
        const bool is_exclusive =
            image.getDescription().sharing_mode == vk::SharingMode::eExclusive;

        if(is_different_queue_family && is_exclusive)
        {
            last_compile_pass->post_image_barriers.insert(
                vk::ImageMemoryBarrier2KHR{
                    .srcStageMask        = last_usage.stages,
                    .srcAccessMask       = last_usage.access,
                    .oldLayout           = last_usage.layout,
                    .newLayout           = usage.layout,
                    .srcQueueFamilyIndex = last_family,
                    .dstQueueFamilyIndex = this_family,
                    .image               = image.get(),
                    .subresourceRange    = vk::ImageSubresourceRange{
                        .aspectMask     = subrsc.aspectMask,
                        .baseMipLevel   = subrsc.mipLevel,
                        .levelCount     = 1,
                        .baseArrayLayer = subrsc.arrayLayer,
                        .layerCount     = 1
                    }
                });
            
            compile_pass->pre_image_barriers.insert(
                vk::ImageMemoryBarrier2KHR{
                    .dstStageMask        = usage.stages,
                    .dstAccessMask       = usage.access,
                    .oldLayout           = last_usage.layout,
                    .newLayout           = usage.layout,
                    .srcQueueFamilyIndex = last_family,
                    .dstQueueFamilyIndex = this_family,
                    .image               = image.get(),
                    .subresourceRange    = vk::ImageSubresourceRange{
                        .aspectMask     = subrsc.aspectMask,
                        .baseMipLevel   = subrsc.mipLevel,
                        .levelCount     = 1,
                        .baseArrayLayer = subrsc.arrayLayer,
                        .layerCount     = 1
                    }
                });
        }
        else
        {
            compile_pass->pre_image_barriers.insert(
                vk::ImageMemoryBarrier2KHR{
                    .srcStageMask        = usage.stages,
                    .dstStageMask        = usage.stages,
                    .dstAccessMask       = usage.access,
                    .oldLayout           = last_usage.layout,
                    .newLayout           = usage.layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = image.get(),
                    .subresourceRange    = vk::ImageSubresourceRange{
                        .aspectMask     = subrsc.aspectMask,
                        .baseMipLevel   = subrsc.mipLevel,
                        .levelCount     = 1,
                        .baseArrayLayer = subrsc.arrayLayer,
                        .layerCount     = 1
                    }
                });
        }
    }
}

void Compiler::generateBarriers()
{
    for(auto group : compile_groups_)
    {
        for(auto pass : group->passes)
        {
            for(auto &[buffer, usage] : pass->raw_pass->buffer_usages_)
                processBufferDependency(pass->raw_pass, buffer, usage);

            for(auto &[image_key, usage] : pass->raw_pass->image_usages_)
            {
                auto &image = image_key.first;
                auto &range = image_key.second;

                for_each_subrsc(range, [&](const vk::ImageSubresource &subrsc)
                {
                    processImageDependency(
                        pass->raw_pass, image, subrsc, usage);
                });
            }

            std::copy(
                pass->raw_pass->pre_memory_barriers_.begin(),
                pass->raw_pass->pre_memory_barriers_.end(),
                agz::misc::direct_inserter(pass->pre_memory_barriers));

            std::copy(
                pass->raw_pass->pre_buffer_barriers_.begin(),
                pass->raw_pass->pre_buffer_barriers_.end(),
                agz::misc::direct_inserter(pass->pre_buffer_barriers));

            std::copy(
                pass->raw_pass->pre_image_barriers_.begin(),
                pass->raw_pass->pre_image_barriers_.end(),
                agz::misc::direct_inserter(pass->pre_image_barriers));

            std::copy(
                pass->raw_pass->post_memory_barriers_.begin(),
                pass->raw_pass->post_memory_barriers_.end(),
                agz::misc::direct_inserter(pass->post_memory_barriers));

            std::copy(
                pass->raw_pass->post_buffer_barriers_.begin(),
                pass->raw_pass->post_buffer_barriers_.end(),
                agz::misc::direct_inserter(pass->post_buffer_barriers));

            std::copy(
                pass->raw_pass->post_image_barriers_.begin(),
                pass->raw_pass->post_image_barriers_.end(),
                agz::misc::direct_inserter(pass->post_image_barriers));
        }
    }
}

void Compiler::fillExecutableGroup(
    std::pmr::memory_resource &memory,
    ExecutableGroup           &group,
    CompileGroup              &compile)
{
    group.queue = compile.passes.front()->raw_pass->queue_;
    group.passes = Vector<ExecutablePass>(compile.passes.size(), &memory);
    for(size_t i = 0; i < group.passes.size(); ++i)
        fillExecutablePass(memory, group.passes[i], *compile.passes[i]);

    size_t ext_wait_semaphore_count   = 0;
    size_t ext_signal_semaphore_count = 0;
    size_t ext_signal_fence_count     = 0;
    for(auto pass : compile.passes)
    {
        ext_wait_semaphore_count   += pass->raw_pass->wait_semaphores_.size();
        ext_signal_semaphore_count += pass->raw_pass->signal_semaphores_.size();
        ext_signal_fence_count     += !!pass->raw_pass->signal_fence_;
    }

    group.wait_semaphores = Vector<vk::SemaphoreSubmitInfoKHR>(&memory);
    group.wait_semaphores.reserve(compile.heads.size() + ext_wait_semaphore_count);
    for(auto &[_, dependency] : compile.heads)
    {
        group.wait_semaphores.push_back(vk::SemaphoreSubmitInfoKHR{
            .semaphore = dependency->semaphore,
            .stageMask = dependency->wait_stages
        });
    }

    group.signal_semaphores = Vector<vk::SemaphoreSubmitInfoKHR>(&memory);
    group.signal_semaphores.reserve(compile.tails.size() + ext_signal_semaphore_count);
    for(auto &[_, dependency] : compile.tails)
    {
        group.signal_semaphores.push_back(vk::SemaphoreSubmitInfoKHR{
            .semaphore = dependency->semaphore,
            .stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands
        });
    }

    group.signal_fences = Vector<vk::Fence>(&memory);
    group.signal_fences.reserve(ext_signal_fence_count);

    for(auto pass : compile.passes)
    {
        auto raw_pass = pass->raw_pass;
        std::copy(
            raw_pass->wait_semaphores_.begin(),
            raw_pass->wait_semaphores_.end(),
            std::back_inserter(group.wait_semaphores));
        std::copy(
            raw_pass->signal_semaphores_.begin(),
            raw_pass->signal_semaphores_.end(),
            std::back_inserter(group.signal_semaphores));
        if(raw_pass->signal_fence_)
            group.signal_fences.push_back(raw_pass->signal_fence_);
    }
}

void Compiler::fillExecutablePass(
    std::pmr::memory_resource &memory,
    ExecutablePass            &pass,
    CompilePass               &compile)
{
    if(compile.raw_pass->callback_)
        pass.callback = &compile.raw_pass->callback_;
    else
        pass.callback = nullptr;

    pass.pre_memory_barriers = Vector<vk::MemoryBarrier2KHR>(&memory);
    pass.pre_memory_barriers.reserve(compile.pre_memory_barriers.size());
    std::copy(
        compile.pre_memory_barriers.begin(),
        compile.pre_memory_barriers.end(),
        std::back_inserter(pass.pre_memory_barriers));

    pass.pre_buffer_barriers = Vector<vk::BufferMemoryBarrier2KHR>(&memory);
    pass.pre_buffer_barriers.reserve(compile.pre_buffer_barriers.size());
    std::copy(
        compile.pre_buffer_barriers.begin(),
        compile.pre_buffer_barriers.end(),
        std::back_inserter(pass.pre_buffer_barriers));

    pass.pre_image_barriers = Vector<vk::ImageMemoryBarrier2KHR>(&memory);
    pass.pre_image_barriers.reserve(compile.pre_image_barriers.size());
    std::copy(
        compile.pre_image_barriers.begin(),
        compile.pre_image_barriers.end(),
        std::back_inserter(pass.pre_image_barriers));

    pass.post_memory_barriers = Vector<vk::MemoryBarrier2KHR>(&memory);
    pass.post_memory_barriers.reserve(
        compile.raw_pass->post_memory_barriers_.size());
    std::copy(
        compile.raw_pass->post_memory_barriers_.begin(),
        compile.raw_pass->post_memory_barriers_.end(),
        std::back_inserter(pass.post_memory_barriers));

    pass.post_buffer_barriers = Vector<vk::BufferMemoryBarrier2KHR>(&memory);
    pass.post_buffer_barriers.reserve(compile.post_buffer_barriers.size());
    std::copy(
        compile.post_buffer_barriers.begin(),
        compile.post_buffer_barriers.end(),
        std::back_inserter(pass.post_buffer_barriers));

    pass.post_image_barriers = Vector<vk::ImageMemoryBarrier2KHR>(&memory);
    pass.post_image_barriers.reserve(compile.post_image_barriers.size());
    std::copy(
        compile.post_image_barriers.begin(),
        compile.post_image_barriers.end(),
        std::back_inserter(pass.post_image_barriers));
}

VKPT_RENDER_GRAPH_END
