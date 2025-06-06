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
    .build();

  modelPool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(100)
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .build();
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

  globalSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    .build();

  modelSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .build();
    
  std::vector<VkDescriptorSet> globalDescriptorSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT);
  for(int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();

    NtDescriptorWriter(*globalSetLayout, *globalPool)
      .writeBuffer(0, &bufferInfo)
      .build(globalDescriptorSets[i]);
  }

  GlobalUbo ubo{};

  GenericRenderSystem genericRenderSystem(ntDevice, ntRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout(), modelSetLayout->getDescriptorSetLayout());
  NtCamera camera{};
  
  auto viewerObject = NtGameObject::createGameObject();
  viewerObject.transform.rotation = {glm::radians(-25.0f), glm::radians(-135.0f), 0};
  auto targetObject = NtGameObject::createGameObject();
  targetObject.transform.translation = {-0.05f, -.3f, 0};
  NtInputController inputController{};

  // Debug world grid
  auto debugGridObject = NtGameObject::createGameObject();
  debugGridObject.model = createGOPlane(1000.0f);

  loadGameObjects();

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
        // ImGui::Text("%.3f ms/frame | %.1f FPS ", 1000.0f / io.Framerate, io.Framerate);
        
        static float frameTimes[120] = {};
        static int frameTimeOffset = 0;

        float frameTime = 1000.0f / io.Framerate; // ms
        frameTimes[frameTimeOffset] = frameTime;
        frameTimeOffset = (frameTimeOffset + 1) % IM_ARRAYSIZE(frameTimes);

        float maxFrameTime = 0.0f;
        float avgFrameTime = 0.0f;
        for (int i = 0; i < IM_ARRAYSIZE(frameTimes); ++i) {
            avgFrameTime += frameTimes[i];
            if (frameTimes[i] > maxFrameTime) maxFrameTime = frameTimes[i];
        }
        avgFrameTime /= IM_ARRAYSIZE(frameTimes);
        maxFrameTime = glm::clamp(maxFrameTime * 1.25f, 5.0f, 50.0f); // cap to avoid extreme spikes

        ImVec4 color;
        if (avgFrameTime < 16.8f) color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);        // Green (60+ FPS)
        else if (avgFrameTime < 33.3f) color = ImVec4(1.0f, 1.0f, 0.2f, 1.0f);   // Yellow (30–60 FPS)
        else color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);                             // Red (<30 FPS)

        char overlay[32];
        snprintf(overlay, sizeof(overlay), "avg %.2f ms", avgFrameTime);

        ImGui::PushStyleColor(ImGuiCol_PlotLines, color);
        ImGui::PlotLines("", frameTimes, IM_ARRAYSIZE(frameTimes), frameTimeOffset,
                         overlay, 0.0f, maxFrameTime, ImVec2(0, 40.0f));
        ImGui::PopStyleColor();

        ImGui::Text("Current FPS: %.1f", io.Framerate);


        const char* renderModeItems[] = { "Lit", "Unlit", "Normals", "LitWireframe", "Wireframe" };
        static int renderModeCurrent = 0;
        ImGui::Combo(" ", &renderModeCurrent, renderModeItems, IM_ARRAYSIZE(renderModeItems));
        ImGui::SetItemTooltip("View mode");
        genericRenderSystem.switchRenderMode(static_cast<RenderMode>(renderModeCurrent));

        // ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Camera"))
        {
          ImGui::Checkbox("Auto Rotate", &autoRotate);ImGui::SameLine();
          if (ImGui::Button("Reset")) {
              targetObject.transform.translation = {-0.05f, -.3f, 0.0f};
          }

          ImGui::RadioButton("Perspective", &cameraType, 0); ImGui::SameLine();
          ImGui::RadioButton("Orthographic", &cameraType, 1);

          ImGui::Text("Position: %.1f %.1f %.1f", viewerObject.transform.translation.x, viewerObject.transform.translation.y, viewerObject.transform.translation.z);
          ImGui::Text("Rotation: %.1f %.1f %.1f", viewerObject.transform.rotation.x, viewerObject.transform.rotation.y, viewerObject.transform.rotation.z);

          ImGui::TreePop();
        }

        if (ImGui::TreeNode("Lighting"))
        {
          const ImGuiSliderFlags flags_for_sliders = imgui_window_flags & ~ImGuiSliderFlags_WrapAround;
          ImGui::SliderFloat("X", &ubo.lightDirection.x, -1.0f, 1.0f, "%.2f", flags_for_sliders);
          ImGui::SliderFloat("Y", &ubo.lightDirection.y, -1.0f, 1.0f, "%.2f", flags_for_sliders);
          ImGui::SliderFloat("Z", &ubo.lightDirection.z, -1.0f, 1.0f, "%.2f", flags_for_sliders);

          ImGui::TreePop();
        }

        if (ImGui::TreeNode("Misc"))
        {
          int winW, winH, fbW, fbH;
          glfwGetWindowSize(ntWindow.getGLFWwindow(), &winW, &winH);
          glfwGetFramebufferSize(ntWindow.getGLFWwindow(), &fbW, &fbH);
          ImGui::Text("Window: X %.1u | Y %.1u", winW, winH);
          ImGui::Text("Framebuffer: X %.1u | Y %.1u", fbW, fbH);
          double xpos, ypos;
          glfwGetCursorPos(ntWindow.getGLFWwindow(), &xpos, &ypos);
          ImGui::Text("Mouse: X %.1f | Y %.1f", xpos, ypos);

          uint32_t totalVertexCount = 0;  
          for (const auto& gobject : gameObjects) {
            if (gobject.model) {
              totalVertexCount += gobject.model->getVertexCount();
            }
          }
          ImGui::Text("Vertex count: %u", totalVertexCount);

          ImGui::TreePop();
        }

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
      ubo.projectionView = camera.getProjection() * camera.getView();
      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      ntRenderer.beginSwapChainRenderPass(commandBuffer);
      genericRenderSystem.renderDebugGrid(frameInfo, debugGridObject, viewerObject.transform.translation);
      genericRenderSystem.renderGameObjects(frameInfo, gameObjects);

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

