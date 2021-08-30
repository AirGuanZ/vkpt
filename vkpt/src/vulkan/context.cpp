#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <vkpt/vulkan/context.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

VKPT_BEGIN

namespace
{
    
    struct GLFWKeyCodeTable
    {
        KeyCode keys[GLFW_KEY_LAST + 1];

        GLFWKeyCodeTable()
            : keys{}
        {
            for(auto &k : keys)
                k = KEY_UNKNOWN;

            keys[GLFW_KEY_SPACE]      = KEY_SPACE;
            keys[GLFW_KEY_APOSTROPHE] = KEY_APOSTROPHE;
            keys[GLFW_KEY_COMMA]      = KEY_COMMA;
            keys[GLFW_KEY_MINUS]      = KEY_MINUS;
            keys[GLFW_KEY_PERIOD]     = KEY_PERIOD;
            keys[GLFW_KEY_SLASH]      = KEY_SLASH;

            keys[GLFW_KEY_SEMICOLON] = KEY_SEMICOLON;
            keys[GLFW_KEY_EQUAL]     = KEY_EQUAL;

            keys[GLFW_KEY_LEFT_BRACKET]  = KEY_LBRAC;
            keys[GLFW_KEY_BACKSLASH]     = KEY_BACKSLASH;
            keys[GLFW_KEY_RIGHT_BRACKET] = KEY_RBRAC;
            keys[GLFW_KEY_GRAVE_ACCENT]  = KEY_GRAVE_ACCENT;
            keys[GLFW_KEY_ESCAPE]        = KEY_ESCAPE;

            keys[GLFW_KEY_ENTER]     = KEY_ENTER;
            keys[GLFW_KEY_TAB]       = KEY_TAB;
            keys[GLFW_KEY_BACKSPACE] = KEY_BACKSPACE;
            keys[GLFW_KEY_INSERT]    = KEY_INSERT;
            keys[GLFW_KEY_DELETE]    = KEY_DELETE;

            keys[GLFW_KEY_RIGHT] = KEY_RIGHT;
            keys[GLFW_KEY_LEFT]  = KEY_LEFT;
            keys[GLFW_KEY_DOWN]  = KEY_DOWN;
            keys[GLFW_KEY_UP]    = KEY_UP;

            keys[GLFW_KEY_HOME] = KEY_HOME;
            keys[GLFW_KEY_END]  = KEY_END;

            keys[GLFW_KEY_F1]  = KEY_F1;
            keys[GLFW_KEY_F2]  = KEY_F2;
            keys[GLFW_KEY_F3]  = KEY_F3;
            keys[GLFW_KEY_F4]  = KEY_F4;
            keys[GLFW_KEY_F5]  = KEY_F5;
            keys[GLFW_KEY_F6]  = KEY_F6;
            keys[GLFW_KEY_F7]  = KEY_F7;
            keys[GLFW_KEY_F8]  = KEY_F8;
            keys[GLFW_KEY_F9]  = KEY_F9;
            keys[GLFW_KEY_F10] = KEY_F10;
            keys[GLFW_KEY_F11] = KEY_F11;
            keys[GLFW_KEY_F12] = KEY_F12;

            keys[GLFW_KEY_KP_0] = KEY_NUMPAD_0;
            keys[GLFW_KEY_KP_1] = KEY_NUMPAD_1;
            keys[GLFW_KEY_KP_2] = KEY_NUMPAD_2;
            keys[GLFW_KEY_KP_3] = KEY_NUMPAD_3;
            keys[GLFW_KEY_KP_4] = KEY_NUMPAD_4;
            keys[GLFW_KEY_KP_5] = KEY_NUMPAD_5;
            keys[GLFW_KEY_KP_6] = KEY_NUMPAD_6;
            keys[GLFW_KEY_KP_7] = KEY_NUMPAD_7;
            keys[GLFW_KEY_KP_8] = KEY_NUMPAD_8;
            keys[GLFW_KEY_KP_9] = KEY_NUMPAD_9;

            keys[GLFW_KEY_KP_DECIMAL]  = KEY_NUMPAD_DECIMAL;
            keys[GLFW_KEY_KP_DIVIDE]   = KEY_NUMPAD_DIV;
            keys[GLFW_KEY_KP_MULTIPLY] = KEY_NUMPAD_MUL;
            keys[GLFW_KEY_KP_SUBTRACT] = KEY_NUMPAD_SUB;
            keys[GLFW_KEY_KP_ADD]      = KEY_NUMPAD_ADD;
            keys[GLFW_KEY_KP_ENTER]    = KEY_NUMPAD_ENTER;

            keys[GLFW_KEY_LEFT_SHIFT]    = KEY_LSHIFT;
            keys[GLFW_KEY_LEFT_CONTROL]  = KEY_LCTRL;
            keys[GLFW_KEY_LEFT_ALT]      = KEY_LALT;
            keys[GLFW_KEY_RIGHT_SHIFT]   = KEY_RSHIFT;
            keys[GLFW_KEY_RIGHT_CONTROL] = KEY_RCTRL;
            keys[GLFW_KEY_RIGHT_ALT]     = KEY_RALT;

            for(int i = 0; i < 9; ++i)
                keys['0' + i] = KEY_D0 + i;

            for(int i = 0; i < 26; ++i)
                keys['A' + i] = KEY_A + i;
        }
    };
    
