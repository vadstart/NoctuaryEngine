#include "astral_app.h"
#include "generic_render_system.h"
#include "nt_camera.h"

#include <chrono>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "vulkan/vulkan_core.h"

// Libraries
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// Std
#include <iostream>
#include <cassert>
#include <memory>

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

namespace nt
{

AstralApp::AstralApp() {

  // static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;
  // VkDescriptorPoolSize pool_sizes[] =
  // {
  //     { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
  // };
  // VkDescriptorPoolCreateInfo pool_info = {};
  // pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  // pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  // pool_info.maxSets = 0;
  // for (VkDescriptorPoolSize& pool_size : pool_sizes)
  //     pool_info.maxSets += pool_size.descriptorCount;
  // pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
  // pool_info.pPoolSizes = pool_sizes;
  // VkResult err = vkCreateDescriptorPool(ntDevice.device(), &pool_info, nullptr, &g_DescriptorPool);
  // check_vk_result(err);

  // Setup ImGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(ntWindow.window(), true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  //init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
  init_info.Instance = ntDevice.instance();
  init_info.PhysicalDevice = ntDevice.physicalDevice();
  init_info.Device = ntDevice.device();
  // Select graphics queue family
  static uint32_t g_QueueFamily = (uint32_t)-1;
  g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(init_info.PhysicalDevice);
  IM_ASSERT(g_QueueFamily != (uint32_t)-1);
  init_info.QueueFamily = g_QueueFamily;
  init_info.Queue = ntDevice.graphicsQueue();
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
  // init_info.DescriptorPool = g_DescriptorPool;
  init_info.RenderPass = ntRenderer.getSwapChainRenderPass();
  init_info.Subpass = 0;
  init_info.MinImageCount = 2;
  init_info.ImageCount = ntRenderer.getSwapChainImageCount();
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  ImGui_ImplVulkan_Init(&init_info);

  loadGameObjects();
}
AstralApp::~AstralApp() {
  
}


void AstralApp::run() {
  GenericRenderSystem genericRenderSystem(ntDevice, ntRenderer.getSwapChainRenderPass());
  NtCamera camera{};

  std::cout << "maxPushConstantSize = " << ntDevice.properties.limits.maxPushConstantsSize << "\n";

  // Temporary implementation of DeltaTime
  auto currentTime = std::chrono::high_resolution_clock::now();

  while (!ntWindow.shouldClose()) {
    auto newTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // if (show_imgui)
    {
        ImGuiWindowFlags imgui_window_flags = 0;
        imgui_window_flags |= ImGuiWindowFlags_NoResize;
        ImGui::Begin("(=^-w-^=)", nullptr, imgui_window_flags);                          
        ImGui::Text("%.3f ms/frame | %.1f FPS", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        const char* items[] = { "Solid", "WirePoly", "WireLines" };
        static int item_current = 0;
        ImGui::Combo("Render", &item_current, items, IM_ARRAYSIZE(items));
        
        ImGui::End();
    }

    float aspect = ntRenderer.getAspectRatio();
    // camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
    camera.setPerspectiveProjection(glm::radians(45.f), aspect, 0.1f, 10.f);

    if (auto commandBuffer = ntRenderer.beginFrame()) {
      // TODO: Add Reflections, Shadows, Postprocessing, etc
      
      ntRenderer.beginSwapChainRenderPass(commandBuffer);
      genericRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera, deltaTime);
      ImGui::Render();
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ntRenderer.getCurrentCommandBuffer());
      ntRenderer.endSwapChainRenderPass(commandBuffer);
      ntRenderer.endFrame();
    }
  }

  vkDeviceWaitIdle(ntDevice.device());

  // IMGUI Cleanup
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

// Temp cube creation helper
std::unique_ptr<NtModel> createCubeModel(NtDevice& device, glm::vec3 offset) {
  using Vertex = NtModel::Vertex;

  // Define the 8 corners of the cube and assign each a unique color (rainbow-ish)
  std::vector<Vertex> corners = {
    Vertex{{-.5f, -.5f, -.5f}, {1.0f, 0.0f, 0.0f}}, // 0: red
    Vertex{{.5f, -.5f, -.5f}, {1.0f, 0.5f, 0.0f}},  // 1: orange
    Vertex{{.5f, .5f, -.5f}, {1.0f, 1.0f, 0.0f}},   // 2: yellow
    Vertex{{-.5f, .5f, -.5f}, {0.0f, 1.0f, 0.0f}},  // 3: green
    Vertex{{-.5f, -.5f, .5f}, {0.0f, 0.0f, 1.0f}},  // 4: blue
    Vertex{{.5f, -.5f, .5f}, {0.29f, 0.0f, 0.51f}}, // 5: indigo
    Vertex{{.5f, .5f, .5f}, {0.56f, 0.0f, 1.0f}},   // 6: violet
    Vertex{{-.5f, .5f, .5f}, {1.0f, 0.0f, 1.0f}},   // 7: magenta
  };

  // Indices into the `corners` array to form 12 triangles (2 per face)
  std::vector<uint32_t> indices = {
    // front face (z = +0.5)
    4, 5, 6,
    4, 6, 7,
    // back face (z = -0.5)
    0, 2, 1,
    0, 3, 2,
    // left face (x = -0.5)
    0, 7, 3,
    0, 4, 7,
    // right face (x = +0.5)
    1, 2, 6,
    1, 6, 5,
    // top face (y = +0.5)
    3, 7, 6,
    3, 6, 2,
    // bottom face (y = -0.5)
    0, 1, 5,
    0, 5, 4,
  };

  std::vector<Vertex> vertices;
  for (auto index : indices) {
    Vertex v = corners[index];
    v.position += offset;
    vertices.push_back(v);
  }

  return std::make_unique<NtModel>(device, vertices);
}

void AstralApp::loadGameObjects() {
  std::shared_ptr<NtModel> ntModel = createCubeModel(ntDevice, {.0f, .0f, .0f});

  auto cube = NtGameObject::createGameObject();
  cube.model = ntModel;
  cube.transform.translation = {.0f, .0f, 1.25f};
  cube.transform.scale = {.5f, .5f, .5f};

  gameObjects.push_back(std::move(cube));
}

}
