//{{{
// Dear ImGui: standalone example application for Glfw + Vulkan
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.
//}}}
//{{{  includes
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <stdio.h>
#include <stdlib.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
//}}}

static VkAllocationCallbacks*   gAllocator = NULL;
static VkInstance               gInstance = VK_NULL_HANDLE;
static VkPhysicalDevice         gPhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 gDevice = VK_NULL_HANDLE;
static uint32_t                 gQueueFamily = (uint32_t)-1;
static VkQueue                  gQueue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT gDebugReport = VK_NULL_HANDLE;
static VkPipelineCache          gPipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         gDescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window gMainWindowData;
static int                      gMinImageCount = 2;
static bool                     gSwapChainRebuild = false;

#ifdef VALIDATION
  //{{{
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReport (VkDebugReportFlagsEXT flags,
                                                     VkDebugReportObjectTypeEXT objectType,
                                                     uint64_t object,
                                                     size_t location,
                                                     int32_t messageCode,
                                                     const char* pLayerPrefix,
                                                     const char* pMessage,
                                                     void* pUserData) {

    (void)flags;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pUserData;
    (void)pLayerPrefix; // Unused arguments

    printf ("vkDebugReport type:%i:%s\n", objectType, pMessage);
    return VK_FALSE;
    }
  //}}}
#endif

//{{{
static void glfwErrorCallback (int error, const char* description) {
  printf ("glfwError:%d:%s\n", error, description);
  }
//}}}
//{{{
static void checkVkResult (VkResult result) {

  if (result == 0)
    return;

  printf ("vkResultError:%d\n", result);

  if (result < 0)
    abort();
  }
//}}}