    void glfwWindowCloseCallback(GLFWwindow *window)
    {
        auto context = static_cast<Context*>(glfwGetWindowUserPointer(window));
        if(context)
            context->_triggerCloseWindow();
    }

    void glfwFramebufferResizeCallback(GLFWwindow *window, int, int)
    {
        auto context = static_cast<Context *>(glfwGetWindowUserPointer(window));
        if(context)
            context->_triggerResizeFramebuffer();
    }

    void glfwKeyCallback(
        GLFWwindow *window,
        int         key,
        int         scancode,
        int         action,
        int         mods)
    {
        auto context = static_cast<Context *>(glfwGetWindowUserPointer(window));
        if(!context)
            return;

        static const GLFWKeyCodeTable table;
        if(key < 0 || key >= agz::array_size<int>(table.keys))
            return;

        const KeyCode keycode = table.keys[key];
        if(keycode == KEY_UNKNOWN)
            return;

        if(action == GLFW_PRESS)
            context->_triggerKeyDown(keycode);
        else if(action == GLFW_RELEASE)
            context->_triggerKeyUp(keycode);
    }
    
    void glfwCharCallback(
        GLFWwindow  *window,
        unsigned int ch)
    {
        auto context = static_cast<Context *>(glfwGetWindowUserPointer(window));
        if(!context)
            return;
        context->_triggerCharInput(ch);
    }

}

struct Context::VKBImpl
{
    vkb::Instance       instance;
    vkb::PhysicalDevice physical_device;
    vkb::Device         device;
};

Context::Context(const Description &desc)
{
    image_count_ = desc.image_count;

    initializeWindow(desc);
    initializeVulkan(desc);

    input_ = std::make_unique<Input>(window_);
}

