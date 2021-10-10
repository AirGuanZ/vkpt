#include <vkpt/graph/semaphore_signal_handler.h>

VKPT_GRAPH_BEGIN

SemaphoreSignalHandler::SemaphoreSignalHandler(
    std::pmr::memory_resource            &memory,
    const DAGTransitiveClosure           *closure,
    Map<Buffer, ResourceState>           &buffer_final_states,
    Map<ImageSubresource, ResourceState> &image_final_states,
    std::function<CompilePass *()>        create_post_pass)
    : memory_(memory),
      signaling_semaphore_to_resources_(&memory),
      closure_(closure),
      buffer_final_states_(buffer_final_states),
      image_final_states_(image_final_states),
      create_post_pass_(std::move(create_post_pass))
{
    
}

void SemaphoreSignalHandler::collectSignalingSemaphores(const Graph &graph)
{
    auto create_list = agz::misc::lazy_construct([&]
    {
        return List<CompileResource>(&memory_);
    });

    for(auto &[buffer, signal] : graph.buffer_signals_)
    {
        auto &resources = signaling_semaphore_to_resources_.try_emplace(
            signal.semaphore, create_list).first->second;
        resources.push_back(buffer);
    }
    for(auto &[image_subrsc, signal] : graph.image_signals_)
    {
        auto &resources = signaling_semaphore_to_resources_.try_emplace(
            signal.semaphore, create_list).first->second;
        resources.push_back(image_subrsc);
    }
}

void SemaphoreSignalHandler::processSignalingSemaphores(
    ResourceRecords &resource_records, const Graph &graph)
{
    for(auto &[semaphore, resources] : signaling_semaphore_to_resources_)
    {
        auto mutable_semaphore = semaphore;
        processSignalingSemaphore(
            mutable_semaphore, resources, resource_records, graph);
    }
}

