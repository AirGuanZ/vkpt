#include <vkpt/graph/group_barrier_generator.h>

VKPT_GRAPH_BEGIN

GroupBarrierGenerator::GroupBarrierGenerator(
    std::pmr::memory_resource &memory, const ResourceRecords &resource_records)
    : dependencies_(memory), resource_records_(resource_records)
{
    
}

void GroupBarrierGenerator::fillBarriers(CompileGroup *group)
{
    last_pass_with_pre_barrier_ = nullptr;

    for(size_t pass_i = 0; pass_i < group->passes.size(); ++pass_i)
    {
        auto pass = group->passes[pass_i];
        assert(pass->sorted_index_in_group == static_cast<int>(pass_i));

        for(auto &[buffer, usage] : pass->generated_buffer_usages_)
            handleResource(pass, buffer, usage);

        for(auto &[image_subrsc, usage] : pass->generated_image_usages_)
            handleResource(pass, image_subrsc, usage);

        if(!pass->raw_pass)
            continue;

        for(auto &[buffer, usage] : pass->raw_pass->_getBufferUsages())
            handleResource(pass, buffer, usage);

        for(auto &[image_subrsc, usage] : pass->raw_pass->_getImageUsages())
            handleResource(pass, image_subrsc, usage);
    }
}

CompilePass *GroupBarrierGenerator::getBarrierPass(CompilePass *A, CompilePass *B)
{
    assert(B);

    if(!last_pass_with_pre_barrier_)
    {
        last_pass_with_pre_barrier_ = B;
        return B;
    }

    if(!A)
        return last_pass_with_pre_barrier_;

    if(last_pass_with_pre_barrier_->sorted_index <= A->sorted_index)
    {
        last_pass_with_pre_barrier_ = B;
        return B;
    }

    return last_pass_with_pre_barrier_;
}

template<typename Resource, typename PassUsage>
void GroupBarrierGenerator::handleResource(
    CompilePass *pass, const Resource &rsc, const PassUsage &usage)
{
    constexpr bool is_buffer = std::is_same_v<Resource, Buffer>;

    auto &record = resource_records_.getRecord(rsc);
    auto usage_it = record.pass_to_usage.at(pass);
    if(usage_it == record.usages.begin())
        return;

    auto &last_usage = *std::prev(usage_it);
    auto last_user = last_usage.pass;
    CompileGroup *group = pass->group;
    CompileGroup *last_group = last_user->group;

    if(last_group->queue == group->queue)
    {
        assert(
            last_user->sorted_index_in_group <
            pass->sorted_index_in_group);
        auto barrier_pass = getBarrierPass(last_user, pass);

        if constexpr(is_buffer)
        {
            barrier_pass->pre_buffer_barriers.insert({
                rsc, vk::BufferMemoryBarrier2KHR{
                    .srcStageMask        = last_usage.end_stages,
                    .srcAccessMask       = last_usage.end_access,
                    .dstStageMask        = usage.stages,
                    .dstAccessMask       = usage.access,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer              = rsc.get(),
                    .offset              = 0,
                    .size                = VK_WHOLE_SIZE
                }
            });
        }
        else
        {
            barrier_pass->pre_image_barriers.insert({
                rsc, vk::ImageMemoryBarrier2KHR{
                    .srcStageMask        = last_usage.end_stages,
                    .srcAccessMask       = last_usage.end_access,
                    .dstStageMask        = usage.stages,
                    .dstAccessMask       = usage.access,
                    .oldLayout           = last_usage.end_layout,
                    .newLayout           = usage.layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = rsc.image.get(),
                    .subresourceRange    = subrscToRange(rsc.subrsc)
                }
            });
        }
    }
    else
    {
        // cross queue dependency

        auto &global_group_dependency =
            dependencies_.getDependency(last_group, group);
        auto direct_head = global_group_dependency.end_entry_head;

        assert(group->heads.contains(direct_head));
        group->heads[direct_head] |= usage.stages;

        if(group->queue->getFamilyIndex() !=
           last_group->queue->getFamilyIndex())
        {
            auto barrier_pass = getBarrierPass(nullptr, pass);

            if constexpr(is_buffer)
            {
                barrier_pass->pre_buffer_barriers.insert({
                    rsc, vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = usage.stages,
                        .dstAccessMask       = usage.access,
                        .srcQueueFamilyIndex = last_group->queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = group->queue->getFamilyIndex(),
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    }
                });
                
                last_usage.pass->post_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask         = last_usage.end_stages,
                        .srcAccessMask        = last_usage.end_access,
                        .dstStageMask         = last_usage.end_stages,
                        .dstAccessMask        = vk::AccessFlagBits2KHR::eNone,
                        .srcQueueFamilyIndex  = last_group->queue->getFamilyIndex(),
                        .dstQueueFamilyIndex  = group->queue->getFamilyIndex(),
                        .buffer               = rsc.get(),
                        .offset               = 0,
                        .size                 = VK_WHOLE_SIZE
                    });
            }
            else
            {
                barrier_pass->pre_image_barriers.insert({
                    rsc, vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = usage.stages,
                        .dstAccessMask       = usage.access,
                        .oldLayout           = last_usage.end_layout,
                        .newLayout           = usage.layout,
                        .srcQueueFamilyIndex = last_group->queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = group->queue->getFamilyIndex(),
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    }
                });

                last_usage.pass->post_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.end_stages,
                        .srcAccessMask       = last_usage.end_access,
                        .dstStageMask        = last_usage.end_stages,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .oldLayout           = last_usage.end_layout,
                        .newLayout           = usage.layout,
                        .srcQueueFamilyIndex = last_group->queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = group->queue->getFamilyIndex(),
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    });
            }
        }
        else if constexpr(!is_buffer)
        {
            // image may need layout transition

            if(last_usage.layout != usage.layout)
            {
                auto barrier_pass = getBarrierPass(nullptr, pass);
                barrier_pass->pre_image_barriers.insert({
                    rsc, vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = usage.stages,
                        .dstAccessMask       = usage.access,
                        .oldLayout           = last_usage.end_layout,
                        .newLayout           = usage.layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    }
                });
            }
        }
    }
}

VKPT_GRAPH_END
