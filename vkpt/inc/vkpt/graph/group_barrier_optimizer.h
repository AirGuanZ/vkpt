#pragma once

#include <vkpt/graph/resource_records.h>

VKPT_GRAPH_BEGIN

class GroupBarrierOptimizer
{
public:

    static bool isReadOnly(vk::AccessFlags2KHR access);

    void optimize(ResourceRecords &records);
    
    void optimize(CompileGroup *group);

private:

    void mergeNeighboringUsages(CompileBuffer &record);

    void mergeNeighboringUsages(CompileImage &record);

    void movePreExtBarriers(CompileGroup *group);

    void movePostExtBarriers(CompileGroup *group);

    void mergePreAndPostBarriers(CompileGroup *group);
};

VKPT_GRAPH_END
