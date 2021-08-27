#pragma once

#include <span>

#include <vkpt/vulkan/command_buffer.h>
#include <vkpt/vulkan/frame_synchronizer.h>
#include <vkpt/vulkan/input.h>
#include <vkpt/vulkan/queue.h>
#include <vkpt/vulkan/perframe_command_buffers.h>
#include <vkpt/vulkan/pipeline.h>
#include <vkpt/vulkan/resource_allocator.h>
#include <vkpt/vulkan/resource_uploader.h>

struct GLFWwindow;

VKPT_BEGIN

struct CloseWindow         { };
struct InvalidateSwapchain { };
struct RecreateSwapchain   { };

using CloseWindowHandler         = agz::event::functional_receiver_t<CloseWindow>;
using InvalidateSwapchainHandler = agz::event::functional_receiver_t<InvalidateSwapchain>;
using RecreateSwapchainHandler   = agz::event::functional_receiver_t<RecreateSwapchain>;

class Context : public agz::misc::uncopyable_t
{
public:

    struct Description
    {
        int  width     = 640;
        int  height    = 480;
        bool maximized = false;

        std::string title = "vkpt";

        int image_count = 3;

        bool ray_tracing = false;

#ifdef VKPT_DEBUG
        bool debug_layers = true;
#else
        bool debug_layers = false;
#endif
    };

    explicit Context(const Description &desc);

    ~Context();

    // vulkan frame

    bool newFrame();

    void nextFrameResources();

    void swapBuffers(const vk::ArrayProxy<const vk::Semaphore> &wait_semaphores);

    void swapBuffers(vk::Semaphore wait_semaphore);

    vk::Fence getFrameFence();

    CommandBuffer newFrameGraphicsCommandBuffer();

    CommandBuffer newFrameTransferCommandBuffer();

    void executeAfterFrameSync(std::function<void()> func);

    template<typename T>
    void destroyAfterFrameSync(T obj);

    // window events

    void waitIdle();

    Input *getInput();

    void doEvents();

    void waitEvents();

    void waitFocus();

    bool isMinimized() const;

    bool getCloseFlag() const;

    void setCloseFlag(bool flag);
    
    // vulkan objects

    vk::Device getDevice();

    vk::SwapchainKHR getSwapchain();

    Queue getGraphicsQueue();

    Queue getPresentQueue();

    Queue getTransferQueue();

    ResourceAllocator &getResourceAllocator();

    ResourceUploader createResourceUploader();

    ResourceUploader createGraphicsResourceUploader();

    vk::UniqueSemaphore createSemaphore();

    std::vector<vk::UniqueSemaphore> createSemaphores(uint32_t count);

    vk::UniqueFence createFence(bool signaled = false);

    vk::UniqueFramebuffer createFramebuffer(
        const Pipeline                     &pipeline,
        vk::ArrayProxy<const vk::ImageView> attachments,
        uint32_t                            width  = 0,
        uint32_t                            height = 0);

    // pipeline

    DescriptorSetLayout createDescriptorSetLayout(
        const DescriptorSetLayoutDescription &desc);

    Pipeline createGraphicsPipeline(const PipelineDescription &desc);

    // swapchain

    void recreateSwapchain();

    bool acquireNextImage();

    vk::Extent2D getFramebufferSize() const;

    uint32_t getFramebufferSizeX() const;

    uint32_t getFramebufferSizeY() const;

    float getFramebufferAspectRatio() const;

    vk::Viewport getDefaultFramebufferViewport() const;

    vk::Rect2D getFramebufferRect() const;

    uint32_t getImageCount() const;

    vk::Format getImageFormat() const;

    vk::ImageView getImageView() const;

    vk::ImageView getImageView(uint32_t index) const;

    vk::Semaphore getImageAvailableSemaphore() const;

    uint32_t getImageIndex() const;

    vk::Image getImage() const;

    vk::Image getImage(uint32_t index) const;

    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, CloseWindow)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, InvalidateSwapchain)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, RecreateSwapchain)

    void _triggerCloseWindow();

    void _triggerResizeFramebuffer();

    void _triggerMouseButtonDown(MouseButton button);

    void _triggerMouseButtonUp(MouseButton button);

    void _triggerWheelScroll(int offset);

    void _triggerKeyDown(KeyCode key);

    void _triggerKeyUp(KeyCode key);

    void _triggerCharInput(uint32_t ch);

private:

    void initializeWindow(const Description &desc);

    void initializeVulkan(const Description &desc);

    void createSwapchain();

    struct VKBImpl;

    std::unique_ptr<VKBImpl> impl_;

    GLFWwindow *window_ = nullptr;

    std::unique_ptr<Input> input_;

    vk::Instance       instance_;
    vk::PhysicalDevice physical_device_;
    vk::Device         device_;

    vk::Queue graphics_queue_;
    vk::Queue present_queue_;
    vk::Queue transfer_queue_;

    uint32_t graphics_queue_family_;
    uint32_t present_queue_family_;
    uint32_t transfer_queue_family_;

    vk::UniqueSurfaceKHR surface_;

    uint32_t               image_count_;
    vk::UniqueSwapchainKHR swapchain_;
    vk::Format             swapchain_format_;
    vk::Extent2D           swapchain_extent_;

    uint32_t image_index_          = 0;
    uint32_t frame_resource_index_ = 0;

    std::vector<vk::Image>           swapchain_images_;
    std::vector<vk::UniqueImageView> swapchain_image_views_;
    std::vector<vk::UniqueSemaphore> swapchain_image_available_semaphores_;

    std::unique_ptr<FrameSynchronizer>      frame_sync_;
    std::unique_ptr<PerFrameCommandBuffers> graphics_cmd_buffers_;
    std::unique_ptr<PerFrameCommandBuffers> transfer_cmd_buffers_;

    ResourceAllocator resource_allocator_;

    std::unique_ptr<DescriptorSetManager> descriptor_set_manager_;

    agz::event::sender_t<
        CloseWindow, InvalidateSwapchain, RecreateSwapchain> sender_;
};

template<typename T>
void Context::destroyAfterFrameSync(T obj)
{
    frame_sync_->destroyAfterSync(std::move(obj));
}

VKPT_END
