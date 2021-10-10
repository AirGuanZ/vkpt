#include <vkpt/graph/semaphore_wait_handler.h>

VKPT_GRAPH_BEGIN

SemaphoreWaitHandler::SemaphoreWaitHandler(
    std::pmr::memory_resource     &memory,
    Messenger                     *messenger,
    const DAGTransitiveClosure    *closure,
    std::function<CompilePass *()> create_pre_pass)
    : memory_(memory),
      waiting_semaphore_to_resources_(&memory),
      messenger_(messenger),
      closure_(closure),
      create_pre_pass_(std::move(create_pre_pass))
{
    
}

void SemaphoreWaitHandler::collectWaitingSemaphores(const Graph &graph)
{
    auto create_list = agz::misc::lazy_construct([&]
    {
        return List<CompileResource>(&memory_);
    });

    for(auto &[buffer, semaphore] : graph.buffer_waits_)
    {
        auto &resources = waiting_semaphore_to_resources_.try_emplace(
            semaphore, create_list).first->second;
        resources.push_back(buffer);
    }

    for(auto &[image_subrsc, semaphore] : graph.image_waits_)
    {
        auto &resources = waiting_semaphore_to_resources_.try_emplace(
            semaphore, create_list).first->second;
        resources.push_back(image_subrsc);
    }
}

void SemaphoreWaitHandler::processWaitingSemaphores(
    ResourceRecords &resource_records)
{
    for(auto &[semaphore, resources] : waiting_semaphore_to_resources_)
    {
        semaphore.match(
            [&](const auto &s)
            {
                processWaitingSemaphore(s, resources, resource_records);
            });
    }
}

template<typename Resource>
void SemaphoreWaitHandler::processResourceWait(
    const BinarySemaphore                &binary,
    const Resource                       &rsc,
    ResourceRecords                      &resource_records)
{
    constexpr bool is_buffer = std::is_same_v<Resource, Buffer>;

    auto &name = rsc.getName();
    auto &record = resource_records.getRecord(rsc);
    record.has_wait_semaphore = true;

    auto &first_usage = record.usages.front();
    CompilePass *first_pass = first_usage.pass;

    ResourceState state;
    if constexpr(is_buffer)
        state = rsc.getState();
    else
        state = rsc.image.getState(rsc.subrsc);

    state.match(
        [&](const FreeState &)
    {
        messenger_->fatal(
            "{} '{}' in FreeState is declared with a waiting semaphore.",
            std::is_same_v<Resource, Buffer> ? "buffer" : "image", name);
    },
        [&](const UsingState &s)
    {
        if(s.queue->getFamilyIndex() == first_pass->queue->getFamilyIndex())
        {
            auto &submit = first_pass->wait_semaphores[binary];
            assert(!submit.semaphore);
            submit.semaphore = binary;
            submit.stageMask = first_usage.stages;

            if constexpr(std::is_same_v<Resource, Buffer>)
            {
                /*first_pass->pre_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = first_usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });*/
                // do nothing
            }
            else
            {
                if(s.layout != first_usage.layout)
                {
                    first_pass->pre_ext_image_barriers.insert(
                        vk::ImageMemoryBarrier2KHR{
                            .srcStageMask        = first_usage.stages,
                            .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                            .dstStageMask        = first_usage.stages,
                            .dstAccessMask       = first_usage.access,
                            .oldLayout           = s.layout,
                            .newLayout           = first_usage.layout,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image               = rsc.image.get(),
                            .subresourceRange    = subrscToRange(rsc.subrsc)
                        });
                }
            }
        }
        else
        {
            // create a dummy pass to perform the ownership transfer

            auto dummy_pass = create_pre_pass_();

            dummy_pass->queue = s.queue;
            dummy_pass->tails.insert(first_pass);
            first_pass->heads.insert(dummy_pass);

            auto &submit = dummy_pass->wait_semaphores[binary];
            submit.semaphore = binary;
            submit.stageMask = first_usage.stages;

            if constexpr(std::is_same_v<Resource, Buffer>)
            {
                record.usages.push_front(CompileBufferUsage{
                    .pass       = dummy_pass,
                    .stages     = first_usage.stages,
                    .access     = vk::AccessFlagBits2KHR::eNone
                });
                dummy_pass->generated_buffer_usages[rsc] =
                    record.usages.back();
            }
            else
            {
                record.usages.push_front(CompileImageUsage{
                    .pass        = dummy_pass,
                    .stages      = first_usage.stages,
                    .access      = vk::AccessFlagBits2KHR::eNone,
                    .layout      = s.layout,
                    .exit_layout = s.layout
                });
                dummy_pass->generated_image_usages[rsc] =
                    record.usages.back();
            }
        }
    },
        [&](const ReleasedState &s)
    {
        if(s.dst_queue->getFamilyIndex() != first_pass->queue->getFamilyIndex())
        {
            messenger_->fatal(
                "{} '{}' is released to a different queue from its first "
                "usage.",
                std::is_same_v<Resource, Buffer> ? "buffer" : "image", name);
        }
        
        auto &submit = first_pass->wait_semaphores[binary];
        assert(!submit.semaphore);
        submit.semaphore = binary;
        submit.stageMask = first_usage.stages;

        if constexpr(std::is_same_v<Resource, Buffer>)
        {
            first_pass->pre_ext_buffer_barriers.insert(
                vk::BufferMemoryBarrier2KHR{
                    .srcStageMask        = first_usage.stages,
                    .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                    .dstStageMask        = first_usage.stages,
                    .dstAccessMask       = first_usage.access,
                    .srcQueueFamilyIndex = s.src_queue->getFamilyIndex(),
                    .dstQueueFamilyIndex = s.dst_queue->getFamilyIndex(),
                    .buffer              = rsc.get(),
                    .offset              = 0,
                    .size                = VK_WHOLE_SIZE
                });
        }
        else
        {
            first_pass->pre_ext_image_barriers.insert(
                vk::ImageMemoryBarrier2KHR{
                    .srcStageMask        = first_usage.stages,
                    .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                    .dstStageMask        = first_usage.stages,
                    .dstAccessMask       = first_usage.access,
                    .oldLayout           = s.old_layout,
                    .newLayout           = s.new_layout,
                    .srcQueueFamilyIndex = s.src_queue->getFamilyIndex(),
                    .dstQueueFamilyIndex = s.dst_queue->getFamilyIndex(),
                    .image               = rsc.image.get(),
                    .subresourceRange    = subrscToRange(rsc.subrsc)
                });
        }
    });
}

