#include <vkpt/graph/finalizer.h>

VKPT_RENDER_GRAPH_BEGIN

Finalizer::Finalizer()
    : buffers_(&memory_), images_(&memory_)
{
    
}

void Finalizer::finalize(Graph &graph)
{
    sorted_passes_ = topologySortPasses(graph);

    collectResourceUsers();

    removed_ = Vector<bool>(graph.passes_.size(), false, &memory_);

    removeUnnecessaryPrepasses(graph);
    removeUnnecessaryPostpasses(graph);

    List<Pass *> new_passes(&graph.memory_arena_);
    for(auto pass : graph.passes_)
    {
        if(removed_[pass->index_])
            continue;

        const int new_index = static_cast<int>(new_passes.size());
        pass->index_ = new_index;
        new_passes.push_back(pass);
    }
    graph.passes_ = std::move(new_passes);
}

Vector<Pass*> Finalizer::topologySortPasses(Graph &graph)
{
    PmrQueue<Pass *> next_passes(&memory_);
    Vector<int> head_count(graph.passes_.size(), &memory_);

    for(auto pass : graph.passes_)
    {
        head_count[pass->index_] = static_cast<int>(pass->heads_.size());
        if(!head_count[pass->index_])
            next_passes.push(pass);
    }

    Vector<Pass *> result(&memory_);
    result.reserve(graph.passes_.size());

    while(!next_passes.empty())
    {
        auto pass = next_passes.front();
        next_passes.pop();

        assert(!head_count[pass->index_]);
        result.push_back(pass);
        for(auto tail : pass->tails_)
        {
            if(!--head_count[tail->index_])
                next_passes.push(tail);
        }
    }

    assert(result.size() == graph.passes_.size());
    return result;
}

void Finalizer::collectResourceUsers()
{
    auto update_record = [](auto &record, Pass *pass, const auto &usage)
    {
        if(!record.users[0])
        {
            record.users[0] = pass;
            record.usages[0] = usage;
        }
        else if(!record.users[1])
        {
            record.users[1] = pass;
            record.usages[1] = usage;
        }

        record.users[2] = record.users[3];
        record.usages[2] = record.usages[3];

        record.users[3] = pass;
        record.usages[3] = usage;
    };

    for(auto pass : sorted_passes_)
    {
        for(auto &[buffer, usage] : pass->buffer_usages_)
            update_record(buffers_[buffer], pass, usage);

        for(auto &[image_key, usage] : pass->image_usages_)
        {
            auto &image = image_key.first;
            auto &range = image_key.second;

            for(uint32_t i = 0; i < range.layerCount; ++i)
            {
                for(uint32_t j = 0; j < range.levelCount; ++j)
                {
                    vk::ImageSubresource subrsc = {
                        .aspectMask = range.aspectMask,
                        .mipLevel   = range.baseMipLevel + j,
                        .arrayLayer = range.baseArrayLayer + i
                    };

                    update_record(images_[{ image.get(), subrsc }], pass, usage);
                }
            }
        }
    }
}

bool Finalizer::isUnnecessaryPrepass(Pass *pass)
{
    if(!pass->heads_.empty())
        return false;

    if(pass->callback_)
        return false;

    if(!pass->pre_buffer_barriers_.empty() ||
       !pass->pre_image_barriers_.empty() ||
       !pass->pre_memory_barriers_.empty())
        return false;
    
    if(!pass->post_buffer_barriers_.empty() ||
       !pass->post_image_barriers_.empty() ||
       !pass->post_memory_barriers_.empty())
        return false;

    if(pass->signal_fence_ || !pass->signal_semaphores_.empty())
        return false;

    if(!pass->wait_semaphores_.empty() && pass->tails_.size() != 1)
        return false;

    for(auto &[buffer, _] : pass->buffer_usages_)
    {
        auto &record = buffers_[buffer];

        if(pass != record.users[0])
            return false;

        auto next_user = record.users[1];
        if(!next_user)
            continue;

        if(pass->queue_->getFamilyIndex() != next_user->queue_->getFamilyIndex())
            return false;
    }

    for(auto &[image_key, _] : pass->image_usages_)
    {
        auto &image = image_key.first;
        auto &range = image_key.second;

        if(!for_each_subrsc(range, [&](const vk::ImageSubresource &subrsc)
        {
            auto &record = images_[{ image.get(), subrsc }];

            if(pass != record.users[0])
                return false;

            auto next_user = record.users[1];
            if(!next_user)
                return true;

            if(pass->queue_->getFamilyIndex() !=
               next_user->queue_->getFamilyIndex())
                return false;

            return true;
        }))
            return false;
    }

    return true;
}

bool Finalizer::isUnnecessaryPostpass(Pass *pass)
{
    if(!pass->tails_.empty())
        return false;

    if(pass->callback_)
        return false;
    
    if(!pass->pre_buffer_barriers_.empty() ||
       !pass->pre_image_barriers_.empty() ||
       !pass->pre_memory_barriers_.empty())
        return false;
    
    if(!pass->post_buffer_barriers_.empty() ||
       !pass->post_image_barriers_.empty() ||
       !pass->post_memory_barriers_.empty())
        return false;

    if(pass->signal_fence_ || !pass->wait_semaphores_.empty())
        return false;

    for(auto &[buffer, _] : pass->buffer_usages_)
    {
        auto &record = buffers_[buffer];

        if(pass != record.users[3])
            return false;

        auto last_user = record.users[2];
        if(!last_user)
            continue;

        if(pass->queue_->getFamilyIndex() != last_user->queue_->getFamilyIndex())
            return false;
    }

    for(auto &[image_key, _] : pass->image_usages_)
    {
        auto &image = image_key.first;
        auto &range = image_key.second;

        if(!for_each_subrsc(range, [&](const vk::ImageSubresource &subrsc)
        {
            auto &record = images_[{ image.get(), subrsc }];

            if(pass != record.users[3])
                return false;

            auto last_user = record.users[2];
            if(!last_user)
                return true;

            if(pass->queue_->getFamilyIndex() !=
               last_user->queue_->getFamilyIndex())
                return false;

            return true;
        }))
            return false;
    }

    return true;
}