Context::~Context()
{
    if(impl_)
    {
        descriptor_set_manager_.reset();

        resource_allocator_ = ResourceAllocator();

        frame_resources_.reset();

        swapchain_image_available_semaphores_.clear();
        swapchain_image_views_.clear();
        swapchain_.reset();

        destroy_device(impl_->device);
        surface_.reset();
        destroy_instance(impl_->instance);
    }

    if(window_)
    {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

void Context::waitIdle()
{
    device_.waitIdle();
    frame_resources_->_triggerAllSync();
}

Input *Context::getInput()
{
    return input_.get();
}

void Context::beginFrame()
{
    for(;;)
    {
        doEvents();
        if(isMinimized() || !acquireNextImage())
            continue;
        break;
    }

    frame_resources_->beginFrame();
}

void Context::endFrame()
{
    endFrame({ getGraphicsQueue() });
}

void Context::endFrame(vk::ArrayProxy<const Queue> queues)
{
    frame_resources_->endFrame(queues);
}

void Context::swapBuffers(
    const vk::ArrayProxy<const vk::Semaphore> &wait_semaphores)
{
    auto swapchain = swapchain_.get();
    (void)present_queue_.presentKHR(
        vk::PresentInfoKHR{
            .waitSemaphoreCount = wait_semaphores.size(),
            .pWaitSemaphores    = wait_semaphores.data(),
            .swapchainCount     = 1,
            .pSwapchains        = &swapchain,
            .pImageIndices      = &image_index_
        });
}

void Context::swapBuffers(vk::Semaphore wait_semaphore)
{
    this->swapBuffers(std::array{ wait_semaphore });
}

CommandBuffer Context::newFrameGraphicsCommandBuffer()
{
    return frame_resources_->getCommandBufferAllocator().newGraphicsCommandBuffer();
}

CommandBuffer Context::newFrameComputeCommandBuffer()
{
    return frame_resources_->getCommandBufferAllocator().newComputeCommandBuffer();
}

CommandBuffer Context::newFrameTransferCommandBuffer()
{
    return frame_resources_->getCommandBufferAllocator().newTransferCommandBuffer();
}

vk::Fence Context::newFrameFence()
{
    return frame_resources_->getFenceAllocator().newFence();
}

vk::Semaphore Context::newFrameSemaphore()
{
    return frame_resources_->getSemaphoreAllocator().newSemaphore();
}

void Context::executeAfterFrameSync(std::function<void()> func)
{
    frame_resources_->executeAfterSync(std::move(func));
}

CommandBufferAllocator &Context::getFrameCommandBufferAllocator()
{
    return frame_resources_->getCommandBufferAllocator();
}

SemaphoreAllocator &Context::getFrameSemaphoreAllocator()
{
    return frame_resources_->getSemaphoreAllocator();
}

void Context::doEvents()
{
    glfwPollEvents();
}

void Context::waitEvents()
{
    glfwWaitEvents();
}

void Context::waitFocus()
{
    while(glfwGetWindowAttrib(window_, GLFW_FOCUSED))
        waitEvents();
}

bool Context::isMinimized() const
{
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);
    return width == 0 || height == 0;
}

bool Context::getCloseFlag() const
{
    return glfwWindowShouldClose(window_);
}

void Context::setCloseFlag(bool flag)
{
    glfwSetWindowShouldClose(window_, flag);
}

vk::Device Context::getDevice()
{
    return device_;
}

vk::SwapchainKHR Context::getSwapchain()
{
    return swapchain_.get();
}

Queue Context::getGraphicsQueue()
{
    return Queue(
        device_, graphics_queue_, Queue::Type::Graphics, graphics_queue_family_);
}

Queue Context::getComputeQueue()
{
    return Queue(
        device_, present_queue_, Queue::Type::Compute, compute_queue_family_);
}

Queue Context::getPresentQueue()
{
    return Queue(
        device_, present_queue_, Queue::Type::Present, present_queue_family_);
}

Queue Context::getTransferQueue()
{
    return Queue(
        device_, transfer_queue_, Queue::Type::Transfer, transfer_queue_family_);
}

ResourceAllocator &Context::getResourceAllocator()
{
    return resource_allocator_;
}

ResourceUploader Context::createResourceUploader()
{
    return ResourceUploader(getTransferQueue(), resource_allocator_);
}

ResourceUploader Context::createGraphicsResourceUploader()
{
    return ResourceUploader(getGraphicsQueue(), resource_allocator_);
}

vk::UniqueSemaphore Context::createSemaphore()
{
    return device_.createSemaphoreUnique(vk::SemaphoreCreateInfo{});
}

std::vector<vk::UniqueSemaphore> Context::createSemaphores(uint32_t count)
{
    std::vector<vk::UniqueSemaphore> result;
    for(uint32_t i = 0; i < count; ++i)
        result.push_back(createSemaphore());
    return result;
}

vk::UniqueFence Context::createFence(bool signaled)
{
    vk::FenceCreateFlags flags = {};
    if(signaled)
        flags |= vk::FenceCreateFlagBits::eSignaled;

    return device_.createFenceUnique(vk::FenceCreateInfo{
        .flags = flags
    });
}

vk::UniqueFramebuffer Context::createFramebuffer(
    const Pipeline                     &pipeline,
    vk::ArrayProxy<const vk::ImageView> attachments,
    uint32_t                            width,
    uint32_t                            height)
{
    if(!width && !height)
    {
        width  = pipeline.getDefaultRect().extent.width;
        height = pipeline.getDefaultRect().extent.height;
    }

    return device_.createFramebufferUnique(vk::FramebufferCreateInfo{
        .renderPass      = pipeline.getRenderPass(),
        .attachmentCount = attachments.size(),
        .pAttachments    = attachments.data(),
        .width           = width,
        .height          = height,
        .layers          = 1
    });
}

DescriptorSetLayout Context::createDescriptorSetLayout(
    const DescriptorSetLayoutDescription &desc)
{
    return descriptor_set_manager_->createLayout(desc);
}

Pipeline Context::createGraphicsPipeline(const PipelineDescription &desc)
{
    return Pipeline::build(device_, desc);
}

void Context::recreateSwapchain()
{
    waitIdle();
    sender_.send(InvalidateSwapchain{});

    swapchain_.reset();
    swapchain_images_.clear();
    swapchain_image_views_.clear();

    if(!isMinimized())
        createSwapchain();

    sender_.send(RecreateSwapchain{});
}

bool Context::acquireNextImage()
{
    frame_resource_index_ = (frame_resource_index_ + 1) % image_count_;

    auto &image_available_semaphore
        = swapchain_image_available_semaphores_[frame_resource_index_].get();

    auto acquire_result = device_.acquireNextImageKHR(
        swapchain_.get(), UINT64_MAX,
        image_available_semaphore, nullptr, &image_index_);

    if(acquire_result == vk::Result::eErrorOutOfDateKHR)
    {
        recreateSwapchain();
        return false;
    }

    if(acquire_result != vk::Result::eSuccess &&
       acquire_result != vk::Result::eSuboptimalKHR)
    {
        throw VKPTException(
            "failed to acquire next swapchain image");
    }

    return true;
}

vk::Extent2D Context::getFramebufferSize() const
{
    return swapchain_extent_;
}

uint32_t Context::getFramebufferSizeX() const
{
    return swapchain_extent_.width;
}

uint32_t Context::getFramebufferSizeY() const
{
    return swapchain_extent_.height;
}

float Context::getFramebufferAspectRatio() const
{
    return static_cast<float>(getFramebufferSizeX()) / getFramebufferSizeY();
}

vk::Viewport Context::getDefaultFramebufferViewport() const
{
    return vk::Viewport{
        .x        = 0,
        .y        = 0,
        .width    = static_cast<float>(getFramebufferSizeX()),
        .height   = static_cast<float>(getFramebufferSizeY()),
        .minDepth = 0,
        .maxDepth = 1
    };
}

vk::Rect2D Context::getFramebufferRect() const
{
    return vk::Rect2D{
        .offset = { 0, 0 },
        .extent = getFramebufferSize()
    };
}

uint32_t Context::getImageCount() const
{
    return image_count_;
}

vk::Format Context::getImageFormat() const
{
    return swapchain_format_;
}

vk::ImageView Context::getImageView() const
{
    return swapchain_image_views_[image_index_].get();
}

vk::ImageView Context::getImageView(uint32_t index) const
{
    return swapchain_image_views_[index].get();
}

vk::Semaphore Context::getImageAvailableSemaphore() const
{
    return swapchain_image_available_semaphores_[frame_resource_index_].get();
}

uint32_t Context::getImageIndex() const
{
    return image_index_;
}

vk::Image Context::getImage() const
{
    return swapchain_images_[image_index_];
}

vk::Image Context::getImage(uint32_t index) const
{
    return swapchain_images_[index];
}

void Context::_triggerCloseWindow()
{
    sender_.send(CloseWindow{});
}

void Context::_triggerResizeFramebuffer()
{
    recreateSwapchain();
}

void Context::_triggerMouseButtonDown(MouseButton button)
{
    input_->_triggerMouseButtonDown(button);
}

void Context::_triggerMouseButtonUp(MouseButton button)
{
    input_->_triggerMouseButtonUp(button);
}

void Context::_triggerWheelScroll(int offset)
{
    input_->_triggerWheelScroll(offset);
}

void Context::_triggerKeyDown(KeyCode key)
{
    input_->_triggerKeyDown(key);
}

void Context::_triggerKeyUp(KeyCode key)
{
    input_->_triggerKeyUp(key);
}

void Context::_triggerCharInput(uint32_t ch)
{
    input_->_triggerCharInput(ch);
}

void Context::initializeWindow(const Description &desc)
{
    if(glfwInit() != GLFW_TRUE)
        throw VKPTException("failed to initialize glfw");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, desc.maximized);
    
    window_ = glfwCreateWindow(
        desc.width, desc.height, desc.title.c_str(), nullptr, nullptr);
    if(!window_)
        throw VKPTException("failed to create glfw window");

    glfwSetWindowUserPointer(window_, this);

    glfwSetWindowCloseCallback(window_, glfwWindowCloseCallback);
    glfwSetFramebufferSizeCallback(window_, glfwFramebufferResizeCallback);
    glfwSetKeyCallback(window_, glfwKeyCallback);
    glfwSetCharCallback(window_, glfwCharCallback);
}

