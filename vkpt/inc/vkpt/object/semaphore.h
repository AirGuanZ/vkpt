#pragma once

#include <agz-utils/misc.h>

#include <vkpt/common.h>

VKPT_BEGIN

class BinarySemaphore;
class TimelineSemaphore;

using Semaphore = agz::misc::variant_t<BinarySemaphore, TimelineSemaphore>;

class BinarySemaphore
{
public:

    BinarySemaphore() = default;

    explicit BinarySemaphore(vk::UniqueSemaphore semaphore);

    operator bool() const;

    operator vk::Semaphore() const;

    vk::Semaphore get() const;

    std::strong_ordering operator<=>(const BinarySemaphore &rhs) const;

private:

    std::shared_ptr<vk::UniqueSemaphore> impl_;
};

class TimelineSemaphore
{
public:

    TimelineSemaphore() = default;

    TimelineSemaphore(vk::UniqueSemaphore semaphore, uint64_t initial_value);

    operator bool() const;

    operator vk::Semaphore() const;

    vk::Semaphore get() const;

    uint64_t getLastSignalValue() const;

    uint64_t nextSignalValue();

    std::strong_ordering operator<=>(const BinarySemaphore &rhs) const;

private:

    struct Impl
    {
        vk::UniqueSemaphore semaphore;
        uint64_t            current_value;
    };

    std::shared_ptr<Impl> impl_;
};

inline vk::Semaphore getRaw(const Semaphore &semaphore)
{
    return semaphore.match(
        [&](const BinarySemaphore   &b) { return b.get(); },
        [&](const TimelineSemaphore &t) { return t.get(); });
}

inline BinarySemaphore::BinarySemaphore(vk::UniqueSemaphore semaphore)
{
    impl_ = std::make_shared<vk::UniqueSemaphore>(std::move(semaphore));
}

inline BinarySemaphore::operator bool() const
{
    return impl_ != nullptr;
}

inline BinarySemaphore::operator vk::Semaphore() const
{
    return get();
}

inline vk::Semaphore BinarySemaphore::get() const
{
    return impl_ ? impl_->get() : nullptr;
}

inline std::strong_ordering BinarySemaphore::operator<=>(
    const BinarySemaphore &rhs) const
{
    return get() <=> rhs.get();
}

inline TimelineSemaphore::TimelineSemaphore(
    vk::UniqueSemaphore semaphore, uint64_t initial_value)
{
    impl_ = std::make_shared<Impl>();
    impl_->semaphore.swap(semaphore);
    impl_->current_value = initial_value;
}

inline TimelineSemaphore::operator bool() const
{
    return impl_ != nullptr;
}

inline TimelineSemaphore::operator vk::Semaphore() const
{
    return get();
}

inline vk::Semaphore TimelineSemaphore::get() const
{
    return impl_ ? impl_->semaphore.get() : nullptr;
}

inline uint64_t TimelineSemaphore::getLastSignalValue() const
{
    assert(impl_);
    return impl_->current_value;
}

inline uint64_t TimelineSemaphore::nextSignalValue()
{
    assert(impl_);
    return ++impl_->current_value;
}

inline std::strong_ordering TimelineSemaphore::operator<=>(
    const BinarySemaphore &rhs) const
{
    return get() <=> rhs.get();
}

VKPT_END
