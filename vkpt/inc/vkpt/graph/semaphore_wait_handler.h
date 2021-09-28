#pragma once

#include <vkpt/graph/transitive_closure.h>
#include <vkpt/graph/resource_records.h>

VKPT_GRAPH_BEGIN

class SemaphoreWaitHandler : public agz::misc::uncopyable_t
{
public:
    
    SemaphoreWaitHandler(
        std::pmr::memory_resource    &memory,
        Messenger                    *messenger,
        const DAGTransitiveClosure   *closure,
        std::function<CompilePass*()> create_pre_pass);

    void collectWaitingSemaphores(const Graph &graph);

    void processWaitingSemaphores(ResourceRecords &resource_records);

private:

    template<typename Resource>
    void processResourceWait(
        const BinarySemaphore                &binary,
        const Resource                       &rsc,
        ResourceRecords                      &resource_records);
    
    template<typename Resource>
    void processResourceWait(
        const TimelineSemaphore               &timeline,
        const Resource                        &rsc,
        ResourceRecords                       &resource_records,
        const Map<CompilePass*, CompilePass*> &necessary_passes);
    
    void processWaitingSemaphore(
        const BinarySemaphore                &binary,
        const List<CompileResource>          &resources,
        ResourceRecords                      &resource_records);
    
    void processWaitingSemaphore(
        const TimelineSemaphore              &timeline,
        const List<CompileResource>          &resources,
        ResourceRecords                      &resource_records);

    std::pmr::memory_resource            &memory_;
    Map<Semaphore, List<CompileResource>> waiting_semaphore_to_resources_;

    Messenger                     *messenger_;
    const DAGTransitiveClosure    *closure_;
    std::function<CompilePass *()> create_pre_pass_;
};

VKPT_GRAPH_END
