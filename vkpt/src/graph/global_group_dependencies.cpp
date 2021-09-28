#include <vkpt/graph/global_group_dependencies.h>

VKPT_GRAPH_BEGIN

GlobalGroupDependencyLUT::GlobalGroupDependencyLUT(
    std::pmr::memory_resource &memory)
    : memory_(memory), dependencies_(&memory)
{
    
}

const GlobalGroupDependency &GlobalGroupDependencyLUT::getDependency(
    const CompileGroup *start, const CompileGroup *end)
{
    if(auto it = dependencies_.find(start);
       it != dependencies_.end())
        return it->second.at(end);

    auto &map = dependencies_.insert(
        { start, Map<const CompileGroup *, GlobalGroupDependency>(&memory_) })
        .first->second;

    struct DependencySearchInfo
    {
        const CompileGroup *start_exit_tail;
        const CompileGroup *end_entry_head;
        CompileGroup *current;
    };

    const int traversal_flag = -++traversal_index_;
    auto set_flag = [traversal_flag](CompileGroup *group)
    {
        assert(group->unprocessed_head_count != traversal_flag);
        group->unprocessed_head_count = traversal_flag;
    };
    auto test_flag = [traversal_flag](const CompileGroup *group)
    {
        return group->unprocessed_head_count == traversal_flag;
    };

    PmrQueue<DependencySearchInfo> next_groups(&memory_);
    for(auto tail : start->tails)
    {
        set_flag(tail);
        next_groups.push({ tail, start, tail });
    }

    while(!next_groups.empty())
    {
        auto info = next_groups.front();
        next_groups.pop();

        assert(test_flag(info.current));
        map.insert(
            { info.current, { info.start_exit_tail, info.end_entry_head } });

        for(auto tail : info.current->tails)
        {
            if(!test_flag(tail))
            {
                set_flag(tail);
                next_groups.push({ info.start_exit_tail, info.current, tail });
            }
        }
    }

    return map.at(end);
}

VKPT_GRAPH_END
