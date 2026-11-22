#include "componeng/renderer/backend/vulkan/vk_device.hpp"

#include "componeng/utils/logger.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace componeng::renderer::vulkan {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT,
              const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR("[vulkan] %s", data->pMessage);
    if (user)
      static_cast<VulkanDevice *>(user)->countError();
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_INFO("[vulkan] %s", data->pMessage);
  }
  return VK_FALSE;
}

constexpr VkPipelineStageFlags2 kShaderStages =
    VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
constexpr VkPipelineStageFlags2 kDepthStages =
    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
constexpr VkAccessFlags2 kDepthAccess =
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

void stageAndAccess(VkImageLayout layout, VkPipelineStageFlags2 &stage,
                    VkAccessFlags2 &access) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      stage = kShaderStages;
      access = VK_ACCESS_2_SHADER_READ_BIT;
      return;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      return;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
      stage = kDepthStages;
      access = kDepthAccess;
      return;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      return;
    default:
      stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
      access = 0;
      return;
  }
}

VkAttachmentLoadOp toVk(rhi::LoadOp op) {
  switch (op) {
    case rhi::LoadOp::Load:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case rhi::LoadOp::Clear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case rhi::LoadOp::DontCare:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }
  return VK_ATTACHMENT_LOAD_OP_LOAD;
}

VkAttachmentStoreOp toVk(rhi::StoreOp op) {
  return op == rhi::StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE
                                   : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

} // namespace

VulkanDevice::~VulkanDevice() {
  shutdown();
}

// Setup

bool VulkanDevice::createInstance() {
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "ComponEng";
  app.pEngineName = "ComponEng";
  app.apiVersion = VK_API_VERSION_1_3;

  uint32_t count = 0;
  const char **glfwExt = glfwGetRequiredInstanceExtensions(&count);
  std::vector<const char *> extensions(glfwExt, glfwExt + count);
  std::vector<const char *> layers;

  const bool wantValidation = std::getenv("COMPONENG_VK_VALIDATION") != nullptr;
  if (wantValidation) {
    uint32_t n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    std::vector<VkLayerProperties> props(n);
    vkEnumerateInstanceLayerProperties(&n, props.data());
    for (const auto &p : props) {
      if (std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        break;
      }
    }
  }

  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ci.pApplicationInfo = &app;
  ci.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  ci.ppEnabledExtensionNames = extensions.data();
  ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
  ci.ppEnabledLayerNames = layers.data();
  if (vkCreateInstance(&ci, nullptr, &m_core->instance) != VK_SUCCESS) {
    LOG_ERROR("Vulkan: vkCreateInstance failed");
    return false;
  }

  if (!layers.empty()) {
    VkDebugUtilsMessengerCreateInfoEXT mi{
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    mi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    mi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    mi.pfnUserCallback = debugCallback;
    mi.pUserData = this;
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_core->instance,
                              "vkCreateDebugUtilsMessengerEXT"));
    if (fn)
      fn(m_core->instance, &mi, nullptr, &m_core->messenger);
    LOG_INFO("Vulkan validation layer enabled");
  }
  return true;
}

bool VulkanDevice::pickPhysicalDevice() {
  uint32_t n = 0;
  vkEnumeratePhysicalDevices(m_core->instance, &n, nullptr);
  std::vector<VkPhysicalDevice> devices(n);
  vkEnumeratePhysicalDevices(m_core->instance, &n, devices.data());

  int bestScore = -1;
  for (VkPhysicalDevice pd : devices) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    if (props.apiVersion < VK_API_VERSION_1_3)
      continue;

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extCount, exts.data());
    const bool hasSwapchain =
        std::any_of(exts.begin(), exts.end(), [](const auto &e) {
          return std::strcmp(e.extensionName,
                             VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        });
    if (!hasSwapchain)
      continue;

    VkPhysicalDeviceVulkan13Features f13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan12Features f12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f13.pNext = &f12;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f13;
    vkGetPhysicalDeviceFeatures2(pd, &f2);
    if (!f13.dynamicRendering || !f13.synchronization2 ||
        !f13.shaderDemoteToHelperInvocation || !f12.timelineSemaphore)
      continue;

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, families.data());
    int family = -1;
    for (uint32_t i = 0; i < qCount; ++i) {
      VkBool32 present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, m_surface, &present);
      if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
        family = static_cast<int>(i);
        break;
      }
    }
    if (family < 0)
      continue;

    const int score =
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 1000 : 0;
    if (score > bestScore) {
      bestScore = score;
      m_core->physical = pd;
      m_core->queueFamily = static_cast<uint32_t>(family);
    }
  }
  if (m_core->physical == VK_NULL_HANDLE) {
    LOG_ERROR(
        "Vulkan: no GPU with Vulkan 1.3, dynamic rendering and presentation");
    return false;
  }
  vkGetPhysicalDeviceProperties(m_core->physical, &m_core->props);
  vkGetPhysicalDeviceMemoryProperties(m_core->physical, &m_core->memProps);
  LOG_INFO("Vulkan device: %s", m_core->props.deviceName);
  return true;
}