//{{{
static void setupVulkan (const char** extensions, uint32_t numExtensions) {

  // create Vulkan Instance
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.enabledExtensionCount = numExtensions;
  instanceCreateInfo.ppEnabledExtensionNames = extensions;

  VkResult result;

  #ifdef VALIDATION
    //{{{  create with validation layers
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = layers;

    // Enable debug report extension (we need additional storage
    // , so we duplicate the user array to add our new extension to it)
    const char** extensionsExt = (const char**)malloc (sizeof(const char*) * (numExtensions + 1));
    memcpy (extensionsExt, extensions, numExtensions * sizeof(const char*));
    extensionsExt[numExtensions] = "VK_EXT_debug_report";
    instanceCreateInfo.enabledExtensionCount = numExtensions + 1;
    instanceCreateInfo.ppEnabledExtensionNames = extensionsExt;

    // Create Vulkan Instance
    result = vkCreateInstance (&instanceCreateInfo, gAllocator, &gInstance);
    checkVkResult (result);
    free (extensionsExt);

    // Get the function pointer (required for any extensions)
    auto vkCreateDebugReportCallbackEXT =
      (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr (gInstance, "vkCreateDebugReportCallbackEXT");
    IM_ASSERT (vkCreateDebugReportCallbackEXT != NULL);

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                            VK_DEBUG_REPORT_WARNING_BIT_EXT |
                            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debugReport;
    debug_report_ci.pUserData = NULL;

    result = vkCreateDebugReportCallbackEXT (gInstance, &debug_report_ci, gAllocator, &gDebugReport);
    checkVkResult (result);
    //}}}
  #else
    // create without validation layers
    result = vkCreateInstance (&instanceCreateInfo, gAllocator, &gInstance);
    checkVkResult (result);
    IM_UNUSED (gDebugReport);
  #endif

  //{{{  select gpu
  #define VK_API_VERSION_VARIANT(version) ((uint32_t)(version) >> 29)
  #define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
  #define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
  #define VK_API_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)

  uint32_t numGpu;
  result = vkEnumeratePhysicalDevices (gInstance, &numGpu, NULL);
  checkVkResult (result);

  if (!numGpu)
    printf ("queueFamilyCount zero\n");
  IM_ASSERT (numGpu > 0);

  VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc (sizeof(VkPhysicalDevice) * numGpu);
  result = vkEnumeratePhysicalDevices (gInstance, &numGpu, gpus);
  checkVkResult (result);

  for (uint32_t i = 0; i < numGpu; i++) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties (gpus[i], &properties);
    printf ("gpu:%d var:%d major:%d minor:%d patch:%d type:%d %s api:%x driver:%d\n",
            (int)i,
            VK_API_VERSION_VARIANT(properties.apiVersion),
            VK_API_VERSION_MAJOR(properties.apiVersion),
            VK_API_VERSION_MINOR(properties.apiVersion),
            VK_API_VERSION_PATCH(properties.apiVersion),
            properties.deviceType, properties.deviceName, properties.apiVersion, properties.driverVersion);
    }

  // If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available
  // This covers most common cases (multi-gpu/integrated+dedicated graphics)
  // Handling more complicated setups (multiple dedicated GPUs) is out of scope of this sample.
  int useGpu = 0;
  for (uint32_t i = 0; i < numGpu; i++) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties (gpus[i], &properties);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      useGpu = i;
      break;
      }
    }

  gPhysicalDevice = gpus[useGpu];
  printf ("useGpu:%d\n", (int)useGpu);

  free (gpus);
  //}}}
  //{{{  select graphics queue family
  uint32_t numQueueFamily;
  vkGetPhysicalDeviceQueueFamilyProperties (gPhysicalDevice, &numQueueFamily, NULL);

  if (!numQueueFamily)
    printf ("queueFamilyCount zero\n");

  VkQueueFamilyProperties* queueFamilyProperties =
    (VkQueueFamilyProperties*)malloc (sizeof(VkQueueFamilyProperties) * numQueueFamily);
  vkGetPhysicalDeviceQueueFamilyProperties (gPhysicalDevice, &numQueueFamily, queueFamilyProperties);

  for (uint32_t i = 0; i < numQueueFamily; i++)
    printf ("queue:%d count:%d queueFlags:%x\n",
             i,
             queueFamilyProperties[i].queueCount,
             queueFamilyProperties[i].queueFlags);

  for (uint32_t i = 0; i < numQueueFamily; i++)
    if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      gQueueFamily = i;
      break;
      }

  free (queueFamilyProperties);

  IM_ASSERT(gQueueFamily != (uint32_t)-1);
  //}}}
  //{{{  create logical device (with 1 queue)
  int device_extension_count = 1;

  const char* device_extensions[] = { "VK_KHR_swapchain" };
  const float queue_priority[] = { 1.0f };

  VkDeviceQueueCreateInfo deviceQueueCreateInfo[1] = {};
  deviceQueueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfo[0].queueFamilyIndex = gQueueFamily;
  deviceQueueCreateInfo[0].queueCount = 1;
  deviceQueueCreateInfo[0].pQueuePriorities = queue_priority;

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = sizeof(deviceQueueCreateInfo) / sizeof(deviceQueueCreateInfo[0]);
  deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;
  deviceCreateInfo.enabledExtensionCount = device_extension_count;
  deviceCreateInfo.ppEnabledExtensionNames = device_extensions;

  result = vkCreateDevice (gPhysicalDevice, &deviceCreateInfo, gAllocator, &gDevice);
  checkVkResult(result);

  vkGetDeviceQueue (gDevice, gQueueFamily, 0, &gQueue);
  //}}}
  //{{{  create descriptor pool
  {
  VkDescriptorPoolSize descriptorPoolSizes[] = {
    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolCreateInfo.maxSets = 1000 * IM_ARRAYSIZE(descriptorPoolSizes);
  descriptorPoolCreateInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(descriptorPoolSizes);
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

  result = vkCreateDescriptorPool (gDevice, &descriptorPoolCreateInfo, gAllocator, &gDescriptorPool);
  checkVkResult (result);
  }
  //}}}
  }
