#pragma once

#include <span>

#include <vkpt/graph/graph.h>
#include <vkpt/object/pipeline.h>
#include <vkpt/object/queue.h>
#include <vkpt/frame_resources.h>
#include <vkpt/imgui.h>
#include <vkpt/input.h>
#include <vkpt/resource_allocator.h>
#include <vkpt/resource_uploader.h>

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

        bool imgui = true;

#ifdef VKPT_DEBUG
        bool debug_layers = true;
#else
        bool debug_layers = false;
#endif
    };

    explicit Context(const Description &desc);

    ~Context();

    bool isImGuiEnabled() const;

    ImGuiIntegration &getImGuiIntegration();

    // vulkan frame

    void swapBuffers();

    FrameResources createFrameResources();

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

    Queue *getGraphicsQueue();

    Queue *getComputeQueue();

    Queue *getPresentQueue();

    Queue *getTransferQueue();

    ResourceAllocator &getResourceAllocator();

    ResourceUploader createResourceUploader();

    ResourceUploader createGraphicsResourceUploader();

    vk::UniqueSemaphore createSemaphore();

    std::vector<vk::UniqueSemaphore> createSemaphores(uint32_t count);

    vk::UniqueFence createFence(bool signaled = false);

    // pipeline

    DescriptorSetLayout createDescriptorSetLayout(
        const DescriptorSetLayoutDescription &desc);

    Pipeline createGraphicsPipeline(const PipelineDescription &desc);

    // render graph

    rg::Pass *addAcquireImagePass(rg::Graph &graph);

    rg::Pass *addPresentImagePass(rg::Graph &graph);

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

    ImageView getImageView() const;

    ImageView getImageView(uint32_t index) const;

    vk::Semaphore getImageAvailableSemaphore() const;

    vk::Semaphore getPresentAvailableSemaphore() const;

    uint32_t getImageIndex() const;

    Image getImage() const;

    Image getImage(uint32_t index) const;

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

    Queue graphics_queue_;
    Queue compute_queue_;
    Queue present_queue_;
    Queue transfer_queue_;

    uint32_t graphics_queue_family_;
    uint32_t compute_queue_family_;
    uint32_t present_queue_family_;
    uint32_t transfer_queue_family_;

    vk::UniqueSurfaceKHR surface_;

    uint32_t               image_count_;
    vk::UniqueSwapchainKHR swapchain_;
    ImageDescription       swapchain_image_desc_;

    uint32_t image_index_          = 0;
    uint32_t frame_resource_index_ = 0;

    std::vector<Image>               swapchain_images_;
    std::vector<ImageView>           swapchain_image_views_;
    std::vector<vk::UniqueSemaphore> swapchain_image_available_semaphores_;
    std::vector<vk::UniqueSemaphore> swapchain_present_semaphores_;

    ResourceAllocator resource_allocator_;

    std::unique_ptr<DescriptorSetManager> descriptor_set_manager_;

    std::unique_ptr<ImGuiIntegration> imgui_;

    using EventSender = agz::event::sender_t<
        CloseWindow, InvalidateSwapchain, RecreateSwapchain>;

    EventSender sender_;
};

VKPT_END
