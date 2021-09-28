#pragma once

#include <memory_resource>

#include <vkpt/graph/common.h>

VKPT_GRAPH_BEGIN

class DAGTransitiveClosure
{
public:

    DAGTransitiveClosure(
        int vertex_count, std::pmr::memory_resource &memory);

    void addArc(int head, int tail);

    void computeClosure();

    bool isReachable(int start, int end) const;

private:

    // bitmap_[head * line].bits[tail] stores isReachable(head, tail)

    int              line_;
    Vector<uint32_t> bitmap_;

    Vector<Vector<int>> direct_neighbors_;
};

VKPT_GRAPH_END
