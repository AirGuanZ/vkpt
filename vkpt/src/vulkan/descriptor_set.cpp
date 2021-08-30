#include <vkpt/vulkan/descriptor_set.h>

VKPT_BEGIN

DescriptorSet::DescriptorSet()
    : set_(nullptr), manager_index_(0), manager_(nullptr)
{
#ifdef VKPT_DEBUG
    id_ = 0;
#endif
}

DescriptorSet::DescriptorSet(DescriptorSet &&other) noexcept
    : DescriptorSet()
{
    swap(other);
}

DescriptorSet &DescriptorSet::operator=(DescriptorSet &&other) noexcept
{
    swap(other);
    return *this;
}

DescriptorSet::~DescriptorSet()
{
    if(manager_)
        manager_->_freeSet(*this);
}

void DescriptorSet::swap(DescriptorSet &other) noexcept
{
    std::swap(set_, other.set_);
    std::swap(manager_index_, other.manager_index_);
    std::swap(manager_, other.manager_);
#ifdef VKPT_DEBUG
    std::swap(id_, other.id_);
#endif
}

DescriptorSet::operator bool() const
{
    return set_;
}

vk::DescriptorSet DescriptorSet::get()
{
    return set_;
}

DescriptorSet::DescriptorSet(
    vk::DescriptorSet     set,
    uint32_t              manager_index,
    DescriptorSetManager *manager)
    : set_(std::move(set)), manager_index_(manager_index), manager_(manager)
{
#ifdef VKPT_DEBUG
    id_ = 0;
#endif
}

DescriptorSetLayout::DescriptorSetLayout()
    : layout_(nullptr), manager_index_(0), manager_(nullptr)
{
#ifdef VKPT_DEBUG
    id_ = 0;
#endif
}

DescriptorSetLayout::DescriptorSetLayout(const DescriptorSetLayout &other)
    : layout_(other.layout_),
      manager_index_(other.manager_index_),
      manager_(other.manager_)
{
#ifdef VKPT_DEBUG
    id_ = other.id_;
#endif
    if(manager_)
        manager_->_incLayoutRefCount(manager_index_);
}

DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout &&other) noexcept
    : DescriptorSetLayout()
{
    swap(other);
}

DescriptorSetLayout &DescriptorSetLayout::operator=(
    const DescriptorSetLayout &other)
{
    DescriptorSetLayout temp(other);
    swap(temp);
    return *this;
}

DescriptorSetLayout &DescriptorSetLayout::operator=(
    DescriptorSetLayout &&other) noexcept
{
    swap(other);
    return *this;
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    if(manager_)
        manager_->_decLayoutRefCount(manager_index_);
}

void DescriptorSetLayout::swap(DescriptorSetLayout &other) noexcept
{
    std::swap(layout_, other.layout_);
    std::swap(manager_index_, other.manager_index_);
    std::swap(manager_, other.manager_);
#ifdef VKPT_DEBUG
    std::swap(id_, other.id_);
#endif
}

DescriptorSetLayout::operator bool() const
{
    return layout_;
}

vk::DescriptorSetLayout DescriptorSetLayout::get() const
{
    return layout_;
}

DescriptorSet DescriptorSetLayout::createSet()
{
    assert(manager_);
    return manager_->createSet(*this);
}

DescriptorSetLayout::DescriptorSetLayout(
    vk::DescriptorSetLayout layout,
    uint32_t                manager_index,
    DescriptorSetManager   *manager)
    : layout_(layout), manager_index_(manager_index), manager_(manager)
{
#ifdef VKPT_DEBUG
    id_ = 0;
#endif
}

DescriptorSetManager::DescriptorSetManager(vk::Device device)
    : device_(device)
{
#ifdef VKPT_DEBUG
    next_layout_id_ = 0;
#endif
}

DescriptorSetLayout DescriptorSetManager::createLayout(
    const DescriptorSetLayoutDescription &desc)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(desc.bindings.size());
    for(auto &b : desc.bindings)
    {
        bindings.push_back(vk::DescriptorSetLayoutBinding{
            .binding            = b.binding,
            .descriptorType     = b.type,
            .descriptorCount    = b.count,
            .stageFlags         = b.stage_flags,
            .pImmutableSamplers = b.immutable_samplers.data()
        });
    }

    vk::DescriptorSetLayoutCreateInfo create_info = {
        .flags        = desc.flags,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings    = bindings.data()
    };
    return createLayout(create_info);
}

