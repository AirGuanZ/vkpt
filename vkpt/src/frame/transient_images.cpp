#include <vkpt/frame/transient_images.h>

VKPT_BEGIN

TransientImages::TransientImages()
    : resource_allocator_(nullptr), frame_index_(0), frame_count_(0)
{
    
}

TransientImages::TransientImages(
    ResourceAllocator &resource_allocator, int frame_count)
    : resource_allocator_(&resource_allocator),
      frame_index_(0), frame_count_(frame_count)
{

}

TransientImages::TransientImages(TransientImages &&other) noexcept
    : TransientImages()
{
    swap(other);
}

TransientImages &TransientImages::operator=(TransientImages &&other) noexcept
{
    swap(other);
    return *this;
}

TransientImages::operator bool() const
{
    return resource_allocator_ != nullptr;
}

void TransientImages::swap(TransientImages &other) noexcept
{
    std::swap(resource_allocator_, other.resource_allocator_);
    std::swap(frame_index_, other.frame_index_);
    std::swap(frame_count_, other.frame_count_);
    std::swap(all_images_, other.all_images_);
    std::swap(unused_images_, other.unused_images_);
}

void TransientImages::newFrame()
{
    for(auto it = all_images_.begin(), jt = it; it != all_images_.end(); it = jt)
    {
        ++jt;
        if(frame_index_ - it->second->last_used_frame_index > frame_count_)
            all_images_.erase(it);
    }
    unused_images_ = all_images_;
    ++frame_index_;
}

Image TransientImages::newImage(const vk::ImageCreateInfo &create_info, vma::MemoryUsage usage)
{
    auto it = unused_images_.find({ create_info, usage });
    if(it != unused_images_.end())
    {
        it->second->last_used_frame_index = frame_index_;
        auto ret = it->second->image;
        unused_images_.erase(it);
        return ret;
    }

    auto new_image = resource_allocator_->createImage(create_info, usage);
    auto new_record = std::make_shared<ImageRecord>();
    new_record->image = new_image;
    new_record->last_used_frame_index = frame_index_;
    unused_images_.insert({ { create_info, usage }, new_record });
    return new_image;
}

VKPT_END
