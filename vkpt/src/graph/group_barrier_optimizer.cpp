#include <vkpt/graph/group_barrier_optimizer.h>

VKPT_GRAPH_BEGIN

bool GroupBarrierOptimizer::isReadOnly(vk::AccessFlags2KHR access)
{
    const vk::AccessFlagBits2KHR write_bits[] = {
        vk::AccessFlagBits2KHR::eShaderWrite,
        vk::AccessFlagBits2KHR::eColorAttachmentWrite,
        vk::AccessFlagBits2KHR::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2KHR::eTransferWrite,
        vk::AccessFlagBits2KHR::eHostWrite,
        vk::AccessFlagBits2KHR::eMemoryWrite,
        vk::AccessFlagBits2KHR::eShaderStorageWrite,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        vk::AccessFlagBits2KHR::eVideoDecodeWrite,
        vk::AccessFlagBits2KHR::eVideoEncodeWrite,
#endif
        vk::AccessFlagBits2KHR::eTransformFeedbackWriteExt,
        vk::AccessFlagBits2KHR::eTransformFeedbackCounterWriteExt,
        vk::AccessFlagBits2KHR::eCommandPreprocessWriteNv,
        vk::AccessFlagBits2KHR::eAccelerationStructureWrite,
        vk::AccessFlagBits2KHR::eAccelerationStructureWriteNv
    };

    for(auto bit : write_bits)
    {
        if(access & bit)
            return false;
    }
    return true;
}

void GroupBarrierOptimizer::optimize(CompileGroup *group)
{
    movePreExtBarriers(group);
    movePostExtBarriers(group);
    mergePreAndPostBarriers(group);

    // TODO: convert buffer barriers to global memory barriers
    // TODO: remove redundant barriers
}

void GroupBarrierOptimizer::movePreExtBarriers(CompileGroup *group)
{
    for(size_t i = 1; i < group->passes.size(); ++i)
    {
        auto pass = group->passes[i];
        if(pass->pre_ext_buffer_barriers.empty() &&
           pass->pre_ext_image_barriers.empty())
            continue;
        if(!pass->pre_buffer_barriers.empty() ||
           !pass->pre_image_barriers.empty())
            continue;

        CompilePass *dst = nullptr;
        for(size_t j = 0; j < i; ++j)
        {
            dst = group->passes[j];
            if(!dst->pre_buffer_barriers.empty() ||
               !dst->pre_image_barriers.empty() ||
               !dst->pre_ext_buffer_barriers.empty() ||
               !dst->pre_ext_image_barriers.empty())
                break;
        }

        if(!dst)
            continue;

        std::copy(
            pass->pre_ext_buffer_barriers.begin(),
            pass->pre_ext_buffer_barriers.end(),
            agz::misc::direct_inserter(dst->pre_ext_buffer_barriers));
        pass->pre_ext_buffer_barriers.clear();

        std::copy(
            pass->pre_ext_image_barriers.begin(),
            pass->pre_ext_image_barriers.end(),
            agz::misc::direct_inserter(dst->pre_ext_image_barriers));
        pass->pre_ext_image_barriers.clear();
    }
}

void GroupBarrierOptimizer::movePostExtBarriers(CompileGroup *group)
{
    for(int i = static_cast<int>(group->passes.size()) - 2; i >= 0; --i)
    {
        auto pass = group->passes[i];
        if(pass->post_ext_buffer_barriers.empty() &&
           pass->post_ext_image_barriers.empty())
            continue;

        CompilePass *dst = nullptr;
        for(int j = static_cast<int>(group->passes.size()) - 1; j > i; --j)
        {
            dst = group->passes[j];
            if(!dst->pre_buffer_barriers.empty() ||
               !dst->pre_image_barriers.empty() ||
               !dst->pre_ext_buffer_barriers.empty() ||
               !dst->pre_ext_image_barriers.empty() ||
               !dst->post_ext_buffer_barriers.empty() ||
               !dst->post_ext_image_barriers.empty())
                break;
        }

        if(!dst)
            continue;

        if(!dst->pre_buffer_barriers.empty() ||
           !dst->pre_image_barriers.empty() ||
           !dst->pre_ext_image_barriers.empty() ||
           !dst->pre_ext_buffer_barriers.empty())
        {
            std::copy(
                pass->post_ext_buffer_barriers.begin(),
                pass->post_ext_buffer_barriers.end(),
                agz::misc::direct_inserter(dst->pre_ext_buffer_barriers));

            std::copy(
                pass->post_ext_image_barriers.begin(),
                pass->post_ext_image_barriers.end(),
                agz::misc::direct_inserter(dst->pre_ext_image_barriers));
        }
        else
        {
            assert(!dst->post_ext_buffer_barriers.empty() ||
                   !dst->post_ext_image_barriers.empty());

            std::copy(
                pass->post_ext_buffer_barriers.begin(),
                pass->post_ext_buffer_barriers.end(),
                agz::misc::direct_inserter(dst->post_ext_buffer_barriers));

            std::copy(
                pass->post_ext_image_barriers.begin(),
                pass->post_ext_image_barriers.end(),
                agz::misc::direct_inserter(dst->post_ext_image_barriers));
        }

        pass->post_ext_buffer_barriers.clear();
        pass->post_ext_image_barriers.clear();
    }
}

void GroupBarrierOptimizer::mergePreAndPostBarriers(CompileGroup *group)
{
    for(size_t i = 1; i < group->passes.size(); ++i)
    {
        auto last_pass = group->passes[i - 1];
        auto pass      = group->passes[i];

        if(last_pass->post_ext_buffer_barriers.empty() &&
           last_pass->post_ext_image_barriers.empty())
            continue;

        std::copy(
            last_pass->post_ext_buffer_barriers.begin(),
            last_pass->post_ext_buffer_barriers.end(),
            agz::misc::direct_inserter(pass->pre_ext_buffer_barriers));

        std::copy(
            last_pass->post_ext_image_barriers.begin(),
            last_pass->post_ext_image_barriers.end(),
            agz::misc::direct_inserter(pass->post_ext_image_barriers));

        last_pass->post_ext_buffer_barriers.clear();
        last_pass->post_ext_image_barriers.clear();
    }
}

VKPT_GRAPH_END
