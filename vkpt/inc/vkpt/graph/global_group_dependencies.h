#pragma once

#include <vkpt/graph/compile_internal.h>

VKPT_GRAPH_BEGIN

class GlobalGroupDependencyLUT
{
public:

    explicit GlobalGroupDependencyLUT(std::pmr::memory_resource &memory);

    const GlobalGroupDependency &getDependency(
        const CompileGroup *start, const CompileGroup *end);

private:

    using Dependencies = Map<
        const CompileGroup *,
        Map<const CompileGroup *, GlobalGroupDependency>>;

    std::pmr::memory_resource &memory_;
    Dependencies               dependencies_;
    int                        traversal_index_ = 0;
};

VKPT_GRAPH_END