template<typename Resource>
void SemaphoreWaitHandler::processResourceWait(
    const TimelineSemaphore                 &timeline,
    const Resource                          &rsc,
    ResourceRecords                         &resource_records,
    const Map<CompilePass *, CompilePass *> &necessary_passes)
{
    constexpr bool is_buffer = std::is_same_v<Resource, Buffer>;

    auto &record = resource_records.getRecord(rsc);
    auto &first_usage = record.usages.front();
    CompilePass *first_pass = first_usage.pass;
    CompilePass *necessary_pass = necessary_passes.at(first_pass);

    auto &name = rsc.getName();

    const ResourceState &state = rsc.getState();

    state.match(
        [&](const FreeState &)
    {
        messenger_->fatal(
            "{} '{}' in FreeState is declared with a waiting semaphore.",
            is_buffer ? "buffer" : "image", name);
    },
        [&](const UsingState &s)
    {
        const uint32_t first_family = first_pass->queue->getFamilyIndex();
        const uint32_t state_family = first_pass->queue->getFamilyIndex();
        const uint32_t necessary_family =
            necessary_pass->queue->getFamilyIndex();

        if(necessary_family == first_family)
        {
            if(state_family == necessary_family)
            {
                // s, n, f: same queue family

                auto &submit = necessary_pass->wait_semaphores[timeline];
                submit.semaphore  = timeline;
                submit.stageMask |= first_usage.stages;
                submit.value      = timeline.getLastSignalValue();

                if constexpr(is_buffer)
                {
                    /*necessary_pass->pre_ext_buffer_barriers.insert(
                        vk::BufferMemoryBarrier2KHR{
                            .srcStageMask        = first_usage.stages,
                            .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                            .dstStageMask        = first_usage.stages,
                            .dstAccessMask       = first_usage.access,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .buffer              = rsc.get(),
                            .offset              = 0,
                            .size                = VK_WHOLE_SIZE
                        });*/
                }
                else
                {
                    if(s.layout != first_usage.layout)
                    {
                        necessary_pass->pre_ext_image_barriers.insert(
                            vk::ImageMemoryBarrier2KHR{
                                .srcStageMask        = first_usage.stages,
                                .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                                .dstStageMask        = first_usage.stages,
                                .dstAccessMask       = first_usage.access,
                                .oldLayout           = s.layout,
                                .newLayout           = first_usage.layout,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image               = rsc.image.get(),
                                .subresourceRange    = subrscToRange(rsc.subrsc)
                            });
                    }
                }
            }
            else
            {
                // s: queue family 1
                // n, f: queue family 2

                auto dummy_pass = create_pre_pass_();
                dummy_pass->queue = s.queue;

                dummy_pass->tails.insert(first_pass);
                first_pass->heads.insert(dummy_pass);

                auto &submit = dummy_pass->wait_semaphores[timeline];
                submit.semaphore = timeline;
                submit.stageMask = first_usage.stages;
                submit.value     = timeline.getLastSignalValue();

                if constexpr(is_buffer)
                {
                    record.usages.push_front(CompileBufferUsage{
                        .pass       = dummy_pass,
                        .stages     = first_usage.stages,
                        .access     = vk::AccessFlagBits2KHR::eNone
                    });
                    dummy_pass->generated_buffer_usages[rsc] =
                        record.usages.front();
                }
                else
                {
                    record.usages.push_front(CompileImageUsage{
                        .pass        = dummy_pass,
                        .stages      = first_usage.stages,
                        .access      = vk::AccessFlagBits2KHR::eNone,
                        .layout      = s.layout,
                        .exit_layout = s.layout
                    });
                    dummy_pass->generated_image_usages[rsc] =
                        record.usages.front();
                }
            }
        }
        else
        {
            if(state_family == first_family)
            {
                // necessary_pass: queue_family 1
                // s, first_pass: queue family 2
                
                auto &submit = first_pass->wait_semaphores[timeline];
                submit.semaphore  = timeline;
                submit.stageMask |= first_usage.stages;
                submit.value      = timeline.getLastSignalValue();

                if constexpr(is_buffer)
                {
                    /*first_pass->pre_ext_buffer_barriers.insert(
                        vk::BufferMemoryBarrier2KHR{
                            .srcStageMask        = first_usage.stages,
                            .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                            .dstStageMask        = first_usage.stages,
                            .dstAccessMask       = first_usage.access,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .buffer              = rsc.get(),
                            .offset              = 0,
                            .size                = VK_WHOLE_SIZE
                        });*/
                }
                else
                {
                    if(s.layout != first_usage.layout)
                    {
                        first_pass->pre_ext_image_barriers.insert(
                            vk::ImageMemoryBarrier2KHR{
                                .srcStageMask        = first_usage.stages,
                                .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                                .dstStageMask        = first_usage.stages,
                                .dstAccessMask       = first_usage.access,
                                .oldLayout           = s.layout,
                                .newLayout           = first_usage.layout,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image               = rsc.image.get(),
                                .subresourceRange    = subrscToRange(rsc.subrsc)
                            });
                    }
                }
            }
            else if(state_family == first_family)
            {
                // s, necessary: queue family 1
                // first_pass: queue family 2

                auto &submit = necessary_pass->wait_semaphores[timeline];
                submit.semaphore  = timeline;
                submit.stageMask |= first_usage.stages;
                submit.value      = timeline.getLastSignalValue();

                if constexpr(is_buffer)
                {
                    record.usages.push_front(CompileBufferUsage{
                        .pass       = necessary_pass,
                        .stages     = first_usage.stages,
                        .access     = vk::AccessFlagBits2KHR::eNone
                    });
                    necessary_pass->generated_buffer_usages[rsc] =
                        record.usages.front();
                }
                else
                {
                    record.usages.push_front(CompileImageUsage{
                        .pass        = necessary_pass,
                        .stages      = first_usage.stages,
                        .access      = vk::AccessFlagBits2KHR::eNone,
                        .layout      = s.layout,
                        .exit_layout = s.layout
                    });
                    necessary_pass->generated_image_usages[rsc] =
                        record.usages.front();
                }
            }
            else
            {
                // s, necessary, first: 3 different queue families
                // this sounds unpractical but i cannot ignore it

                auto dummy_pass = create_pre_pass_();
                dummy_pass->queue = s.queue;

                dummy_pass->tails.insert(first_pass);
                first_pass->heads.insert(dummy_pass);

                auto &submit = dummy_pass->wait_semaphores[timeline];
                submit.semaphore = timeline;
                submit.stageMask = first_usage.stages;
                submit.value     = timeline.getLastSignalValue();

                if constexpr(is_buffer)
                {
                    record.usages.push_front(CompileBufferUsage{
                        .pass       = dummy_pass,
                        .stages     = first_usage.stages,
                        .access     = vk::AccessFlagBits2KHR::eNone
                    });
                    dummy_pass->generated_buffer_usages[rsc] =
                        record.usages.front();
                }
                else
                {
                    record.usages.push_front(CompileImageUsage{
                        .pass        = necessary_pass,
                        .stages      = first_usage.stages,
                        .access      = vk::AccessFlagBits2KHR::eNone,
                        .layout      = s.layout,
                        .exit_layout = s.layout
                    });
                    dummy_pass->generated_image_usages[rsc] =
                        record.usages.front();
                }
            }
        }
    },
        [&](const ReleasedState &s)
    {
        if(s.dst_queue->getFamilyIndex() !=
           first_pass->queue->getFamilyIndex())
        {
            messenger_->fatal(
                "{} '{}' is released to a different queue from its "
                "first usage.", is_buffer ? "buffer" : "image", name);
        }

        if(necessary_pass->queue->getFamilyIndex() ==
           first_pass->queue->getFamilyIndex())
        {
            auto &submit = necessary_pass->wait_semaphores[timeline];
            submit.semaphore  = timeline;
            submit.stageMask |= first_usage.stages;
            submit.value      = timeline.getLastSignalValue();

            if constexpr(is_buffer)
            {
                necessary_pass->pre_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = first_usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .srcQueueFamilyIndex = s.src_queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = s.dst_queue->getFamilyIndex(),
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });
            }
            else
            {
                necessary_pass->pre_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = first_usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .oldLayout           = s.old_layout,
                        .newLayout           = s.new_layout,
                        .srcQueueFamilyIndex = s.src_queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = s.dst_queue->getFamilyIndex(),
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    });
            }
        }
        else
        {
            auto &submit = first_pass->wait_semaphores[timeline];
            submit.semaphore  = timeline;
            submit.stageMask |= first_usage.stages;
            submit.value      = timeline.getLastSignalValue();

            if constexpr(is_buffer)
            {
                first_pass->pre_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = first_usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .srcQueueFamilyIndex = s.src_queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = s.dst_queue->getFamilyIndex(),
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });
            }
            else
            {
                first_pass->pre_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = first_usage.stages,
                        .srcAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .dstStageMask        = first_usage.stages,
                        .dstAccessMask       = first_usage.access,
                        .oldLayout           = s.old_layout,
                        .newLayout           = s.new_layout,
                        .srcQueueFamilyIndex = s.src_queue->getFamilyIndex(),
                        .dstQueueFamilyIndex = s.dst_queue->getFamilyIndex(),
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    });
            }
        }
    });
}