void Finalizer::removeUnnecessaryPrepasses(Graph &graph)
{
    for(auto pass : graph.passes_)
    {
        if(removed_[pass->index_])
            continue;

        if(!isUnnecessaryPrepass(pass))
            continue;

        for(auto &[buffer, usage] : pass->buffer_usages_)
        {
            auto &record = buffers_[buffer];
            auto next_user = record.users[1];
            if(!next_user)
                continue;
            auto &next_usage = record.usages[1];
            
            next_user->pre_memory_barriers_.push_back(vk::MemoryBarrier2KHR{
                .srcStageMask  = usage.stages,
                .srcAccessMask = usage.access,
                .dstStageMask  = next_usage.stages,
                .dstAccessMask = next_usage.access
            });
        }

        for(auto &[image_key, usage] : pass->image_usages_)
        {
            for_each_subrsc(image_key.second, [&](const vk::ImageSubresource &subrsc)
            {
                auto &record = images_[{ image_key.first.get(), subrsc }];
                auto next_user = record.users[1];
                if(!next_user)
                    return;
                auto &next_usage = record.usages[1];

                next_user->pre_image_barriers_.push_back(vk::ImageMemoryBarrier2KHR{
                    .srcStageMask        = usage.stages,
                    .srcAccessMask       = usage.access,
                    .dstStageMask        = next_usage.stages,
                    .dstAccessMask       = next_usage.access,
                    .oldLayout           = usage.layout,
                    .newLayout           = next_usage.layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = image_key.first.get(),
                    .subresourceRange    = vk::ImageSubresourceRange{
                        .aspectMask     = subrsc.aspectMask,
                        .baseMipLevel   = subrsc.mipLevel,
                        .levelCount     = 1,
                        .baseArrayLayer = subrsc.arrayLayer,
                        .layerCount     = 1
                    }
                });
            });
        }

        if(!pass->wait_semaphores_.empty())
        {
            assert(pass->tails_.size() == 1);
            auto tail = *pass->tails_.begin();

            std::copy(
                pass->wait_semaphores_.begin(),
                pass->wait_semaphores_.end(),
                std::back_inserter(tail->wait_semaphores_));
        }

        assert((*pass->tails_.begin())->heads_.contains(pass));
        (*pass->tails_.begin())->heads_.erase(pass);

        removed_[pass->index_] = true;
    }
}

void Finalizer::removeUnnecessaryPostpasses(Graph &graph)
{
    for(auto pass : graph.passes_)
    {
        if(removed_[pass->index_])
            continue;

        if(!isUnnecessaryPostpass(pass))
            continue;

        for(auto &[buffer, usage] : pass->buffer_usages_)
        {
            auto &record = buffers_[buffer];
            auto last_user = record.users[2];
            if(!last_user)
                continue;
            auto &last_usage = record.usages[2];

            last_user->post_memory_barriers_.push_back(vk::MemoryBarrier2KHR{
                .srcStageMask  = last_usage.stages,
                .srcAccessMask = last_usage.access,
                .dstStageMask  = usage.stages,
                .dstAccessMask = usage.access
            });
        }

        for(auto &[image_key, usage] : pass->image_usages_)
        {
            for_each_subrsc(image_key.second, [&](const vk::ImageSubresource &subrsc)
            {
                auto &record = images_[{ image_key.first.get(), subrsc }];
                auto last_user = record.users[2];
                if(!last_user)
                    return;
                auto &last_usage = record.usages[2];

                last_user->post_image_barriers_.push_back(vk::ImageMemoryBarrier2KHR{
                    .srcStageMask        = last_usage.stages,
                    .srcAccessMask       = last_usage.access,
                    .dstStageMask        = usage.stages,
                    .dstAccessMask       = usage.access,
                    .oldLayout           = last_usage.layout,
                    .newLayout           = usage.layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = image_key.first.get(),
                    .subresourceRange    = vk::ImageSubresourceRange{
                        .aspectMask     = subrsc.aspectMask,
                        .baseMipLevel   = subrsc.mipLevel,
                        .levelCount     = 1,
                        .baseArrayLayer = subrsc.arrayLayer,
                        .layerCount     = 1
                    }

                });
            });
        }

        if(!pass->signal_semaphores_.empty())
        {
            assert(pass->heads_.size() == 1);
            auto head = *pass->heads_.begin();

            std::copy(
                pass->signal_semaphores_.begin(),
                pass->signal_semaphores_.end(),
                std::back_inserter(head->signal_semaphores_));
        }

        assert((*pass->heads_.begin())->tails_.contains(pass));
        (*pass->heads_.begin())->tails_.erase(pass);

        removed_[pass->index_] = true;
    }
}

VKPT_RENDER_GRAPH_END