DescriptorSetLayout DescriptorSetManager::createLayout(
    const vk::DescriptorSetLayoutCreateInfo &create_info)
{
    if(auto it = info_to_layout_.find(create_info); it != info_to_layout_.end())
    {
        const uint32_t index = it->second;
        auto &record = records_[index];
        record.ref_count++;
        return DescriptorSetLayout(record.layout.get(), index, this);
    }

    vk::UniqueDescriptorSetLayout vk_layout =
        device_.createDescriptorSetLayoutUnique(create_info);

    // allocate new record

    uint32_t new_index;
    if(!free_records_.empty())
    {
        new_index = free_records_.back();
        free_records_.pop_back();
    }
    else
    {
        new_index = static_cast<uint32_t>(records_.size());
        records_.emplace_back();
    }
    
    auto info_to_layout_iterator =
        info_to_layout_.insert({ create_info, new_index }).first;

    // fill new_record

    auto &new_record = records_[new_index];
    new_record.ref_count               = 1;
    new_record.layout                  = std::move(vk_layout);
    new_record.info_to_layout_iterator = info_to_layout_iterator;
#ifdef VKPT_DEBUG
    new_record.id                      = next_layout_id_++;
#endif

    // fill pool_create_info

    constexpr uint32_t set_count = 16;

    std::map<vk::DescriptorType, uint32_t> descriptor_counts;
    for(uint32_t i = 0; i < create_info.bindingCount; ++i)
    {
        auto &binding = create_info.pBindings[i];
        descriptor_counts[binding.descriptorType] += binding.descriptorCount;
    }
    const uint32_t pool_size_count =
        static_cast<uint32_t>(descriptor_counts.size());

    new_record.pool_sizes.clear();
    for(auto &p : descriptor_counts)
    {
        new_record.pool_sizes.push_back(vk::DescriptorPoolSize{
            .type            = p.first,
            .descriptorCount = p.second * set_count
        });
    }

    new_record.pool_create_info.flags         = {};
    new_record.pool_create_info.maxSets       = set_count;
    new_record.pool_create_info.poolSizeCount = pool_size_count;
    new_record.pool_create_info.pPoolSizes    = new_record.pool_sizes.data();

    // create set layout

    auto result = DescriptorSetLayout(new_record.layout.get(), new_index, this);
#ifdef VKPT_DEBUG
    result.id_ = new_record.id;
#endif
    return result;
}

DescriptorSet DescriptorSetManager::createSet(const DescriptorSetLayout &layout)
{
    assert(layout.layout_);
    assert(layout.manager_ == this);

    auto &record = records_[layout.manager_index_];
    assert(record.id == layout.id_);

    if(!record.free_sets.empty())
    {
        auto result = std::move(record.free_sets.back());
        record.free_sets.pop_back();
        return result;
    }
    
    auto new_pool = device_.createDescriptorPoolUnique(record.pool_create_info);

    const uint32_t new_set_count = record.pool_create_info.maxSets;
    std::vector new_set_layouts(new_set_count, layout.get());
    
    auto new_sets = device_.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
        .descriptorPool     = new_pool.get(),
        .descriptorSetCount = new_set_count ,
        .pSetLayouts        = new_set_layouts.data()
    });

    record.pools.push_back(std::move(new_pool));
    for(auto &s : new_sets)
    {
        auto set = DescriptorSet(s, layout.manager_index_, this);
#ifdef VKPT_DEBUF
        set.id_ = record.id;
#endif
        record.free_sets.push_back(std::move(set));
    }

    return createSet(layout);
}

void DescriptorSetManager::_incLayoutRefCount(uint32_t index)
{
    auto &record = records_[index];
    ++record.ref_count;
}

void DescriptorSetManager::_decLayoutRefCount(uint32_t index)
{
    auto &record = records_[index];
    if(--record.ref_count)
        return;

    for(auto &p : record.pools)
        device_.resetDescriptorPool(p.get());

    info_to_layout_.erase(record.info_to_layout_iterator);

    record.layout.reset();
    record.pool_sizes.clear();
    record.pool_create_info = vk::DescriptorPoolCreateInfo{};
    record.free_sets.clear();
    record.pools.clear();
    record.info_to_layout_iterator = {};

    free_records_.push_back(index);
}

void DescriptorSetManager::_freeSet(DescriptorSet &set)
{
    auto &record = records_[set.manager_index_];
    assert(record.id == set.id_);
    record.free_sets.push_back(std::move(set));
}

VKPT_END
