#pragma once

#include <vkpt/graph/compile_internal.h>
#include <vkpt/graph/executor.h>
#include <vkpt/graph/resource_records.h>
#include <vkpt/graph/transitive_closure.h>

VKPT_GRAPH_BEGIN

class Compiler : protected Messenger
{
public:

    Compiler();

    void setMessenger(std::function<void(const std::string &)> func);

    ExecutableGraph compile(
        SemaphoreAllocator &semaphore_allocator,
        const Graph        &graph);

private:

    void initializeCompilePasses(const Graph &graph);

    void topologySortCompilePasses();
    
    void buildTransitiveClosure();

    void collectResourceUsages();

    template<typename Rsc>
    void processUnwaitedFirstUsage(const Rsc &resource);

    template<typename Rsc>
    void processUnsignaledFinalState(const Rsc &rsc);

    void mergeGeneratedPreAndPostPasses();

    template<bool Reverse>
    void addGroupFlag(CompilePass *entry, size_t bit_index);

    void generateGroupFlags();

    List<CompileGroup *> generateGroups();

    void fillInterGroupArcs(const List<CompileGroup *> &groups);

    void topologySortGroups(const List<CompileGroup *> &groups);

    void topologySortPassesInGroup(CompileGroup *group);

    void fillInterGroupSemaphores(SemaphoreAllocator &allocator);

    void buildPassToUsage(CompileBuffer &record);

    void buildPassToUsage(CompileImage &record);

    void fillExecutableGroup(
        const CompileGroup &group, ExecutableGroup &output);

    std::pmr::unsynchronized_pool_resource memory_;
    agz::alloc::obj_arena_t                arena_;

    Set<CompilePass *>    compile_passes_;
    Vector<CompilePass *> sorted_compile_passes_;

    Vector<CompileGroup *> compile_groups_;

    DAGTransitiveClosure *closure_;

    List<CompilePass *> generated_pre_passes_;
    List<CompilePass *> generated_post_passes_;

    ResourceRecords resource_records_;

    Map<Buffer, ResourceState>           buffer_final_states_;
    Map<ImageSubresource, ResourceState> image_final_states_;
};

VKPT_GRAPH_END
