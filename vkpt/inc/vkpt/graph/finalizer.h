#pragma once

#include <vkpt/graph/graph.h>

VKPT_RENDER_GRAPH_BEGIN

class Finalizer
{
public:

    Finalizer();

    void finalize(Graph &graph);

private:

    struct BufferRecord
    {
        Pass             *users[4] = {};
        Pass::BufferUsage usages[4] = {};
    };

    struct ImageRecord
    {
        Pass            *users[4] = {};
        Pass::ImageUsage usages[4] = {};
    };

    Vector<Pass *> topologySortPasses(Graph &graph);

    void collectResourceUsers();

    bool isUnnecessaryPrepass(Pass *pass);

    bool isUnnecessaryPostpass(Pass *pass);

    void removeUnnecessaryPrepasses(Graph &graph);

    void removeUnnecessaryPostpasses(Graph &graph);

    agz::alloc::memory_resource_arena_t memory_;

    Vector<Pass *> sorted_passes_;
    Vector<bool>   removed_;

    Map<vk::Buffer, BufferRecord>                                buffers_;
    Map<std::pair<vk::Image, vk::ImageSubresource>, ImageRecord> images_;
};

VKPT_RENDER_GRAPH_END
