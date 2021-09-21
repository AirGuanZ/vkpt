#pragma once

#include <agz-utils/misc.h>

#include <vkpt/common.h>

VKPT_BEGIN

template<typename O, typename OStorage, typename Description>
class Object
{
public:

    Object() = default;

    Object(OStorage object, Description desc);

    template<typename Deleter>
    Object(OStorage object, Description desc, Deleter deleter);

    operator O() const;

    operator bool() const;

    O get() const;

    const auto &getDescription() const;

    void reset();

    auto operator<=>(const Object &rhs) const;

protected:

    struct Record
    {
        OStorage    object;
        Description description;
    };

    std::shared_ptr<Record> record_;
};

#define VKPT_USING_OBJECT_METHODS(O)                                            \
    using Object::operator O;                                                   \
    using Object::operator bool;                                                \
    using Object::get;                                                          \
    using Object::getDescription;                                               \
    using Object::reset

template<typename O, typename OStorage, typename Description>
Object<O, OStorage, Description>::Object(OStorage object, Description desc)
{
    record_ = std::make_shared<Record>(
        Record{ std::move(object), std::move(desc) });
}

template<typename O, typename OStorage, typename Description>
template<typename Deleter>
Object<O, OStorage, Description>::Object(
    OStorage object, Description desc, Deleter deleter)
{
    auto record = new Record{ std::move(object), std::move(desc) };
    agz::misc::fixed_scope_guard_t guard([&]
    {
        deleter(record);
    });
    record_ = std::shared_ptr<Record>(record, deleter);
    guard.dismiss();
}

template<typename O, typename OStorage, typename Description>
Object<O, OStorage, Description>::operator O() const
{
    return this->get();
}

template<typename O, typename OStorage, typename Description>
O Object<O, OStorage, Description>::get() const
{
    if constexpr(std::is_same_v<O, OStorage>)
    {
        return record_->object;
    }
    else
    {
        return record_->object.get();
    }
}

template<typename O, typename OStorage, typename Description>
Object<O, OStorage, Description>::operator bool() const
{
    return this->get();
}

template<typename O, typename OStorage, typename Description>
const auto &Object<O, OStorage, Description>::getDescription() const
{
    return record_->description;
}

template<typename O, typename OStorage, typename Description>
void Object<O, OStorage, Description>::reset()
{
    record_.reset();
}

template<typename O, typename OStorage, typename Description>
auto Object<O, OStorage, Description>::operator<=>(const Object &rhs) const
{
    return this->get() <=> rhs.get();
}

VKPT_END