template<typename Resource>
void SemaphoreSignalHandler::processSignalingSemaphore(
    const Resource                  &rsc,
    ResourceRecords                 &resource_records,
    Set<CompilePass*>               &emit_passes,
    Map<CompilePass*, CompilePass*> &necessary_passes,
    Map<const Queue*, CompilePass*> &queue_to_dummy_pass,
    const Graph                     &graph)
{
    constexpr bool is_buffer = std::is_same_v<Resource, Buffer>;
    
    auto &record = resource_records.getRecord(rsc);
    auto &name = rsc.getName();

    // get signal info

    const Graph::Signal *p_signal;
    if constexpr(is_buffer)
        p_signal = &graph.buffer_signals_.at(rsc);
    else
        p_signal = &graph.image_signals_.at(rsc);
    auto &signal = *p_signal;

    auto &last_usage = record.usages.back();
    CompilePass *last_pass = last_usage.pass;
    CompilePass *necessary_pass = necessary_passes.at(last_pass);

    const uint32_t last_family      = last_pass->queue->getFamilyIndex();
    const uint32_t signal_family    = signal.queue->getFamilyIndex();
    const uint32_t necessary_family = necessary_pass->queue->getFamilyIndex();

    if(last_family == necessary_family)
    {
        if(necessary_family == signal_family)
        {
            emit_passes.insert(necessary_pass);

            if constexpr(is_buffer)
            {
                /*last_pass->post_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.end_stages,
                        .srcAccessMask       = last_usage.end_access,
                        .dstStageMask        = last_usage.end_stages,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });*/

                buffer_final_states_[rsc] = UsingState{
                    .queue  = last_pass->queue,
                    .stages = last_usage.stages,
                    .access = last_usage.access
                };
            }
            else
            {
                if(last_usage.exit_layout != signal.layout)
                {
                    last_pass->post_ext_image_barriers.insert(
                        vk::ImageMemoryBarrier2KHR{
                            .srcStageMask        = last_usage.stages,
                            .srcAccessMask       = last_usage.access,
                            .dstStageMask        = last_usage.stages,
                            .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                            .oldLayout           = last_usage.exit_layout,
                            .newLayout           = signal.layout,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image               = rsc.image.get(),
                            .subresourceRange    = subrscToRange(rsc.subrsc)
                        });
                }

                image_final_states_[rsc] = UsingState{
                    .queue  = last_pass->queue,
                    .stages = last_usage.stages,
                    .access = last_usage.access,
                    .layout = last_usage.exit_layout
                };
            }
        }
        else
        {
            assert(last_family == necessary_family);
            assert(last_family != signal_family);
            
            if(signal.release_only)
            {
                emit_passes.insert(necessary_pass);

                if constexpr(is_buffer)
                {
                    necessary_pass->post_ext_buffer_barriers.insert(
                        vk::BufferMemoryBarrier2KHR{
                            .srcStageMask        = last_usage.stages,
                            .srcAccessMask       = last_usage.access,
                            .dstStageMask        = vk::PipelineStageFlagBits2KHR::eNone,
                            .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                            .srcQueueFamilyIndex = last_family,
                            .dstQueueFamilyIndex = signal_family,
                            .buffer              = rsc.get(),
                            .offset              = 0,
                            .size                = VK_WHOLE_SIZE
                        });

                    buffer_final_states_[rsc] = ReleasedState{
                        .src_queue = last_pass->queue,
                        .dst_queue = signal.queue
                    };
                }
                else
                {
                    necessary_pass->post_ext_image_barriers.insert(
                        vk::ImageMemoryBarrier2KHR{
                            .srcStageMask        = last_usage.stages,
                            .srcAccessMask       = last_usage.access,
                            .dstStageMask        = vk::PipelineStageFlagBits2KHR::eNone,
                            .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                            .oldLayout           = last_usage.exit_layout,
                            .newLayout           = signal.layout,
                            .srcQueueFamilyIndex = last_family,
                            .dstQueueFamilyIndex = signal_family,
                            .image               = rsc.image.get(),
                            .subresourceRange    = subrscToRange(rsc.subrsc)
                        });

                    image_final_states_[rsc] = ReleasedState{
                        .src_queue  = last_pass->queue,
                        .dst_queue  = signal.queue,
                        .old_layout = last_usage.exit_layout,
                        .new_layout = signal.layout
                    };
                }
            }
            else
            {
                // create a dummy pass to complete the ownership transfer

                CompilePass *dummy_pass = queue_to_dummy_pass.try_emplace(
                    signal.queue, agz::misc::lazy_construct([&]
                {
                    auto pass =  create_post_pass_();
                    pass->queue = signal.queue;
                    return pass;
                })).first->second;

                emit_passes.insert(dummy_pass);

                necessary_pass->tails.insert(dummy_pass);
                dummy_pass->heads.insert(necessary_pass);

                if constexpr(is_buffer)
                {
                    record.usages.push_back(CompileBufferUsage{
                        .pass       = dummy_pass,
                        .stages     = vk::PipelineStageFlagBits2KHR::eNone,
                        .access     = vk::AccessFlagBits2KHR::eNone
                    });
                    dummy_pass->generated_buffer_usages_[rsc] =
                        record.usages.back();

                    buffer_final_states_[rsc] = UsingState{
                        .queue  = signal.queue,
                        .stages = vk::PipelineStageFlagBits2KHR::eNone,
                        .access = vk::AccessFlagBits2KHR::eNone
                    };
                }
                else
                {
                    record.usages.push_back(CompileImageUsage{
                        .pass        = dummy_pass,
                        .stages      = vk::PipelineStageFlagBits2KHR::eNone,
                        .access      = vk::AccessFlagBits2KHR::eNone,
                        .layout      = signal.layout,
                        .exit_layout = signal.layout
                    });
                    dummy_pass->generated_image_usages_[rsc] =
                        record.usages.back();

                    image_final_states_[rsc] = UsingState{
                        .queue = signal.queue,
                        .stages = vk::PipelineStageFlagBits2KHR::eNone,
                        .access = vk::AccessFlagBits2KHR::eNone,
                        .layout = signal.layout
                    };
                }
            }
        }
    }
    else if(last_family == signal_family)
    {
        assert(last_family == signal_family);
        assert(last_family != necessary_family);

        emit_passes.insert(necessary_pass);

        if constexpr(is_buffer)
        {
            /*last_pass->post_ext_buffer_barriers.insert(
                vk::BufferMemoryBarrier2KHR{
                    .srcStageMask        = last_usage.end_stages,
                    .srcAccessMask       = last_usage.end_access,
                    .dstStageMask        = last_usage.end_stages,
                    .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer              = rsc.get(),
                    .offset              = 0,
                    .size                = VK_WHOLE_SIZE
                });*/

            buffer_final_states_[rsc] = UsingState{
                .queue  = last_pass->queue,
                .stages = last_usage.stages,
                .access = last_usage.access
            };
        }
        else
        {
            if(last_usage.exit_layout != signal.layout)
            {
                last_pass->post_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.stages,
                        .srcAccessMask       = last_usage.access,
                        .dstStageMask        = last_usage.stages,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .oldLayout           = last_usage.exit_layout,
                        .newLayout           = signal.layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    });
            }

            image_final_states_[rsc] = UsingState{
                .queue  = last_pass->queue,
                .stages = last_usage.stages,
                .access = last_usage.access,
                .layout = signal.layout
            };
        }
    }
    else if(necessary_family == signal_family)
    {
        assert(necessary_family == signal_family);
        assert(last_family != signal_family);

        emit_passes.insert(necessary_pass);

        if(signal.release_only)
        {
            if constexpr(is_buffer)
            {
                last_pass->post_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.stages,
                        .srcAccessMask       = last_usage.access,
                        .dstStageMask        = last_usage.stages,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .srcQueueFamilyIndex = last_family,
                        .dstQueueFamilyIndex = signal_family,
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });

                buffer_final_states_[rsc] = ReleasedState{
                    .src_queue = last_pass->queue,
                    .dst_queue = signal.queue
                };
            }
            else
            {
                last_pass->post_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.stages,
                        .srcAccessMask       = last_usage.access,
                        .dstStageMask        = last_usage.stages,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .oldLayout           = last_usage.exit_layout,
                        .newLayout           = signal.layout,
                        .srcQueueFamilyIndex = last_family,
                        .dstQueueFamilyIndex = signal_family,
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    });

                image_final_states_[rsc] = ReleasedState{
                    .src_queue  = last_pass->queue,
                    .dst_queue  = signal.queue,
                    .old_layout = last_usage.exit_layout,
                    .new_layout = signal.layout
                };
            }
        }
        else
        {
            // transfer ownership to necessary pass queue family

            if constexpr(is_buffer)
            {
                record.usages.push_back(CompileBufferUsage{
                    .pass       = necessary_pass,
                    .stages     = vk::PipelineStageFlagBits2KHR::eNone,
                    .access     = vk::AccessFlagBits2KHR::eNone
                });
                necessary_pass->generated_buffer_usages_[rsc] =
                    record.usages.back();

                buffer_final_states_[rsc] = UsingState{
                    .queue  = necessary_pass->queue,
                    .stages = vk::PipelineStageFlagBits2KHR::eNone,
                    .access = vk::AccessFlagBits2KHR::eNone
                };
            }
            else
            {
                record.usages.push_back(CompileImageUsage{
                    .pass        = necessary_pass,
                    .stages      = vk::PipelineStageFlagBits2KHR::eNone,
                    .access      = vk::AccessFlagBits2KHR::eNone,
                    .layout      = signal.layout,
                    .exit_layout = signal.layout
                });
                necessary_pass->generated_image_usages_[rsc] =
                    record.usages.back();

                image_final_states_[rsc] = UsingState{
                    .queue  = necessary_pass->queue,
                    .stages = vk::PipelineStageFlagBits2KHR::eNone,
                    .access = vk::AccessFlagBits2KHR::eNone,
                    .layout = signal.layout
                };
            }
        }
    }
    else
    {
        // three passes are in three different queue families

        assert(last_family != necessary_family);
        assert(necessary_family != signal_family);
        assert(signal_family != last_family);

        if(signal.release_only)
        {
            emit_passes.insert(necessary_pass);

            if constexpr(is_buffer)
            {
                last_pass->post_ext_buffer_barriers.insert(
                    vk::BufferMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.stages,
                        .srcAccessMask       = last_usage.access,
                        .dstStageMask        = vk::PipelineStageFlagBits2KHR::eNone,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .srcQueueFamilyIndex = last_family,
                        .dstQueueFamilyIndex = signal_family,
                        .buffer              = rsc.get(),
                        .offset              = 0,
                        .size                = VK_WHOLE_SIZE
                    });

                buffer_final_states_[rsc] = ReleasedState{
                    .src_queue = last_pass->queue,
                    .dst_queue = signal.queue
                };
            }
            else
            {
                last_pass->post_ext_image_barriers.insert(
                    vk::ImageMemoryBarrier2KHR{
                        .srcStageMask        = last_usage.stages,
                        .srcAccessMask       = last_usage.access,
                        .dstStageMask        = vk::PipelineStageFlagBits2KHR::eNone,
                        .dstAccessMask       = vk::AccessFlagBits2KHR::eNone,
                        .oldLayout           = last_usage.exit_layout,
                        .newLayout           = signal.layout,
                        .srcQueueFamilyIndex = last_family,
                        .dstQueueFamilyIndex = signal_family,
                        .image               = rsc.image.get(),
                        .subresourceRange    = subrscToRange(rsc.subrsc)
                    });

                image_final_states_[rsc] = ReleasedState{
                    .src_queue  = last_pass->queue,
                    .dst_queue  = signal.queue,
                    .old_layout = last_usage.exit_layout,
                    .new_layout = signal.layout
                };
            }
        }
        else
        {
            // create a dummy pass to complete the ownership transfer

            CompilePass *dummy_pass = queue_to_dummy_pass.try_emplace(
                signal.queue, agz::misc::lazy_construct([&]
            {
                auto pass = create_post_pass_();
                pass->queue = signal.queue;
                return pass;
            })).first->second;

            emit_passes.insert(dummy_pass);

            last_pass->tails.insert(dummy_pass);
            dummy_pass->heads.insert(last_pass);

            if constexpr(is_buffer)
            {
                record.usages.push_back(CompileBufferUsage{
                    .pass       = dummy_pass,
                    .stages     = vk::PipelineStageFlagBits2KHR::eNone,
                    .access     = vk::AccessFlagBits2KHR::eNone
                });
                dummy_pass->generated_buffer_usages_[rsc] =
                    record.usages.back();

                buffer_final_states_[rsc] = UsingState{
                    .queue  = dummy_pass->queue,
                    .stages = vk::PipelineStageFlagBits2KHR::eNone,
                    .access = vk::AccessFlagBits2KHR::eNone
                };
            }
            else
            {
                record.usages.push_back(CompileImageUsage{
                    .pass        = dummy_pass,
                    .stages      = vk::PipelineStageFlagBits2KHR::eNone,
                    .access      = vk::AccessFlagBits2KHR::eNone,
                    .layout      = signal.layout,
                    .exit_layout = signal.layout
                });
                dummy_pass->generated_image_usages_[rsc] =
                    record.usages.back();

                image_final_states_[rsc] = UsingState{
                    .queue  = dummy_pass->queue,
                    .stages = vk::PipelineStageFlagBits2KHR::eNone,
                    .access = vk::AccessFlagBits2KHR::eNone,
                    .layout = signal.layout
                };
            }
        }
    }
}