bool VulkanDevice::createDevice() {
  VkPhysicalDeviceFeatures supported;
  vkGetPhysicalDeviceFeatures(m_core->physical, &supported);
  m_core->anisotropy = supported.samplerAnisotropy == VK_TRUE;

  const float priority = 1.0f;
  VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qi.queueFamilyIndex = m_core->queueFamily;
  qi.queueCount = 1;
  qi.pQueuePriorities = &priority;

  VkPhysicalDeviceVulkan13Features f13{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;
  f13.shaderDemoteToHelperInvocation =
      VK_TRUE; // glslang compiles `discard` to it
  VkPhysicalDeviceVulkan12Features f12{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  f12.timelineSemaphore = VK_TRUE;
  f13.pNext = &f12;
  VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  f2.pNext = &f13;
  f2.features.samplerAnisotropy = supported.samplerAnisotropy;

  const char *ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
  VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  di.pNext = &f2;
  di.queueCreateInfoCount = 1;
  di.pQueueCreateInfos = &qi;
  di.enabledExtensionCount = 1;
  di.ppEnabledExtensionNames = &ext;
  if (vkCreateDevice(m_core->physical, &di, nullptr, &m_core->device) !=
      VK_SUCCESS) {
    LOG_ERROR("Vulkan: vkCreateDevice failed");
    return false;
  }
  vkGetDeviceQueue(m_core->device, m_core->queueFamily, 0, &m_core->queue);

  m_core->allocator =
      createGpuAllocator(m_core->instance, m_core->physical, m_core->device);
  if (!m_core->allocator) {
    LOG_ERROR("Vulkan: could not create the memory allocator");
    return false;
  }

  // One timeline semaphore carries frame progress: each submit signals its
  // serial, and anything that needs a frame finished waits for that value.
  VkSemaphoreTypeCreateInfo ti{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
  ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  ti.initialValue = 0;
  VkSemaphoreCreateInfo tsi{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  tsi.pNext = &ti;
  if (vkCreateSemaphore(m_core->device, &tsi, nullptr, &m_core->timeline) !=
      VK_SUCCESS)
    return false;

  VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pi.queueFamilyIndex = m_core->queueFamily;
  return vkCreateCommandPool(m_core->device, &pi, nullptr, &m_core->cmdPool) ==
         VK_SUCCESS;
}

void VulkanDevice::createFrameObjects() {
  for (FrameSlot &fs : m_slots) {
    VkCommandBufferAllocateInfo ai{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = m_core->cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_core->device, &ai, &fs.cmd);

    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(m_core->device, &si, nullptr, &fs.imageAvailable);
  }
}

bool VulkanDevice::createDepthImage() {
  for (DepthTarget &depth : m_depth) {
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = m_depthFormat;
    ii.extent = {m_extent.width, m_extent.height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!m_core->createImage(ii, /*dedicated=*/true, depth.image))
      return false;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = depth.image.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_depthFormat;
    vi.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(m_core->device, &vi, nullptr, &depth.view) !=
        VK_SUCCESS)
      return false;
  }
  return true;
}

bool VulkanDevice::createSwapchain() {
  VkDevice dev = m_core->device;
  VkPhysicalDevice pd = m_core->physical;

  VkSurfaceCapabilitiesKHR caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, m_surface, &caps);

  uint32_t n = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(pd, m_surface, &n, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(n);
  vkGetPhysicalDeviceSurfaceFormatsKHR(pd, m_surface, &n, formats.data());
  // UNORM, not sRGB: the GL path writes straight to a linear default
  // framebuffer and the shaders encode their own output.
  VkSurfaceFormatKHR chosen = formats.at(0);
  for (const auto &f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM ||
        f.format == VK_FORMAT_R8G8B8A8_UNORM) {
      chosen = f;
      break;
    }
  }

  vkGetPhysicalDeviceSurfacePresentModesKHR(pd, m_surface, &n, nullptr);
  std::vector<VkPresentModeKHR> modes(n);
  vkGetPhysicalDeviceSurfacePresentModesKHR(pd, m_surface, &n, modes.data());
  auto has = [&](VkPresentModeKHR m) {
    return std::find(modes.begin(), modes.end(), m) != modes.end();
  };
  // The GL path runs with swap interval 0.
  VkPresentModeKHR mode =
      has(VK_PRESENT_MODE_IMMEDIATE_KHR) ? VK_PRESENT_MODE_IMMEDIATE_KHR
      : has(VK_PRESENT_MODE_MAILBOX_KHR) ? VK_PRESENT_MODE_MAILBOX_KHR
                                         : VK_PRESENT_MODE_FIFO_KHR;

  int fbw = 0, fbh = 0;
  glfwGetFramebufferSize(m_window, &fbw, &fbh);
  VkExtent2D extent = caps.currentExtent;
  if (extent.width == UINT32_MAX) {
    extent.width =
        std::clamp(static_cast<uint32_t>(fbw), caps.minImageExtent.width,
                   caps.maxImageExtent.width);
    extent.height =
        std::clamp(static_cast<uint32_t>(fbh), caps.minImageExtent.height,
                   caps.maxImageExtent.height);
  }
  if (extent.width == 0 || extent.height == 0)
    return false;

  uint32_t imageCount = caps.minImageCount + 1;
  if (caps.maxImageCount > 0)
    imageCount = std::min(imageCount, caps.maxImageCount);

  VkSwapchainKHR old = m_swapchain;
  VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ci.surface = m_surface;
  ci.minImageCount = imageCount;
  ci.imageFormat = chosen.format;
  ci.imageColorSpace = chosen.colorSpace;
  ci.imageExtent = extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  ((caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                       ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                       : 0);
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha =
      (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
          ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
          : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  ci.presentMode = mode;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = old;
  VkSwapchainKHR created = VK_NULL_HANDLE;
  if (vkCreateSwapchainKHR(dev, &ci, nullptr, &created) != VK_SUCCESS) {
    LOG_ERROR("Vulkan: vkCreateSwapchainKHR failed");
    return false;
  }

  // The old chain (and what hangs off it) goes once the new one exists.
  vkDeviceWaitIdle(dev);
  destroySwapchain(/*keepHandle=*/false);
  m_swapchain = created;
  m_swapFormat = chosen.format;
  m_extent = extent;

  vkGetSwapchainImagesKHR(dev, m_swapchain, &n, nullptr);
  m_images.resize(n);
  vkGetSwapchainImagesKHR(dev, m_swapchain, &n, m_images.data());
  m_imageViews.resize(n);
  m_renderDone.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = m_images[i];
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_swapFormat;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(dev, &vi, nullptr, &m_imageViews[i]);
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(dev, &si, nullptr, &m_renderDone[i]);
  }
  if (!createDepthImage())
    return false;
  if (m_imguiReady)
    ImGui_ImplVulkan_SetMinImageCount(std::max(2u, caps.minImageCount));
  m_swapDirty = false;
  return true;
}

void VulkanDevice::destroySwapchain(bool keepHandle) {
  VkDevice dev = m_core->device;
  for (VkSemaphore s : m_renderDone)
    vkDestroySemaphore(dev, s, nullptr);
  for (VkImageView v : m_imageViews)
    vkDestroyImageView(dev, v, nullptr);
  m_renderDone.clear();
  m_imageViews.clear();
  m_images.clear();
  for (DepthTarget &depth : m_depth) {
    if (depth.view)
      vkDestroyImageView(dev, depth.view, nullptr);
    depth.view = VK_NULL_HANDLE;
    m_core->destroyImage(depth.image);
  }
  if (!keepHandle && m_swapchain) {
    vkDestroySwapchainKHR(dev, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }
}

void VulkanDevice::init(void *windowHandle) {
  m_window = static_cast<GLFWwindow *>(windowHandle);
  if (!glfwVulkanSupported()) {
    LOG_ERROR("Vulkan: GLFW reports no Vulkan loader/ICD");
    return;
  }
  m_core = std::make_shared<VulkanCore>();
  if (!createInstance())
    return;
  if (glfwCreateWindowSurface(m_core->instance, m_window, nullptr,
                              &m_surface) != VK_SUCCESS) {
    LOG_ERROR("Vulkan: could not create a window surface");
    return;
  }
  if (!pickPhysicalDevice() || !createDevice())
    return;
  createFrameObjects();
  m_dumpPath = std::getenv("COMPONENG_VK_DUMP_FRAME");
  m_timer.init(*m_core);
  if (!createSwapchain())
    return;

  LOG_INFO("Vulkan backend ready (%ux%u)", m_extent.width, m_extent.height);
}

void VulkanDevice::shutdown() {
  if (!m_core || m_core->device == VK_NULL_HANDLE)
    return;
  VkDevice dev = m_core->device;
  vkDeviceWaitIdle(dev);
  if (m_cmdOpen) {
    vkResetCommandBuffer(m_cmd, 0);
    m_cmdOpen = false;
    m_frameOpen = false;
    m_rendering = false;
  }
  m_core->pollCompleted();
  m_timer.stopRecording();
  m_timer.shutdown();
  m_pipeline = nullptr;
  for (VulkanBindGroup *&g : m_groups)
    g = nullptr;
  for (FrameSlot &fs : m_slots) {
    if (fs.imageAvailable)
      vkDestroySemaphore(dev, fs.imageAvailable, nullptr);
    fs.imageAvailable = VK_NULL_HANDLE;
  }
  destroySwapchain(/*keepHandle=*/false);
  if (m_surface) {
    vkDestroySurfaceKHR(m_core->instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }
  m_core->flushAllDeferred(); // the device is idle
  m_shutDown = true;
}

// Frame

bool VulkanDevice::ensureCommandBuffer() {
  if (m_cmdOpen)
    return true;
  if (m_shutDown || !m_core || m_core->device == VK_NULL_HANDLE)
    return false;

  // Reusing this slot needs its previous frame (kFramesInFlight ago) finished;
  // the frame in between may still run, which is the overlap.
  FrameSlot &fs = m_slots[m_slot];
  m_core->waitSerial(fs.serial);
  pollCompletedFrames();

  m_cmd = fs.cmd;
  vkResetCommandBuffer(m_cmd, 0);
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(m_cmd, &bi);
  m_timer.startRecording(m_cmd, m_slot);
  m_cmdOpen = true;
  m_stagingBytes = 0;
  for (VkDescriptorSet &set : m_applied)
    set = VK_NULL_HANDLE;
  return true;
}

bool VulkanDevice::beginFrame() {
  if (m_frameOpen)
    return true;
  if (m_shutDown || !m_core || !m_swapchain)
    return false;

  int w = 0, h = 0;
  glfwGetFramebufferSize(m_window, &w, &h);
  if (w == 0 || h == 0)
    return false; // minimised
  if (m_swapDirty || static_cast<uint32_t>(w) != m_extent.width ||
      static_cast<uint32_t>(h) != m_extent.height) {
    if (!createSwapchain())
      return false;
  }
  if (!ensureCommandBuffer())
    return false;

  FrameSlot &fs = m_slots[m_slot];
  VkResult r = VK_ERROR_OUT_OF_DATE_KHR;
  for (int attempt = 0;
       attempt < 2 && r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR; ++attempt) {
    r = vkAcquireNextImageKHR(m_core->device, m_swapchain, UINT64_MAX,
                              fs.imageAvailable, VK_NULL_HANDLE, &m_imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR && !createSwapchain())
      return false;
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
    return false;

  transitionImage(m_cmd, m_images[m_imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  // This slot's depth image was last used kFramesInFlight frames ago and that
  // frame has finished (ensureCommandBuffer waited), so no dependency needed.
  transitionImage(m_cmd, m_depth[m_slot].image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, 1, kDepthStages,
                  0, kDepthStages, kDepthAccess, VK_IMAGE_ASPECT_DEPTH_BIT);

  m_frameOpen = true;
  m_rendering = false;
  return true;
}

void VulkanDevice::present(void * /*windowHandle*/) {
  if (!m_frameOpen)
    return;
  if (m_rendering) {
    LOG_ERROR("Vulkan: present with a rendering still open");
    endRendering();
  }

  VulkanCore::Allocation dump;
  const bool dumping = m_dumpPath && ++m_frameCount == 120;
  if (dumping) {
    const VkDeviceSize bytes =
        VkDeviceSize{m_extent.width} * m_extent.height * 4;
    if (m_core->createMappedBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   dump, HostMemory::PreferHost)) {
      transitionImage(m_cmd, m_images[m_imageIndex],
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_READ_BIT);
      VkBufferImageCopy copy{};
      copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
      copy.imageExtent = {m_extent.width, m_extent.height, 1};
      vkCmdCopyImageToBuffer(m_cmd, m_images[m_imageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dump.buffer,
                             1, &copy);
      transitionImage(
          m_cmd, m_images[m_imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1,
          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    }
  }
  transitionImage(m_cmd, m_images[m_imageIndex],
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 1,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);
  m_timer.stopRecording();
  vkEndCommandBuffer(m_cmd);

  FrameSlot &fs = m_slots[m_slot];
  const uint64_t serial = m_core->recordingSerial;
  const VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  // Binary semaphores (acquire, render-done) take a dummy value 0 in the
  // timeline-submit arrays; the timeline gets the frame's serial.
  const uint64_t waitValue = 0;
  const uint64_t signalValues[2] = {0, serial};
  VkTimelineSemaphoreSubmitInfo ts{
      VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
  ts.waitSemaphoreValueCount = 1;
  ts.pWaitSemaphoreValues = &waitValue;
  ts.signalSemaphoreValueCount = 2;
  ts.pSignalSemaphoreValues = signalValues;
  const VkSemaphore signals[2] = {m_renderDone[m_imageIndex], m_core->timeline};
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.pNext = &ts;
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &fs.imageAvailable;
  si.pWaitDstStageMask = &waitStage;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &m_cmd;
  si.signalSemaphoreCount = 2;
  si.pSignalSemaphores = signals;
  vkQueueSubmit(m_core->queue, 1, &si, VK_NULL_HANDLE);
  fs.serial = serial;
  ++m_core->recordingSerial;
  m_stagingBytes = 0;
  if (dump.mapped) {
    m_core->waitSerial(serial);
    vkQueueWaitIdle(m_core->queue);
    if (FILE *f = std::fopen(m_dumpPath, "wb")) {
      std::fprintf(f, "P6\n%u %u\n255\n", m_extent.width, m_extent.height);
      const auto *px = static_cast<const unsigned char *>(dump.mapped);
      const bool bgra = m_swapFormat != VK_FORMAT_R8G8B8A8_UNORM;
      for (size_t i = 0; i < size_t{m_extent.width} * m_extent.height; ++i) {
        const unsigned char rgb[3] = {px[i * 4 + (bgra ? 2 : 0)], px[i * 4 + 1],
                                      px[i * 4 + (bgra ? 0 : 2)]};
        std::fwrite(rgb, 1, 3, f);
      }
      std::fclose(f);
      LOG_INFO("Vulkan: wrote frame %u to %s", m_frameCount, m_dumpPath);
    }
    m_core->destroyAllocation(dump);
  }

  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &m_renderDone[m_imageIndex];
  pi.swapchainCount = 1;
  pi.pSwapchains = &m_swapchain;
  pi.pImageIndices = &m_imageIndex;
  const VkResult pr = vkQueuePresentKHR(m_core->queue, &pi);
  if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR)
    m_swapDirty = true;

  // No wait here: the GPU runs this frame while the CPU records the next.
  m_cmdOpen = false;
  m_frameOpen = false;
  m_pipeline = nullptr;
  m_slot = (m_slot + 1) % kFramesInFlight;
  pollCompletedFrames();
}

void VulkanDevice::pollCompletedFrames() {
  m_core->pollCompleted();
  m_core->flushDeferred();
  for (uint32_t i = 0; i < kFramesInFlight; ++i) {
    if (m_slots[i].serial != 0 &&
        m_slots[i].serial <= m_core->completedSerial && m_timer.hasPending(i))
      m_timer.collect(i);
  }
}

void VulkanDevice::setWindowViewport(int x, int y, int w, int h) {
  m_windowViewport = {x, y, static_cast<uint32_t>(std::max(w, 0)),
                      static_cast<uint32_t>(std::max(h, 0))};
  m_windowViewportSet = true;
}

rhi::Extent2D VulkanDevice::windowViewportExtent() const {
  if (m_windowViewportSet && m_windowViewport.width > 0 &&
      m_windowViewport.height > 0)
    return {m_windowViewport.width, m_windowViewport.height};
  return {m_extent.width, m_extent.height};
}

// Transfers

VkCommandBuffer VulkanDevice::beginTransfer(size_t stagingBytes) {
  // Staging stays alive until the frame it rode in has finished; past this much
  // a blocking upload (which frees it at once) is the better trade. Transfers
  // cannot be recorded inside a rendering either.
  constexpr size_t kMaxQueuedStaging = 256u << 20;
  if (m_rendering || m_stagingBytes + stagingBytes > kMaxQueuedStaging ||
      !ensureCommandBuffer())
    return VK_NULL_HANDLE;
  m_stagingBytes += stagingBytes;
  return m_cmd;
}

/**
 * Where an image's transfer commands go: normally the frame's command buffer
 * (no CPU wait; staging is freed once that frame finishes), or a blocking
 * one-shot submit if no frame can be opened, a rendering is open, or too much
 * staging is queued.
 */
struct VulkanDevice::UploadScope {
  UploadScope(VulkanDevice &device, size_t stagingBytes)
      : m_core(*device.m_core) {
    m_cmd = device.beginTransfer(stagingBytes);
    if (m_cmd == VK_NULL_HANDLE) {
      m_cmd = m_core.beginOneShot();
      m_oneShot = true;
    }
  }
  VkCommandBuffer cmd() const {
    return m_cmd;
  }
  /** Done recording; `staging` (if any) is released at the right time. */
  void finish(VulkanCore::Allocation *staging) {
    if (m_oneShot) {
      m_core.endOneShot(m_cmd);
      if (staging)
        m_core.destroyAllocation(*staging);
    } else if (staging && staging->buffer != VK_NULL_HANDLE) {
      VulkanCore *core = &m_core;
      VulkanCore::Allocation doomed = *staging;
      core->defer(
          [core, doomed]() mutable { core->destroyAllocation(doomed); });
    }
    if (staging)
      *staging = VulkanCore::Allocation{};
  }

  VulkanCore &m_core;
  VkCommandBuffer m_cmd = VK_NULL_HANDLE;
  bool m_oneShot = false;
};

void VulkanDevice::uploadBuffer(rhi::IBuffer &dst, uint64_t offset,
                                const void *data, uint64_t bytes) {
  static_cast<VulkanBuffer &>(dst).fill(offset, data, bytes);
}

void VulkanDevice::uploadImage(rhi::IImage &dst, const rhi::Rect &region,
                               const void *pixels) {
  auto &image = static_cast<VulkanImage &>(dst);
  if (!image.valid() || !pixels)
    return;
  const rhi::Extent2D extent = image.extent();
  if (region.x < 0 || region.y < 0 ||
      static_cast<uint32_t>(region.x) + region.width > extent.width ||
      static_cast<uint32_t>(region.y) + region.height > extent.height) {
    LOG_ERROR("Vulkan: image upload region lies outside the image");
    return;
  }
  const size_t bytes = static_cast<size_t>(region.width) * region.height *
                       bytesPerPixel(image.format());
  VulkanCore::Allocation staging;
  if (!m_core->createMappedBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  staging))
    return;
  std::memcpy(staging.mapped, pixels, bytes);

  UploadScope scope(*this, bytes);
  VkCommandBuffer cmd = scope.cmd();
  VulkanImage::Slot &slot = image.slot(0);
  const uint32_t mips = image.mipLevels();

  VkPipelineStageFlags2 srcStage;
  VkAccessFlags2 srcAccess;
  stageAndAccess(slot.layout, srcStage, srcAccess);
  transitionImage(cmd, slot.alloc.image, slot.layout,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mips, srcStage,
                  srcAccess, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT);

  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageOffset = {region.x, region.y, 0};
  copy.imageExtent = {region.width, region.height, 1};
  vkCmdCopyBufferToImage(cmd, staging.buffer, slot.alloc.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  const auto T = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  const auto W = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  const auto R = VK_ACCESS_2_TRANSFER_READ_BIT;
  const auto F = kShaderStages;
  const auto S = VK_ACCESS_2_SHADER_READ_BIT;

  int32_t mw = static_cast<int32_t>(extent.width);
  int32_t mh = static_cast<int32_t>(extent.height);
  for (uint32_t i = 1; i < mips; ++i) {
    transitionImage(cmd, slot.alloc.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, i - 1, 1, T, W, T, R);
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
    blit.srcOffsets[1] = {mw, mh, 1};
    mw = std::max(mw / 2, 1);
    mh = std::max(mh / 2, 1);
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
    blit.dstOffsets[1] = {mw, mh, 1};
    vkCmdBlitImage(cmd, slot.alloc.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   slot.alloc.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &blit, VK_FILTER_LINEAR);
  }
  if (mips > 1) {
    transitionImage(cmd, slot.alloc.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mips - 1, T, R,
                    F, S);
  }
  transitionImage(cmd, slot.alloc.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mips - 1, 1, T, W,
                  F, S);
  slot.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  scope.finish(&staging);
}

// Resources

std::unique_ptr<rhi::IBuffer>
VulkanDevice::createBuffer(const rhi::BufferDesc &desc) {
  return std::make_unique<VulkanBuffer>(m_core, desc);
}

std::unique_ptr<rhi::IImage>
VulkanDevice::createImage(const rhi::ImageDesc &desc) {
  auto image = std::make_unique<VulkanImage>(m_core, desc);
  const bool attachment =
      hasFlag(desc.usage, rhi::ImageUsage::ColorAttachment) ||
      hasFlag(desc.usage, rhi::ImageUsage::DepthAttachment);
  if (image->valid() && !attachment &&
      hasFlag(desc.usage, rhi::ImageUsage::Sampled)) {
    // Readable from the start, even before anything is uploaded.
    UploadScope scope(*this, 0);
    VulkanImage::Slot &slot = image->slot(0);
    transitionImage(scope.cmd(), slot.alloc.image, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0,
                    image->mipLevels(), VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                    kShaderStages, VK_ACCESS_2_SHADER_READ_BIT);
    slot.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scope.finish(nullptr);
  }
  return image;
}

std::unique_ptr<rhi::ISampler>
VulkanDevice::createSampler(const rhi::SamplerDesc &desc) {
  return std::make_unique<VulkanSampler>(m_core, desc);
}

std::unique_ptr<rhi::IShaderModule>
VulkanDevice::createShaderModule(const rhi::ShaderModuleDesc &desc) {
  return std::make_unique<VulkanShaderModule>(m_core, desc);
}

std::unique_ptr<rhi::IBindGroupLayout>
VulkanDevice::createBindGroupLayout(const rhi::BindGroupLayoutDesc &desc) {
  return std::make_unique<VulkanBindGroupLayout>(m_core, desc);
}

std::unique_ptr<rhi::IBindGroup>
VulkanDevice::createBindGroup(const rhi::BindGroupDesc &desc) {
  auto layout = static_cast<VulkanBindGroupLayout *>(desc.layout)->data();
  return std::make_unique<VulkanBindGroup>(m_core, std::move(layout), desc);
}

std::unique_ptr<rhi::IGraphicsPipeline>
VulkanDevice::createGraphicsPipeline(const rhi::GraphicsPipelineDesc &desc) {
  auto pipeline = std::make_unique<VulkanGraphicsPipeline>(m_core, desc);
  if (!pipeline->valid())
    return nullptr;
  return pipeline;
}

std::unique_ptr<rhi::IComputePipeline>
VulkanDevice::createComputePipeline(const rhi::ComputePipelineDesc &) {
  LOG_ERROR("Vulkan: compute pipelines are not implemented yet");
  return nullptr;
}

// Rendering

void VulkanDevice::transition(VulkanImage &image, uint32_t slotIndex,
                              VkImageLayout to) {
  VulkanImage::Slot &slot = image.slot(slotIndex);
  if (image.perFrame() && slot.frameSerial != m_core->recordingSerial) {
    // First use this frame. The frame that last used this copy has finished
    // (ensureCommandBuffer waited for it), and nothing in it is kept.
    slot.frameSerial = m_core->recordingSerial;
    slot.layout = VK_IMAGE_LAYOUT_UNDEFINED;
  }
  if (slot.layout == to)
    return;
  VkPipelineStageFlags2 srcStage, dstStage;
  VkAccessFlags2 srcAccess, dstAccess;
  stageAndAccess(slot.layout, srcStage, srcAccess);
  stageAndAccess(to, dstStage, dstAccess);
  transitionImage(m_cmd, slot.alloc.image, slot.layout, to, 0,
                  image.mipLevels(), srcStage, srcAccess, dstStage, dstAccess,
                  image.aspect());
  slot.layout = to;
}

void VulkanDevice::applyViewport(const rhi::Rect &r) {
  if (m_targetColor) {
    // Not flipped: row 0 of the image is the bottom of the picture, as in a GL
    // framebuffer texture, so a shader samples it like any other texture.
    const VkViewport vp{static_cast<float>(r.x),
                        static_cast<float>(r.y),
                        static_cast<float>(r.width),
                        static_cast<float>(r.height),
                        0.0f,
                        1.0f};
    vkCmdSetViewport(m_cmd, 0, 1, &vp);
    const VkRect2D rect{{r.x, r.y}, {r.width, r.height}};
    vkCmdSetScissor(m_cmd, 0, 1, &rect);
    return;
  }
  // GL's origin is bottom-left; Vulkan's is top-left. A negative height keeps
  // GL's up direction and winding.
  const float fbh = static_cast<float>(m_extent.height);
  const VkViewport vp{static_cast<float>(r.x),
                      fbh - static_cast<float>(r.y),
                      static_cast<float>(r.width),
                      -static_cast<float>(r.height),
                      0.0f,
                      1.0f};
  vkCmdSetViewport(m_cmd, 0, 1, &vp);
  const int top = std::max(0, static_cast<int>(m_extent.height) -
                                  (r.y + static_cast<int>(r.height)));
  const VkRect2D rect{{std::max(0, r.x), top}, {r.width, r.height}};
  vkCmdSetScissor(m_cmd, 0, 1, &rect);
}

void VulkanDevice::setViewport(const rhi::Rect &r) {
  applyViewport(r);
}

void VulkanDevice::setScissor(const rhi::Rect &r) {
  applyViewport(r);
}

void VulkanDevice::beginRendering(const rhi::RenderingDesc &desc) {
  if (!m_frameOpen) {
    LOG_ERROR("Vulkan: beginRendering outside a frame");
    return;
  }
  if (m_rendering)
    endRendering();

  auto *colorImage = static_cast<VulkanImage *>(desc.color.image);
  auto *depthImage = static_cast<VulkanImage *>(desc.depth.image);

  VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  VkExtent2D extent = m_extent;
  if (colorImage) {
    transition(*colorImage, m_slot, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    color.imageView = colorImage->slot(m_slot).view;
    extent = {colorImage->extent().width, colorImage->extent().height};
  } else {
    color.imageView = m_imageViews[m_imageIndex];
  }
  color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color.loadOp = toVk(desc.color.load);
  color.storeOp = toVk(desc.color.store);
  for (int i = 0; i < 4; ++i)
    color.clearValue.color.float32[i] = desc.color.clear[i];

  VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  if (desc.hasDepth) {
    if (depthImage) {
      transition(*depthImage, m_slot, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      depth.imageView = depthImage->slot(m_slot).view;
    } else {
      depth.imageView = m_depth[m_slot].view;
    }
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth.loadOp = toVk(desc.depth.load);
    depth.storeOp = toVk(desc.depth.store);
    depth.clearValue.depthStencil = {desc.depth.clearDepth, 0};
  }

  VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ri.renderArea = {{0, 0}, extent};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &color;
  ri.pDepthAttachment = desc.hasDepth ? &depth : nullptr;
  vkCmdBeginRendering(m_cmd, &ri);

  m_rendering = true;
  m_targetColor = colorImage;
  m_targetDepth = desc.hasDepth ? depthImage : nullptr;
  m_renderExtent = extent;

  // An image is drawn unflipped, which mirrors the winding.
  vkCmdSetFrontFace(m_cmd, colorImage ? VK_FRONT_FACE_CLOCKWISE
                                      : VK_FRONT_FACE_COUNTER_CLOCKWISE);
  if (colorImage) {
    applyViewport({0, 0, extent.width, extent.height});
  } else if (m_windowViewportSet && m_windowViewport.width > 0 &&
             m_windowViewport.height > 0) {
    applyViewport(m_windowViewport);
  } else {
    applyViewport({0, 0, extent.width, extent.height});
  }
}

void VulkanDevice::endRendering() {
  if (!m_rendering)
    return;
  vkCmdEndRendering(m_cmd);
  m_rendering = false;
  // What was drawn into an image is sampled from here on.
  if (m_targetColor &&
      hasFlag(m_targetColor->desc().usage, rhi::ImageUsage::Sampled)) {
    transition(*m_targetColor, m_slot,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
  m_targetColor = nullptr;
  m_targetDepth = nullptr;
}

void VulkanDevice::bindPipeline(rhi::IGraphicsPipeline &pipeline) {
  if (!m_rendering)
    return;
  m_pipeline = static_cast<VulkanGraphicsPipeline *>(&pipeline);
  vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->handle());
}

void VulkanDevice::bindGroup(uint32_t set, rhi::IBindGroup &group) {
  if (set < kMaxSets)
    m_groups[set] = static_cast<VulkanBindGroup *>(&group);
}

void VulkanDevice::bindVertexBuffer(uint32_t slot, rhi::IBuffer &buffer,
                                    uint64_t byteOffset) {
  if (!m_rendering)
    return;
  auto &b = static_cast<VulkanBuffer &>(buffer);
  b.markUsed(m_core->recordingSerial);
  const VkBuffer handle = b.handle();
  const VkDeviceSize offset = byteOffset;
  vkCmdBindVertexBuffers(m_cmd, slot, 1, &handle, &offset);
}

void VulkanDevice::bindIndexBuffer(rhi::IBuffer &buffer, uint64_t byteOffset,
                                   rhi::IndexType type) {
  if (!m_rendering)
    return;
  auto &b = static_cast<VulkanBuffer &>(buffer);
  b.markUsed(m_core->recordingSerial);
  vkCmdBindIndexBuffer(m_cmd, b.handle(), byteOffset,
                       type == rhi::IndexType::U16 ? VK_INDEX_TYPE_UINT16
                                                   : VK_INDEX_TYPE_UINT32);
}

bool VulkanDevice::prepareDraw(const char *what) {
  if (!m_rendering || !m_pipeline || !m_pipeline->valid())
    return false;
  const auto &layouts = m_pipeline->setLayouts();
  for (uint32_t set = 0; set < layouts.size() && set < kMaxSets; ++set) {
    VulkanBindGroup *group = m_groups[set];
    if (!group || !group->valid() || group->layout() != layouts[set]) {
      if (!m_warnedUnbound) {
        LOG_ERROR("Vulkan: %s with set %u unbound or bound to a group of a "
                  "different layout",
                  what, set);
        m_warnedUnbound = true;
      }
      return false;
    }
    const VkDescriptorSet ds = group->prepare(m_slot);
    if (m_applied[set] != ds) {
      vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_pipeline->layout(), set, 1, &ds, 0, nullptr);
      m_applied[set] = ds;
    }
  }
  return true;
}

void VulkanDevice::draw(uint32_t vertexCount, uint32_t instanceCount,
                        uint32_t firstVertex, uint32_t firstInstance) {
  if (prepareDraw("draw"))
    vkCmdDraw(m_cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanDevice::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                               uint32_t firstIndex, int32_t vertexOffset,
                               uint32_t firstInstance) {
  if (prepareDraw("drawIndexed"))
    vkCmdDrawIndexed(m_cmd, indexCount, instanceCount, firstIndex, vertexOffset,
                     firstInstance);
}

void VulkanDevice::drawIndexedIndirect(rhi::IBuffer &commands, uint64_t offset,
                                       uint32_t drawCount, uint32_t stride) {
  if (!prepareDraw("drawIndexedIndirect"))
    return;
  auto &b = static_cast<VulkanBuffer &>(commands);
  b.markUsed(m_core->recordingSerial);
  vkCmdDrawIndexedIndirect(m_cmd, b.handle(), offset, drawCount, stride);
}

void VulkanDevice::bindComputePipeline(rhi::IComputePipeline &) {
  LOG_ERROR("Vulkan: compute is not implemented yet");
}

void VulkanDevice::dispatch(uint32_t, uint32_t, uint32_t) {
  LOG_ERROR("Vulkan: compute is not implemented yet");
}

void VulkanDevice::memoryBarrier(rhi::Access, rhi::Access) {
  if (!m_cmdOpen || m_rendering)
    return;
  VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
  b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
  b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
  VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep.memoryBarrierCount = 1;
  dep.pMemoryBarriers = &b;
  vkCmdPipelineBarrier2(m_cmd, &dep);
}

void VulkanDevice::copyBuffer(rhi::IBuffer &src, rhi::IBuffer &dst,
                              uint64_t srcOffset, uint64_t dstOffset,
                              uint64_t bytes) {
  if (m_rendering || !ensureCommandBuffer()) {
    LOG_ERROR("Vulkan: copyBuffer needs an open frame outside a rendering");
    return;
  }
  auto &from = static_cast<VulkanBuffer &>(src);
  auto &to = static_cast<VulkanBuffer &>(dst);
  from.markUsed(m_core->recordingSerial);
  to.markUsed(m_core->recordingSerial);
  const VkBufferCopy region{srcOffset, dstOffset, bytes};
  vkCmdCopyBuffer(m_cmd, from.handle(), to.handle(), 1, &region);
}

// Dear ImGui

void VulkanDevice::initImGui(void *windowHandle) {
  ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow *>(windowHandle), true);

  ImGui_ImplVulkan_InitInfo info = {};
  info.ApiVersion = VK_API_VERSION_1_3;
  info.Instance = m_core->instance;
  info.PhysicalDevice = m_core->physical;
  info.Device = m_core->device;
  info.QueueFamily = m_core->queueFamily;
  info.Queue = m_core->queue;
  info.DescriptorPoolSize =
      IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE * 4;
  info.MinImageCount =
      std::max<uint32_t>(2, static_cast<uint32_t>(m_images.size()));
  info.ImageCount = std::max<uint32_t>(info.MinImageCount,
                                       static_cast<uint32_t>(m_images.size()));
  info.UseDynamicRendering = true;
  info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.PipelineInfoMain.PipelineRenderingCreateInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &m_swapFormat;
  ImGui_ImplVulkan_Init(&info);
  m_imguiReady = true;
}

void VulkanDevice::imguiNewFrame() {
  if (m_imguiReady)
    ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
}

void VulkanDevice::imguiRender() {
  ImDrawData *data = ImGui::GetDrawData();
  if (!m_imguiReady || !data || data->DisplaySize.x <= 0 ||
      data->DisplaySize.y <= 0 || !m_frameOpen)
    return;
  // The UI goes on top of the finished scene in a colour-only pass.
  if (m_rendering)
    endRendering();
  rhi::RenderingDesc ui;
  ui.color.load = rhi::LoadOp::Load;
  ui.hasDepth = false;
  beginRendering(ui);
  ImGui_ImplVulkan_RenderDrawData(data, m_cmd);
  endRendering();
  // ImGui bound its own pipeline and sets.
  m_pipeline = nullptr;
  for (VkDescriptorSet &set : m_applied)
    set = VK_NULL_HANDLE;
}

void VulkanDevice::shutdownImGui() {
  if (!m_imguiReady)
    return;
  vkDeviceWaitIdle(m_core->device);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  m_imguiReady = false;
}

} // namespace componeng::renderer::vulkan