std::unique_ptr<NtModel> AstralApp::createGOPlane(float size) {
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

  return std::make_unique<NtModel>(ntDevice, modelData);
}

std::unique_ptr<NtModel> AstralApp::createGOCube(float size) {
  NtModel::Data modelData{};

  modelData.vertices = {
    {{-size, -size, -size}, {1.0f, 0.0f, 0.0f}}, // 0: red
    {{size, -size, -size}, {1.0f, 0.5f, 0.0f}},  // 1: orange
    {{size, size, -size}, {1.0f, 1.0f, 0.0f}},   // 2: yellow
    {{-size, size, -size}, {0.0f, 1.0f, 0.0f}},  // 3: green
    {{-size, -size, size}, {0.0f, 0.0f, 1.0f}},  // 4: blue
    {{size, -size, size}, {0.29f, 0.0f, 0.51f}}, // 5: indigo
    {{size, size, size}, {0.56f, 0.0f, 1.0f}},   // 6: violet
    {{-size, size, size}, {1.0f, 0.0f, 1.0f}},   // 7: magenta
  };

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

  return std::make_unique<NtModel>(ntDevice, modelData);
}

void AstralApp::loadGameObjects() {
  auto go_VikingRoom = NtGameObject::createGameObject();
  go_VikingRoom.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/viking_room.obj"));
  go_VikingRoom.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/viking_room.png"));
  // TODO: refactor
  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = go_VikingRoom.texture->getImageView();
  imageInfo.sampler = go_VikingRoom.texture->getSampler();
  bool success = NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &imageInfo)
      .build(go_VikingRoom.materialDescriptorSet);
  if (!success) {
      std::cerr << "Failed to build material descriptor set!" << std::endl;
  }
  go_VikingRoom.transform.rotation = {0.0f, glm::radians(180.0f), 0.0f};
  gameObjects.push_back(std::move(go_VikingRoom));



  auto go_Bunny = NtGameObject::createGameObject();
  go_Bunny.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/bunny_low.obj"));
  go_Bunny.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/bunny_albedo.jpeg"));

  // TODO: refactor
  // VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = go_Bunny.texture->getImageView();
  imageInfo.sampler = go_Bunny.texture->getSampler();
  NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &imageInfo)
      .build(go_Bunny.materialDescriptorSet);

  go_Bunny.transform.translation = {2.3f, 0.0f, 0.0f};
  gameObjects.push_back(std::move(go_Bunny));

  auto go_DewStalker_ground = NtGameObject::createGameObject();
  go_DewStalker_ground.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/dew_stalker_ground.obj"));
  go_DewStalker_ground.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/dew_stalker_ground.png"));
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = go_DewStalker_ground.texture->getImageView();
  imageInfo.sampler = go_DewStalker_ground.texture->getSampler();
  NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &imageInfo)
      .build(go_DewStalker_ground.materialDescriptorSet);
  go_DewStalker_ground.transform.translation = {-2.3f, 0.0f, 0.0f};
  gameObjects.push_back(std::move(go_DewStalker_ground));

  auto go_DewStalker_grass = NtGameObject::createGameObject();
  go_DewStalker_grass.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/dew_stalker_grass.obj"));
  go_DewStalker_grass.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/dew_stalker_grass.png"));
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = go_DewStalker_grass.texture->getImageView();
  imageInfo.sampler = go_DewStalker_grass.texture->getSampler();
  NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &imageInfo)
      .build(go_DewStalker_grass.materialDescriptorSet);
  go_DewStalker_grass.transform.translation = {-2.3f, 0.0f, 0.0f};
  gameObjects.push_back(std::move(go_DewStalker_grass));

  auto go_DewStalker = NtGameObject::createGameObject();
  go_DewStalker.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/dew_stalker.obj"));
  go_DewStalker.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/dew_stalker.png"));
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = go_DewStalker.texture->getImageView();
  imageInfo.sampler = go_DewStalker.texture->getSampler();
  NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &imageInfo)
      .build(go_DewStalker.materialDescriptorSet);
  go_DewStalker.transform.translation = {-2.3f, 0.0f, 0.0f};
  gameObjects.push_back(std::move(go_DewStalker));
}

}