//}}}
//{{{
static void setupVulkanWindow (ImGui_ImplVulkanH_Window* vulkanWindow, VkSurfaceKHR surface, int width, int height) {
// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.

  vulkanWindow->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR (gPhysicalDevice, gQueueFamily, vulkanWindow->Surface, &res);
  if (res != VK_TRUE) {
    printf ("Error no WSI support on physical device 0\n");
    exit (-1);
    }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM,
                                                 VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_FORMAT_B8G8R8_UNORM,
                                                 VK_FORMAT_R8G8B8_UNORM };
  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  vulkanWindow->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat (gPhysicalDevice,
                                                                       vulkanWindow->Surface,
                                                                       requestSurfaceImageFormat,
                                                                       (size_t)IM_ARRAYSIZE (requestSurfaceImageFormat),
                                                                        requestSurfaceColorSpace);
// Select Present Mode
  #ifdef VSYNC
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
  #else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR,
                                         VK_PRESENT_MODE_IMMEDIATE_KHR,
                                         VK_PRESENT_MODE_FIFO_KHR };
  #endif

  vulkanWindow->PresentMode = ImGui_ImplVulkanH_SelectPresentMode (gPhysicalDevice,
                                                                   vulkanWindow->Surface,
                                                                   &present_modes[0], IM_ARRAYSIZE(present_modes));
  printf ("Selected PresentMode = %d\n", vulkanWindow->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT (gMinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow (gInstance, gPhysicalDevice, gDevice,
                                          vulkanWindow, gQueueFamily, gAllocator, width, height, gMinImageCount);
  }
//}}}
//{{{
static void uploadFonts (ImGui_ImplVulkanH_Window* vulkanWindow) {
//  upload fonts texture

  VkCommandPool commandPool = vulkanWindow->Frames[vulkanWindow->FrameIndex].CommandPool;
  VkCommandBuffer commandBuffer = vulkanWindow->Frames[vulkanWindow->FrameIndex].CommandBuffer;

  VkResult result = vkResetCommandPool (gDevice, commandPool, 0);
  checkVkResult (result);

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  result = vkBeginCommandBuffer (commandBuffer, &begin_info);
  checkVkResult (result);

  ImGui_ImplVulkan_CreateFontsTexture (commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  result = vkEndCommandBuffer (commandBuffer);
  checkVkResult (result);

  result = vkQueueSubmit (gQueue, 1, &submitInfo, VK_NULL_HANDLE);
  checkVkResult (result);

  result = vkDeviceWaitIdle (gDevice);
  checkVkResult (result);

  ImGui_ImplVulkan_DestroyFontUploadObjects();
  }
//}}}

//{{{
static void cleanupVulkan() {

  vkDestroyDescriptorPool (gDevice, gDescriptorPool, gAllocator);

  #ifdef VALIDATION
    // Remove the debug report callback
    auto vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr (gInstance, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT (gInstance, gDebugReport, gAllocator);
  #endif

  vkDestroyDevice (gDevice, gAllocator);
  vkDestroyInstance (gInstance, gAllocator);
  }
//}}}
//{{{
static void cleanupVulkanWindow() {
  ImGui_ImplVulkanH_DestroyWindow (gInstance, gDevice, &gMainWindowData, gAllocator);
  }
//}}}

//{{{
static void renderDrawData (ImGui_ImplVulkanH_Window* vulkanWindow, ImDrawData* draw_data) {


  VkSemaphore imageAcquiredSem = vulkanWindow->FrameSemaphores[vulkanWindow->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore renderCompleteSem = vulkanWindow->FrameSemaphores[vulkanWindow->SemaphoreIndex].RenderCompleteSemaphore;

  VkResult result = vkAcquireNextImageKHR (gDevice, vulkanWindow->Swapchain, UINT64_MAX,
                                        imageAcquiredSem, VK_NULL_HANDLE, &vulkanWindow->FrameIndex);
  if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
    gSwapChainRebuild = true;
    return;
    }
  checkVkResult (result);

  ImGui_ImplVulkanH_Frame* vulkanFrame = &vulkanWindow->Frames[vulkanWindow->FrameIndex];

  // wait indefinitely instead of periodically checking
  result = vkWaitForFences (gDevice, 1, &vulkanFrame->Fence, VK_TRUE, UINT64_MAX);
  checkVkResult (result);

  result = vkResetFences (gDevice, 1, &vulkanFrame->Fence);
  checkVkResult(result);

  result = vkResetCommandPool (gDevice, vulkanFrame->CommandPool, 0);
  checkVkResult (result);

  VkCommandBufferBeginInfo commandBufferBeginInfo = {};
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  result = vkBeginCommandBuffer (vulkanFrame->CommandBuffer, &commandBufferBeginInfo);
  checkVkResult (result);

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.renderPass = vulkanWindow->RenderPass;
  renderPassBeginInfo.framebuffer = vulkanFrame->Framebuffer;
  renderPassBeginInfo.renderArea.extent.width = vulkanWindow->Width;
  renderPassBeginInfo.renderArea.extent.height = vulkanWindow->Height;
   renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues = &vulkanWindow->ClearValue;

  vkCmdBeginRenderPass (vulkanFrame->CommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  // record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData (draw_data, vulkanFrame->CommandBuffer);

  // submit command buffer
  vkCmdEndRenderPass (vulkanFrame->CommandBuffer);

  VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAcquiredSem;
  submitInfo.pWaitDstStageMask = &pipelineStageFlags;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &vulkanFrame->CommandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderCompleteSem;

  result = vkEndCommandBuffer (vulkanFrame->CommandBuffer);
  checkVkResult (result);

  result = vkQueueSubmit (gQueue, 1, &submitInfo, vulkanFrame->Fence);
  checkVkResult (result);
  }
//}}}
//{{{
static void present (ImGui_ImplVulkanH_Window* vulkanWindow) {

  if (gSwapChainRebuild)
    return;

  VkSemaphore renderCompleteSem = vulkanWindow->FrameSemaphores[vulkanWindow->SemaphoreIndex].RenderCompleteSemaphore;

  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &renderCompleteSem;
  info.swapchainCount = 1;
  info.pSwapchains = &vulkanWindow->Swapchain;
  info.pImageIndices = &vulkanWindow->FrameIndex;

  VkResult result = vkQueuePresentKHR (gQueue, &info);
  if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
    gSwapChainRebuild = true;
    return;
    }

  checkVkResult (result);

  // now we can use next set of semaphores
  vulkanWindow->SemaphoreIndex = (vulkanWindow->SemaphoreIndex + 1) % vulkanWindow->ImageCount;
  }
//}}}

//{{{
int main (int, char**) {

  // setup glfw
  glfwSetErrorCallback (glfwErrorCallback);
  if (!glfwInit())
    return 1;

  // setup glfw window
  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* glfwWindow = glfwCreateWindow (1280, 720, "Dear ImGui GLFW+Vulkan example", NULL, NULL);

  // setup vulkan
  if (!glfwVulkanSupported()) {
    printf ("GLFW: Vulkan Not Supported\n");
    return 1;
    }

  // get glfw required vulkan extensions
  uint32_t extensionsCount = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions (&extensionsCount);
  for (uint32_t i = 0; i < extensionsCount; i++)
    printf ("glfwVulkanExt:%d %s\n", int(i), extensions[i]);
  setupVulkan (extensions, extensionsCount);

  // create windowSurface
  VkSurfaceKHR surface;
  VkResult result = glfwCreateWindowSurface (gInstance, glfwWindow, gAllocator, &surface);
  checkVkResult (result);

  // create framebuffers
  int width, height;
  glfwGetFramebufferSize (glfwWindow, &width, &height);
  ImGui_ImplVulkanH_Window* vulkanWindow = &gMainWindowData;
  setupVulkanWindow (vulkanWindow, surface, width, height);

  // setup imGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

  #ifdef DOCKING
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
  #endif
  //io.ConfigViewportsNoAutoMerge = true;
  //io.ConfigViewportsNoTaskBarIcon = true;

  ImGui::StyleColorsDark();

  #ifdef DOCKING
    ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
      }
  #endif

  // setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan (glfwWindow, true);

  ImGui_ImplVulkan_InitInfo vulkanInitInfo = {};
  vulkanInitInfo.Instance = gInstance;
  vulkanInitInfo.PhysicalDevice = gPhysicalDevice;
  vulkanInitInfo.Device = gDevice;
  vulkanInitInfo.QueueFamily = gQueueFamily;
  vulkanInitInfo.Queue = gQueue;
  vulkanInitInfo.PipelineCache = gPipelineCache;
  vulkanInitInfo.DescriptorPool = gDescriptorPool;
  vulkanInitInfo.Allocator = gAllocator;
  vulkanInitInfo.MinImageCount = gMinImageCount;
  vulkanInitInfo.ImageCount = vulkanWindow->ImageCount;
  vulkanInitInfo.CheckVkResultFn = checkVkResult;
  ImGui_ImplVulkan_Init (&vulkanInitInfo, vulkanWindow->RenderPass);

  uploadFonts (vulkanWindow);

  // Our state
  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clearColor = ImVec4 (0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  while (!glfwWindowShouldClose (glfwWindow)) {
    glfwPollEvents();

    // Resize swap chain?
    if (gSwapChainRebuild) {
      int width;
      int height;
      glfwGetFramebufferSize (glfwWindow, &width, &height);
      if ((width > 0) && (height > 0)) {
        ImGui_ImplVulkan_SetMinImageCount (gMinImageCount);
        ImGui_ImplVulkanH_CreateOrResizeWindow (gInstance, gPhysicalDevice, gDevice,
                                                &gMainWindowData, gQueueFamily,
                                                gAllocator, width, height, gMinImageCount);
        gMainWindowData.FrameIndex = 0;
        gSwapChainRebuild = false;
        }
      }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
      ImGui::ShowDemoWindow (&show_demo_window);

    //{{{  Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin ("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

    ImGui::Text ("This is some useful text.");               // Display some text (you can use a format strings too)
    ImGui::Checkbox ("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
    ImGui::Checkbox ("Another Window", &show_another_window);

    ImGui::SliderFloat ("float", &f, 0.0f, 1.0f);           // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3 ("clear color", (float*)&clearColor); // Edit 3 floats representing a color

    if (ImGui::Button ("Button"))
      counter++;
    ImGui::SameLine();
    ImGui::Text ("counter = %d", counter);

    ImGui::Text ("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
    }
    //}}}
    //{{{  Show another simple window.
    if (show_another_window) {
      ImGui::Begin ("Another Window", &show_another_window);
      ImGui::Text ("Hello from another window!");
      if (ImGui::Button ("Close Me"))
        show_another_window = false;
      ImGui::End();
      }
    //}}}

    // Rendering
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    const bool minimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);
    vulkanWindow->ClearValue.color.float32[0] = clearColor.x * clearColor.w;
    vulkanWindow->ClearValue.color.float32[1] = clearColor.y * clearColor.w;
    vulkanWindow->ClearValue.color.float32[2] = clearColor.z * clearColor.w;
    vulkanWindow->ClearValue.color.float32[3] = clearColor.w;
    if (!minimized)
      renderDrawData (vulkanWindow, drawData);

    #ifdef DOCKING
      if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        }
    #endif

    if (!minimized)
      present (vulkanWindow);
    }

  // cleanup
  result = vkDeviceWaitIdle (gDevice);
  checkVkResult (result);

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  cleanupVulkanWindow();
  cleanupVulkan();

  glfwDestroyWindow (glfwWindow);
  glfwTerminate();

  return 0;
  }
//}}}