void Context::initializeVulkan(const Description &desc)
{
    assert(!impl_);
    impl_ = std::make_unique<VKBImpl>();

    static bool is_vk_dispatcher_initialized = false;
    if(!is_vk_dispatcher_initialized)
    {
        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        is_vk_dispatcher_initialized = true;
    }

    // instance

    vkb::InstanceBuilder instance_builder;
    instance_builder
        .set_app_name("vkpt")
        .require_api_version(1, 2);

    if(desc.debug_layers)
    {
        instance_builder
            .use_default_debug_messenger()
            .request_validation_layers();
    }

    auto build_instance_result = instance_builder.build();
    if(!build_instance_result)
        throw VKPTException("failed to create vulkan instance");
    impl_->instance = build_instance_result.value();
    instance_ = impl_->instance.instance;

    VULKAN_HPP_DEFAULT_DISPATCHER.init(impl_->instance.instance);

    // surface

    VkSurfaceKHR raw_surface;
    const VkResult create_surface_result = glfwCreateWindowSurface(
        impl_->instance.instance, window_, nullptr, &raw_surface);
    if(create_surface_result != VK_SUCCESS)
        throw VKPTException("failed to create vulkan surface");

    using Deleter = vk::UniqueHandleTraits<
        vk::SurfaceKHR, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>::deleter;
    surface_ =
        vk::UniqueSurfaceKHR(raw_surface, Deleter(impl_->instance.instance));
    
    // physical device

    vkb::PhysicalDeviceSelector physical_device_selector(impl_->instance);

    physical_device_selector
        .set_surface(surface_.get())
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .set_minimum_version(1, 2)
        .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
        .require_separate_compute_queue()
        .require_dedicated_transfer_queue();

    if(desc.ray_tracing)
    {
        physical_device_selector
            .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
            .add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
            .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
            .add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
            .add_required_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
            .add_required_extension(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    }

    auto select_physical_device_result = physical_device_selector.select();

    if(!select_physical_device_result)
        throw VKPTException("failed to select vulkan physical device");
    impl_->physical_device = select_physical_device_result.value();
    physical_device_ = impl_->physical_device.physical_device;

    // device

    auto build_device_result = vkb::DeviceBuilder(impl_->physical_device).build();
    if(!build_device_result)
        throw VKPTException("failed to create vulkan device");
    impl_->device = build_device_result.value();

    VULKAN_HPP_DEFAULT_DISPATCHER.init(impl_->device.device);

    device_ = impl_->device.device;

    graphics_queue_ = impl_->device.get_queue(vkb::QueueType::graphics).value();
    compute_queue_  = impl_->device.get_queue(vkb::QueueType::compute).value();
    present_queue_  = impl_->device.get_queue(vkb::QueueType::present).value();
    transfer_queue_ = impl_->device.get_queue(vkb::QueueType::transfer).value();

    graphics_queue_family_ = impl_->device.get_queue_index(
        vkb::QueueType::graphics).value();
    compute_queue_family_ = impl_->device.get_queue_index(
        vkb::QueueType::compute).value();
    present_queue_family_ = impl_->device.get_queue_index(
        vkb::QueueType::present).value();
    transfer_queue_family_ = impl_->device.get_queue_index(
        vkb::QueueType::transfer).value();

    // swapchain

    createSwapchain();

    // semaphores
    
    swapchain_image_available_semaphores_.resize(image_count_);
    for(uint32_t i = 0; i < image_count_; ++i)
    {
        auto semaphore = device_.createSemaphoreUnique({});
        swapchain_image_available_semaphores_[i] = std::move(semaphore);
    }

    // frame resources

    frame_resources_ = std::make_unique<FrameResources>(
        device_, image_count_,
        getGraphicsQueue(),
        getComputeQueue(),
        getTransferQueue());
    
    // resource allocator

    resource_allocator_ =
        ResourceAllocator(instance_, physical_device_, device_);

    // descriptor set manager

    descriptor_set_manager_ = std::make_unique<DescriptorSetManager>(device_);
}

void Context::createSwapchain()
{
    auto capabilities = physical_device_.getSurfaceCapabilitiesKHR(surface_.get());
    auto formats = physical_device_.getSurfaceFormatsKHR(surface_.get());
    auto present_modes = physical_device_.getSurfacePresentModesKHR(surface_.get());

    // choose format

    assert(!formats.empty());
    auto format = formats[0];
    for(auto &f : formats)
    {
        if(f.format == vk::Format::eB8G8R8A8Srgb &&
           f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            format = f;
            break;
        }
    }

    // choose present mode

    assert(!present_modes.empty());
    auto present_mode = vk::PresentModeKHR::eFifo;
    for(auto p : present_modes)
    {
        if(p == vk::PresentModeKHR::eMailbox)
        {
            present_mode = vk::PresentModeKHR::eMailbox;
            break;
        }
    }

    // choose extent

    vk::Extent2D extent = capabilities.currentExtent;
    if(extent.width == UINT32_MAX)
    {
        int fb_w, fb_h;
        glfwGetFramebufferSize(window_, &fb_w, &fb_h);

        extent.width = std::clamp(
            static_cast<uint32_t>(fb_w),
            capabilities.minImageExtent.width,
            capabilities.maxImageExtent.width);
        extent.height = std::clamp(
            static_cast<uint32_t>(fb_h),
            capabilities.minImageExtent.height,
            capabilities.maxImageExtent.height);
    }

    // choose image count
    
    uint32_t image_count = std::max(image_count_, capabilities.minImageCount);
    if(capabilities.maxImageCount > 0)
        image_count = std::min(image_count, capabilities.maxImageCount);
    if(image_count != image_count_)
    {
        throw VKPTException(
            "unsupported image count: " + std::to_string(image_count_));
    }

    // queue families & sharing mode

    uint32_t queue_family_indices[] = {
        impl_->device.get_queue_index(vkb::QueueType::graphics).value(),
        impl_->device.get_queue_index(vkb::QueueType::present).value()
    };
    const uint32_t queue_family_index_count =
        queue_family_indices[0] != queue_family_indices[1] ? 2 : 0;

    const auto queue_sharing_mode = queue_family_index_count ?
        vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;

    // create swapchain

    vk::SwapchainCreateInfoKHR swapchain_create_info = {
        .surface               = surface_.get(),
        .minImageCount         = image_count,
        .imageFormat           = format.format,
        .imageColorSpace       = format.colorSpace,
        .imageExtent           = extent,
        .imageArrayLayers      = 1,
        .imageUsage            = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode      = queue_sharing_mode,
        .queueFamilyIndexCount = queue_family_index_count,
        .pQueueFamilyIndices   = queue_family_indices,
        .preTransform          = capabilities.currentTransform,
        .presentMode           = present_mode,
        .clipped               = true
    };

    swapchain_ = device_.createSwapchainKHRUnique(swapchain_create_info);
    swapchain_format_ = format.format;
    swapchain_extent_ = extent;

    // get swapchain images

    uint32_t swapchain_image_count;
    std::vector<VkImage> swapchain_images;

    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetSwapchainImagesKHR(
        device_, swapchain_.get(), &swapchain_image_count, nullptr);
    swapchain_images.resize(swapchain_image_count);
    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetSwapchainImagesKHR(
        device_, swapchain_.get(),
        &swapchain_image_count, swapchain_images.data());

    for(auto i : swapchain_images)
        swapchain_images_.push_back(i);

    // create image views

    for(auto image : swapchain_images_)
    {
        vk::ImageViewCreateInfo image_view_create_info = {
            .image    = image,
            .viewType = vk::ImageViewType::e2D,
            .format   = swapchain_format_,
            .components = {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity,
            },
            .subresourceRange = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };

        auto image_view = device_.createImageViewUnique(image_view_create_info);
        swapchain_image_views_.push_back(std::move(image_view));
    }
}

VKPT_END
