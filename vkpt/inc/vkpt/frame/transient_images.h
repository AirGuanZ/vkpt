#pragma once

#include <vkpt/allocator/image_allocator.h>

VKPT_BEGIN

class TransientImages : public ImageAllocator, public agz::misc::uncopyable_t
{
public:

    TransientImages();

    explicit TransientImages(
        ResourceAllocator &resource_allocator, int frame_count);

    TransientImages(TransientImages &&other) noexcept;

    TransientImages &operator=(TransientImages &&other) noexcept;

    operator bool() const;

    void swap(TransientImages &other) noexcept;

    void newFrame();

    Image newImage(
        const vk::ImageCreateInfo &create_info,
        vma::MemoryUsage           usage) override;

private:

    struct ImageRecord
    {
        Image   image;
        int64_t last_used_frame_index = 0;
    };

    using ImageMap = std::multimap<
        std::pair<vk::ImageCreateInfo, vma::MemoryUsage>,
        std::shared_ptr<ImageRecord>>;

    ResourceAllocator *resource_allocator_;

    int64_t frame_index_;
    int     frame_count_;

    ImageMap all_images_;
    ImageMap unused_images_;
};

VKPT_END
