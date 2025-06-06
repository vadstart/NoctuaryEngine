#include "astral_app.hpp"
#include "generic_render_system.hpp"
#include "glm/geometric.hpp"
#include "nt_camera.hpp"
#include "nt_buffer.hpp"
#include "nt_descriptors.hpp"
#include "nt_frame_info.hpp"
#include "nt_image.hpp"
#include "nt_input.hpp"
#include "nt_types.hpp"
#include "nt_utils.hpp"

#include <chrono>
#include <cstdint>
#include <glm/fwd.hpp>

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

struct GlobalUbo {
  glm::mat4 projectionView{1.f};
  glm::vec3 lightDirection = glm::normalize(glm::vec3{-3.f, -5.f, -6.f});
};

namespace nt
{

AstralApp::AstralApp() 
{
  // Setup ImGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan(ntWindow.getGLFWwindow(), true);
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

  globalPool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .build();

  loadGameObjects();
}
AstralApp::~AstralApp() {
  
}

void AstralApp::run() {
  std::vector<std::unique_ptr<NtBuffer>> uboBuffers(NtSwapChain::MAX_FRAMES_IN_FLIGHT);
  for (int i = 0; i < uboBuffers.size(); i++) {
    uboBuffers[i] = std::make_unique<NtBuffer> (
      ntDevice,
      sizeof(GlobalUbo),
      NtSwapChain::MAX_FRAMES_IN_FLIGHT,
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    uboBuffers[i]->map();
  }

  auto globalSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .build();

  std::shared_ptr<NtImage> globalTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/viking_room.png"));
    
  std::vector<VkDescriptorSet> globalDescriptorSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT);
  for(int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = globalTexture->getImageView();
    imageInfo.sampler = globalTexture->getSampler();

    NtDescriptorWriter(*globalSetLayout, *globalPool)
      .writeBuffer(0, &bufferInfo)
      .writeImage(1, &imageInfo)
      .build(globalDescriptorSets[i]);

  }


  GenericRenderSystem genericRenderSystem(ntDevice, ntRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout());
  NtCamera camera{};
  
  auto viewerObject = NtGameObject::createGameObject();
  viewerObject.transform.rotation = {glm::radians(-25.0f), glm::radians(-135.0f), 0};
  auto targetObject = NtGameObject::createGameObject();
  targetObject.transform.translation = {-0.05f, -.3f, 0};
  NtInputController inputController{};

  // Temporary implementation of DeltaTime
  auto currentTime = std::chrono::high_resolution_clock::now();

  while (!ntWindow.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime; 

    static int cameraType = 0;
    static bool autoRotate = false;
    static float autoRotateSpeed = glm::radians(30.0f); // degrees per second
    
    // Start the ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    // TODO: Refactor inputs and camera controls
    // if (!ntWindow.getShowCursor()) {
    inputController.update(
      &ntWindow,
      viewerObject,
      targetObject,
      deltaTime,
      io.MouseWheel,
      static_cast<nt::CameraType>(cameraType)
    );

    if (autoRotate) {
      viewerObject.transform.rotation.y += autoRotateSpeed * deltaTime;
    }

    // camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
    camera.setViewTarget(viewerObject.transform.translation, targetObject.transform.translation);
    // }

    if (ntWindow.getShowImGUI())
    {
        ImGuiWindowFlags imgui_window_flags = 0;
        ImGui::Begin("(=^-w-^=)", nullptr, imgui_window_flags);                          
        ImGui::Text("%.3f ms/frame | %.1f FPS ", 1000.0f / io.Framerate, io.Framerate);

        const char* renderModeItems[] = { "Lit", "Unlit", "Normals", "LitWireframe", "Wireframe" };
        static int renderModeCurrent = 0;
        ImGui::Combo("View", &renderModeCurrent, renderModeItems, IM_ARRAYSIZE(renderModeItems));
        genericRenderSystem.switchRenderMode(static_cast<RenderMode>(renderModeCurrent));

        ImGui::Checkbox("Auto Rotate", &autoRotate);
        if (ImGui::Button("Reset Target")) {
            targetObject.transform.translation = {-0.05f, -.3f, 0.0f};
        }

        int winW, winH, fbW, fbH;
        glfwGetWindowSize(ntWindow.getGLFWwindow(), &winW, &winH);
        glfwGetFramebufferSize(ntWindow.getGLFWwindow(), &fbW, &fbH);

        ImGui::Text("Window: X %.1u | Y %.1u", winW, winH);
        ImGui::Text("Framebuffer: X %.1u | Y %.1u", fbW, fbH);
        double xpos, ypos;
        glfwGetCursorPos(ntWindow.getGLFWwindow(), &xpos, &ypos);
        ImGui::Text("Mouse: X %.1f | Y %.1f", xpos, ypos);

        ImGui::RadioButton("Perspective", &cameraType, 0); ImGui::SameLine();
        ImGui::RadioButton("Orthographic", &cameraType, 1);

        ImGui::Text("Camera position: %.1f %.1f %.1f", viewerObject.transform.translation.x, viewerObject.transform.translation.y, viewerObject.transform.translation.z);
        ImGui::Text("Camera rotation: %.1f %.1f %.1f", viewerObject.transform.rotation.x, viewerObject.transform.rotation.y, viewerObject.transform.rotation.z);

        uint32_t totalVertexCount = 0;  
        for (const auto& gobject : gameObjects) {
          if (gobject.model) {
            totalVertexCount += gobject.model->getVertexCount();
          }
        }
        ImGui::Text("Vertex count: %u", totalVertexCount);

        ImGui::End();
    }

    float aspect = ntRenderer.getAspectRatio();

    if (!cameraType) {
      camera.setPerspectiveProjection(glm::radians(45.f), aspect, 0.1f, 100.f);
    }
    else
    {
      float zoom = inputController.orthoZoomLevel; // ← from inputController
      float halfHeight = zoom;
      float halfWidth = aspect * halfHeight;
      camera.setOrthographicProjection(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 100.f);
    }
    

    if (auto commandBuffer = ntRenderer.beginFrame()) {
      // TODO: Add Reflections, Shadows, Postprocessing, etc
      int frameIndex = ntRenderer.getFrameIndex();
      FrameInfo frameInfo {
        frameIndex,
        deltaTime,
        commandBuffer,
        camera,
        globalDescriptorSets[frameIndex]
      };

      //update
      GlobalUbo ubo{};
      ubo.projectionView = camera.getProjection() * camera.getView();
      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      ntRenderer.beginSwapChainRenderPass(commandBuffer);
      genericRenderSystem.renderGameObjects(frameInfo, gameObjects, viewerObject.transform.translation);

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

// Temp gameObj creation helper
std::unique_ptr<NtModel> createGameObjPlane(NtDevice& device, float size) {
  NtModel::Data modelData{};

  modelData.vertices = {
    {{-size, 0.f, -size}, glm::vec3(0.2), glm::vec3(0), {1.0f, 0.0f}},
    {{ size, 0.f, -size}, glm::vec3(0.2), glm::vec3(0), {0.0f, 0.0f}},
    {{ size, 0.f,  size}, glm::vec3(0.2), glm::vec3(0), {0.0f, 1.0f}},
    {{-size, 0.f,  size}, glm::vec3(0.2), glm::vec3(0), {1.0f, 1.0f}},
  };

  modelData.indices = {
    0, 1, 2,
    2, 3, 0,
  };

  return std::make_unique<NtModel>(device, modelData);
}

std::unique_ptr<NtModel> createGameObjCube(NtDevice& device, glm::vec3 offset) {
  NtModel::Data modelData{};

  // Define the 8 corners of the gameObj and assign each a unique color (rainbow-ish)
  modelData.vertices = {
    {{-.5f, -.5f, -.5f}, {1.0f, 0.0f, 0.0f}}, // 0: red
    {{.5f, -.5f, -.5f}, {1.0f, 0.5f, 0.0f}},  // 1: orange
    {{.5f, .5f, -.5f}, {1.0f, 1.0f, 0.0f}},   // 2: yellow
    {{-.5f, .5f, -.5f}, {0.0f, 1.0f, 0.0f}},  // 3: green
    {{-.5f, -.5f, .5f}, {0.0f, 0.0f, 1.0f}},  // 4: blue
    {{.5f, -.5f, .5f}, {0.29f, 0.0f, 0.51f}}, // 5: indigo
    {{.5f, .5f, .5f}, {0.56f, 0.0f, 1.0f}},   // 6: violet
    {{-.5f, .5f, .5f}, {1.0f, 0.0f, 1.0f}},   // 7: magenta
  };

  // Indices into the `corners` array to form 12 triangles (2 per face)
  modelData.indices = {
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

  for (auto& v : modelData.vertices) {
    v.position += offset;
  }

  return std::make_unique<NtModel>(device, modelData);
}

void AstralApp::loadGameObjects() {
  // Debug world grid
  auto gameObj = NtGameObject::createGameObject();
  gameObj.model = createGameObjPlane(ntDevice, 1000.0f);
  gameObjects.push_back(std::move(gameObj));

  auto go_VikingRoom = NtGameObject::createGameObject();
  go_VikingRoom.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/viking_room.obj"));
  // go_VikingRoom.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/viking_room.png"));

  go_VikingRoom.transform.rotation = {0.0f, glm::radians(180.0f), 0.0f};
  gameObjects.push_back(std::move(go_VikingRoom));
}

}