void SemaphoreWaitHandler::processWaitingSemaphore(
    const BinarySemaphore                &binary,
    const List<CompileResource>          &resources,
    ResourceRecords                      &resource_records)
{
    assert(!resources.empty());
    if(resources.size() > 1)
    {
        auto &rsc = resources.front();
        messenger_->fatal(
            "{} '{}'s waiting semaphore is binary but is waited by multiple"
            "resources.",
            rsc.is<Buffer>() ? "buffer" : "image",
            rsc.is<Buffer>() ? rsc.as<Buffer>().getName() :
            rsc.as<ImageSubresource>().getName());
    }

    resources.front().match(
        [&](auto &rsc)
        {
            processResourceWait(
                binary, rsc, resource_records);
        });
}

void SemaphoreWaitHandler::processWaitingSemaphore(
    const TimelineSemaphore              &timeline,
    const List<CompileResource>          &resources,
    ResourceRecords                      &resource_records)
{
    assert(!resources.empty());

    Set<CompilePass *> passes(&memory_);

    for(auto &resource : resources)
    {
        resource.match(
            [&](const Buffer &buffer)
        {
            auto &record = resource_records.getRecord(buffer);
            record.has_wait_semaphore = true;
            passes.insert(record.usages.front().pass);
        },
            [&](const ImageSubresource &image)
        {
            auto &record = resource_records.getRecord(image);
            record.has_wait_semaphore = true;
            passes.insert(record.usages.front().pass);
        });
    }

    Map<CompilePass *, CompilePass *> necessary_passes(&memory_);
    for(auto it = passes.begin(); it != passes.end(); ++it)
    {
        auto pass = *it;
        necessary_passes.insert({ pass, pass });

        for(auto jt = std::next(it), njt = jt; jt != passes.end(); jt = njt)
        {
            ++njt;
            if(closure_->isReachable(pass->sorted_index, (*jt)->sorted_index))
            {
                necessary_passes.insert({ *jt, pass });
                passes.erase(jt);
            }
        }
    }

    for(auto &resource : resources)
    {
        resource.match([&](const auto &rsc)
        {
            processResourceWait(
                timeline, rsc, resource_records, necessary_passes);
        });
    }
}

VKPT_GRAPH_END
