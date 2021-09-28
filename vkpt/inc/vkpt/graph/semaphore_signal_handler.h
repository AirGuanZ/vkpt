#pragma once

#include <vkpt/graph/resource_records.h>
#include <vkpt/graph/transitive_closure.h>

VKPT_GRAPH_BEGIN

class SemaphoreSignalHandler : public agz::misc::uncopyable_t
{
public:

    SemaphoreSignalHandler(
        std::pmr::memory_resource            &memory,
        const DAGTransitiveClosure           *closure,
        Map<Buffer, ResourceState>           &buffer_final_states,
        Map<ImageSubresource, ResourceState> &image_final_states,
        std::function<CompilePass*()>         create_post_pass);

    void collectSignalingSemaphores(const Graph &graph);
    
    void processSignalingSemaphores(
        ResourceRecords &resource_records,
        const Graph     &graph);

private:

    template<typename Resource>
    void processSignalingSemaphore(
        const Resource                    &rsc,
        ResourceRecords                   &resource_records,
        Set<CompilePass *>                &emit_passes,
        Map<CompilePass *, CompilePass *> &necessary_passes,
        Map<const Queue *, CompilePass *> &queue_to_dummy_pass,
        const Graph                       &graph);
    
    void processSignalingSemaphore(
        Semaphore                   &semaphore,
        const List<CompileResource> &resources,
        ResourceRecords             &resource_records,
        const Graph                 &graph);

    std::pmr::memory_resource            &memory_;
    Map<Semaphore, List<CompileResource>> signaling_semaphore_to_resources_;

    const DAGTransitiveClosure           *closure_;
    Map<Buffer, ResourceState>           &buffer_final_states_;
    Map<ImageSubresource, ResourceState> &image_final_states_;
    std::function<CompilePass *()>        create_post_pass_;
};

VKPT_GRAPH_END
