#pragma once

#include <vkpt/graph/compile_internal.h>

VKPT_GRAPH_BEGIN

class GroupBarrierOptimizer
{
public:

    static bool isReadOnly(vk::AccessFlags2KHR access);
    
    void optimize(CompileGroup *group);

private:

    void movePreExtBarriers(CompileGroup *group);

    void movePostExtBarriers(CompileGroup *group);

    void mergePreAndPostBarriers(CompileGroup *group);
};

VKPT_GRAPH_END