void SemaphoreSignalHandler::processSignalingSemaphore(
    Semaphore                   &semaphore,
    const List<CompileResource> &resources,
    ResourceRecords             &resource_records,
    const Graph                 &graph)
{
    assert(!resources.empty());

    Set<CompilePass *> passes(&memory_);
    for(auto &resource : resources)
    {
        resource.match(
            [&](const Buffer &buffer)
        {
            auto &record = resource_records.getRecord(buffer);
            record.has_signal_semaphore = true;
            passes.insert(record.usages.back().pass);
        },
            [&](const ImageSubresource &image)
        {
            auto &record = resource_records.getRecord(image);
            record.has_signal_semaphore = true;
            passes.insert(record.usages.back().pass);
        });
    }

    Map<CompilePass *, CompilePass *> necessary_passes(&memory_);
    for(auto it = passes.rbegin(); it != passes.rend(); ++it)
    {
        auto pass = *it;
        necessary_passes.insert({ pass, pass });

        for(auto jt = std::next(it), njt = jt; jt != passes.rend(); jt = njt)
        {
            ++njt;
            if(closure_->isReachable((*jt)->sorted_index, pass->sorted_index))
            {
                necessary_passes.insert({ *jt, pass });
                passes.erase(std::next(jt).base());
            }
        }
    }

    Set<CompilePass *>                emit_passes(&memory_);
    Map<const Queue *, CompilePass *> queue_to_dummy_pass(&memory_);

    for(auto &resource : resources)
    {
        resource.match([&](const auto &rsc)
        {
            processSignalingSemaphore(
                rsc, resource_records, emit_passes,
                necessary_passes, queue_to_dummy_pass, graph);
        });
    }

    assert(!emit_passes.empty());
    if(emit_passes.size() == 1)
    {
        auto &submit = (*emit_passes.begin())->signal_semaphores[semaphore];
        semaphore.match(
            [&](const BinarySemaphore &binary)
        {
            submit.semaphore = binary;
        },
            [&](TimelineSemaphore &timeline)
        {
            submit.semaphore = timeline;
            submit.value = timeline.nextSignalValue();
        });
    }
    else
    {
        auto resolve_queue = [](const Queue *a, const Queue *b)
        {
            if(a->getFamilyIndex() == b->getFamilyIndex())
                return a;
            return a->getType() < b->getType() ? a : b;
        };

        const Queue *signal_queue = (*emit_passes.begin())->queue;
        for(auto it = std::next(emit_passes.begin());
            it != emit_passes.end(); ++it)
        {
            signal_queue = resolve_queue(signal_queue, (*it)->queue);
        }

        auto signal_pass = create_post_pass_();
        signal_pass->queue = signal_queue;

        for(auto p : emit_passes)
        {
            p->tails.insert(signal_pass);
            signal_pass->heads.insert(p);
        }

        auto &submit = signal_pass->signal_semaphores[semaphore];
        semaphore.match(
            [&](const BinarySemaphore &binary)
        {
            submit.semaphore = binary;
            submit.stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands;
        },
            [&](TimelineSemaphore &timeline)
        {
            submit.semaphore = timeline;
            submit.stageMask = vk::PipelineStageFlagBits2KHR::eAllCommands;
            submit.value = timeline.nextSignalValue();
        });
    }
}

VKPT_GRAPH_END
