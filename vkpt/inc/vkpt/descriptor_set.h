#pragma once

#include <map>

#include <vkpt/common.h>

VKPT_BEGIN

class DescriptorSetManager;

class DescriptorSet : public agz::misc::uncopyable_t
{
public:

    DescriptorSet();

    DescriptorSet(DescriptorSet &&other) noexcept;

    DescriptorSet &operator=(DescriptorSet &&other) noexcept;

    ~DescriptorSet();

    void swap(DescriptorSet &other) noexcept;

    operator bool() const;

    vk::DescriptorSet get();

private:

    friend class DescriptorSetManager;

    DescriptorSet(
        vk::DescriptorSet     set,
        uint32_t              manager_index,
        DescriptorSetManager *manager);

    vk::DescriptorSet     set_;
    uint32_t              manager_index_;
    DescriptorSetManager *manager_;

#ifdef VKPT_DEBUG
    uint32_t id_;
#endif
};

class DescriptorSetLayout
{
public:

    DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout &other);

    DescriptorSetLayout(DescriptorSetLayout &&other) noexcept;

    DescriptorSetLayout &operator=(const DescriptorSetLayout &other);

    DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept;

    ~DescriptorSetLayout();

    void swap(DescriptorSetLayout &other) noexcept;

    operator bool() const;

    vk::DescriptorSetLayout get() const;

    DescriptorSet createSet();

private:

    friend class DescriptorSetManager;

    DescriptorSetLayout(
        vk::DescriptorSetLayout     layout,
        uint32_t                    manager_index,
        DescriptorSetManager       *manager);

    vk::DescriptorSetLayout layout_;
    uint32_t                manager_index_;
    DescriptorSetManager   *manager_;

#ifdef VKPT_DEBUG
    uint32_t id_;
#endif
};

struct DescriptorSetLayoutBinding
{
    uint32_t                 binding     = {};
    vk::DescriptorType       type        = {};
    uint32_t                 count       = {};
    vk::ShaderStageFlags     stage_flags = {};
    std::vector<vk::Sampler> immutable_samplers;
};

struct DescriptorSetLayoutDescription
{
    vk::DescriptorSetLayoutCreateFlags      flags = {};
    std::vector<DescriptorSetLayoutBinding> bindings;
};

class DescriptorSetManager
{
public:

    explicit DescriptorSetManager(vk::Device device);

    DescriptorSetLayout createLayout(const DescriptorSetLayoutDescription &desc);

    DescriptorSet createSet(const DescriptorSetLayout &layout);

    void _incLayoutRefCount(uint32_t index);

    void _decLayoutRefCount(uint32_t index);

    void _freeSet(DescriptorSet &set);

private:

    DescriptorSetLayout createLayout(
        const vk::DescriptorSetLayoutCreateInfo &create_info);

    using InfoToLayout = std::map<vk::DescriptorSetLayoutCreateInfo, uint32_t>;

    struct Record
    {
        uint32_t ref_count = 0;

#ifdef VKPT_DEBUG
        uint32_t id = 0;
#endif

        vk::UniqueDescriptorSetLayout layout;

        std::vector<vk::DescriptorPoolSize>   pool_sizes;
        vk::DescriptorPoolCreateInfo          pool_create_info;
        std::vector<vk::UniqueDescriptorPool> pools;
        std::vector<DescriptorSet>            free_sets;

        InfoToLayout::iterator info_to_layout_iterator;
    };

    vk::Device device_;

    InfoToLayout info_to_layout_;

#ifdef VKPT_DEBUG
    uint32_t next_layout_id_;
#endif

    std::vector<Record>   records_;
    std::vector<uint32_t> free_records_;
};

VKPT_END
