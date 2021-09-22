#pragma once

#include <vkpt/graph/graph.h>

VKPT_RENDER_GRAPH_BEGIN

class TransientResourceManager : public agz::misc::uncopyable_t
{
public:

    explicit TransientResourceManager(ResourceAllocator &resource_allocator);


};

VKPT_RENDER_GRAPH_END
