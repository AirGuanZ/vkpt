#pragma once

#include <vkpt/vulkan/queue.h>

VKPT_BEGIN

class FrameSynchronizer : public agz::misc::uncopyable_t
{
public:

    FrameSynchronizer(vk::Device device, int frame_count);

    ~FrameSynchronizer();

    void newFrame();

    void endFrame(vk::ArrayProxy<const Queue> queues);

    void executeAfterSync(std::function<void()> func);

    template<typename T>
    void destroyAfterSync(T obj);

    void _triggerAllSync();

private:

    struct FenceInfo
    {
        bool will_be_signaled = false;
        std::vector<vk::Fence>       used_fences;
        std::vector<vk::UniqueFence> fences;

        std::vector<std::function<void()>> delayed_functions;
    };

    void wait(FenceInfo &info);

    vk::Device device_;

    int frame_index_;
    std::vector<FenceInfo> fence_info_;
};

template<typename T>
void FrameSynchronizer::destroyAfterSync(T obj)
{
    this->executeAfterSync([o = std::move(obj)]{});
}

VKPT_END
