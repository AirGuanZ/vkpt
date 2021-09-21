#pragma once

#include <vkpt/graph/graph.h>

VKPT_RENDER_GRAPH_BEGIN

template<typename UsageData>
class ResourceStateTracer
{
public:

    struct Usage : UsageData
    {
        Pass *pass;
    };

    explicit ResourceStateTracer(std::pmr::memory_resource &memory)
        : usages_(&memory), last_usages_(&memory)
    {

    }

    void addUsage(Pass *pass, const UsageData &usage_data)
    {
        Usage *last = usages_.empty() ? nullptr : &usages_.back();

        Usage new_usage;
        static_cast<UsageData&>(new_usage) = usage_data;
        new_usage.pass = pass;

        usages_.push_back(new_usage);
        last_usages_[pass] = last;
    }

    bool isInitialUsage(Pass *pass)
    {
        return usages_.front().pass == pass;
    }

    bool isFinalUsage(Pass *pass)
    {
        return usages_.back().pass == pass;
    }

    const Usage &getLastUsage(Pass *pass) const
    {
        assert(last_usages_.contains(pass));
        return *last_usages_.at(pass);
    }

private:

    List<Usage>          usages_;
    Map<Pass *, Usage *> last_usages_;
};

using BufferStateTracer = ResourceStateTracer<Pass::BufferUsage>;
using ImageStateTracer  = ResourceStateTracer<Pass::ImageUsage>;

VKPT_RENDER_GRAPH_END
