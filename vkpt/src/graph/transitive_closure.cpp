#include <vkpt/graph/transitive_closure.h>

VKPT_GRAPH_BEGIN

namespace
{

    bool getBit(uint32_t bits, int index)
    {
        return bits & (1 << index);
    }

    void setBit(uint32_t &bits, int index)
    {
        bits |= (1 << index);
    }

} // namespace anonymous

DAGTransitiveClosure::DAGTransitiveClosure(
    int vertex_count, std::pmr::memory_resource &memory)
    : bitmap_(&memory), direct_neighbors_(&memory)
{
    line_ = agz::upalign_to(vertex_count, 32) / 32;

    bitmap_.resize(line_ * vertex_count);
    for(int i = 0; i < vertex_count; ++i)
    {
        uint32_t &bits = bitmap_[i * line_ + i / 32];
        setBit(bits, i % 32);
    }

    direct_neighbors_.resize(vertex_count, Vector<int>(&memory));
}

void DAGTransitiveClosure::addArc(int head, int tail)
{
    uint32_t &bits = bitmap_[head * line_ + tail / 32];
    setBit(bits, tail % 32);
    direct_neighbors_[head].push_back(tail);
}

void DAGTransitiveClosure::computeClosure()
{
    for(int head = static_cast<int>(bitmap_.size() - 1); head >= 0; --head)
    {
        uint32_t *head_line = &bitmap_[head * line_];
        for(int tail : direct_neighbors_[head])
        {
            uint32_t *tail_line = &bitmap_[tail * line_];
            for(int i = 0; i < line_; ++i)
                head_line[i] |= tail_line[i];
        }
    }
}

bool DAGTransitiveClosure::isReachable(int start, int end) const
{
    const uint32_t bits = bitmap_[start * line_ + end / 32];
    return getBit(bits, end % 32);
}

VKPT_GRAPH_END
