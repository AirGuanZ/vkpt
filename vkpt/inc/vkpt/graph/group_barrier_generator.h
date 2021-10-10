#pragma once

#include <vkpt/graph/global_group_dependencies.h>
#include <vkpt/graph/resource_records.h>

VKPT_GRAPH_BEGIN

class GroupBarrierGenerator
{
public:

    GroupBarrierGenerator(
        std::pmr::memory_resource &memory,
        const ResourceRecords     &resource_records);

    void fillBarriers(CompileGroup *group);

private:

    // returns the closest pass to B that:
    //    1. is after A
    //    2. has one or more barriers
    // if no such pass exists, returns B
    CompilePass *getBarrierPass(CompilePass *A, CompilePass *B);

    bool shouldSkipBarrier(
        const Pass::BufferUsage &a, const Pass::BufferUsage &b) const;

    bool shouldSkipBarrier(
        const Pass::ImageUsage &a, const Pass::ImageUsage &b) const;

    template<typename Resource, typename PassUsage>
    void handleResource(
        CompilePass *pass, const Resource &rsc, const PassUsage &usage);

    GlobalGroupDependencyLUT dependencies_;
    const ResourceRecords   &resource_records_;

    CompilePass *last_pass_with_pre_barrier_ = nullptr;
};

VKPT_GRAPH_END
