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
    .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
    .build();

  modelSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
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
  targetObject.transform.translation = {-0.1f, -0.5f, 0};
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
    int windowW, windowH;
    int framebufferW, framebufferH;
    glfwGetWindowSize(ntWindow.getGLFWwindow(), &windowW, &windowH);
    glfwGetFramebufferSize(ntWindow.getGLFWwindow(), &framebufferW, &framebufferH);

    ImVec2 scale = ImVec2(
        windowW > 0 ? (float)framebufferW / windowW : 1.0f,
        windowH > 0 ? (float)framebufferH / windowH : 1.0f
    );
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

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


        const char* renderModeItems[] = { "Lit", "Unlit", "Normals", "Depth", "Lighting", "LitWireframe", "Wireframe" };
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

        glm::vec4 ambientLightColor{1.f, 1.f, 1.f, 0.2f};
        alignas(16) glm::vec3 lightPosition{-1.f};
        glm::vec4 lightColor{1.f};

        if (ImGui::TreeNode("Lighting"))
        {
          static bool alpha_preview = true;
          static bool alpha_half_preview = false;
          static bool drag_and_drop = false;
          static bool options_menu = false;
          static bool hdr = false;
          ImGuiColorEditFlags misc_flags = (hdr ? ImGuiColorEditFlags_HDR : 0) | (drag_and_drop ? 0 : ImGuiColorEditFlags_NoDragDrop) | (alpha_half_preview ? ImGuiColorEditFlags_AlphaPreviewHalf : (alpha_preview ? ImGuiColorEditFlags_AlphaPreview : 0)) | (options_menu ? 0 : ImGuiColorEditFlags_NoOptions);
          ImGui::ColorEdit4("Ambient", (float*)&ubo.ambientLightColor, ImGuiColorEditFlags_DisplayHSV |  misc_flags);
          // ImGui::ColorEdit4("Point", (float*)&ubo.lightColor, ImGuiColorEditFlags_DisplayHSV |  misc_flags);
          // const ImGuiSliderFlags flags_for_sliders = imgui_window_flags & ~ImGuiSliderFlags_WrapAround;
          // ImGui::SliderFloat("X", &ubo.lightPosition.x, -5.0f, 5.0f, "%.2f", flags_for_sliders);
          // ImGui::SliderFloat("Y", &ubo.lightPosition.y, -5.0f, 5.0f, "%.2f", flags_for_sliders);
          // ImGui::SliderFloat("Z", &ubo.lightPosition.z, -5.0f, 5.0f, "%.2f", flags_for_sliders);

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
          for (const auto& kv : gameObjects) {
            auto &gobject = kv.second;
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
      camera.setPerspectiveProjection(glm::radians(45.f), aspect, 0.1f, 500.f);
    }
    else
    {
      float zoom = inputController.orthoZoomLevel; // ← from inputController
      float halfHeight = zoom;
      float halfWidth = aspect * halfHeight;
      camera.setOrthographicProjection(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 500.f);
    }
    

    if (auto commandBuffer = ntRenderer.beginFrame()) {
      // TODO: Add Reflections, Shadows, Postprocessing, etc
      int frameIndex = ntRenderer.getFrameIndex();
      FrameInfo frameInfo {
        frameIndex,
        deltaTime,
        commandBuffer,
        camera,
        globalDescriptorSets[frameIndex],
        gameObjects
      };

      //update
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();

      // Animate the lights
      genericRenderSystem.updateLights(frameInfo, ubo);

      float time = static_cast<float>(glfwGetTime());
      float orbitRadius = 1.5f;
      float bounceAmplitude = 0.3f;
      float speed = .5f;

      for (int i = 0; i < ubo.numLights; i++) {
          float angleOffset = glm::two_pi<float>() * i / ubo.numLights;
          float t = time * speed + angleOffset;

          // Circular orbit on XZ plane
          float x = glm::cos(t) * orbitRadius;
          float z = glm::sin(t) * orbitRadius;

          // Optional: Figure-8 shape (Lissajous-like curve)
          // float x = glm::sin(t) * orbitRadius;
          // float z = glm::sin(2 * t) * orbitRadius * 0.5f;

          // Vertical bounce (like dancing)
          float y = -1.2f + glm::sin(t * 2.0f) * bounceAmplitude;

          ubo.pointLights[i].position = glm::vec4(x, y, z, 1.0f);
      }

      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      ntRenderer.beginSwapChainRenderPass(commandBuffer);
      genericRenderSystem.renderDebugGrid(frameInfo, debugGridObject, viewerObject.transform.translation);
      genericRenderSystem.renderGameObjects(frameInfo);

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
    {{-size, 0.f, -size}, glm::vec3(0.2), glm::vec3(0, -1, 0), {1.0f, 0.0f}},
    {{ size, 0.f, -size}, glm::vec3(0.2), glm::vec3(0, -1, 0), {0.0f, 0.0f}},
    {{ size, 0.f,  size}, glm::vec3(0.2), glm::vec3(0, -1, 0), {0.0f, 1.0f}},
    {{-size, 0.f,  size}, glm::vec3(0.2), glm::vec3(0, -1, 0), {1.0f, 1.0f}},
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
  // auto go_VikingRoom = NtGameObject::createGameObject();
  // go_VikingRoom.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/viking_room.obj"));
  // go_VikingRoom.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/viking_room.png"));
  // // TODO: refactor
  // VkDescriptorImageInfo imageInfo{};
  // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // imageInfo.imageView = go_VikingRoom.texture->getImageView();
  // imageInfo.sampler = go_VikingRoom.texture->getSampler();
  // bool success = NtDescriptorWriter(*modelSetLayout, *modelPool)
  //     .writeImage(0, &imageInfo)
  //     .build(go_VikingRoom.materialDescriptorSet);
  // if (!success) {
  //     std::cerr << "Failed to build material descriptor set!" << std::endl;
  // }
  // go_VikingRoom.transform.rotation = {0.0f, glm::radians(180.0f), 0.0f};
  // gameObjects.emplace(go_VikingRoom.getId(), std::move(go_VikingRoom));

  // auto debugLightObject = NtGameObject::createGameObject();
  // debugLightObject.model = createGOCube(0.05f);
  // gameObjects.emplace(debugLightObject.getId(), std::move(debugLightObject));

  // auto floorObject = NtGameObject::createGameObject();
  // floorObject.model = createGOPlane(2.0f);
  // floorObject.diffuseTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/floor_diffuse.jpg"));
  // VkDescriptorImageInfo diffuseInfo{};
  // diffuseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // diffuseInfo.imageView = floorObject.diffuseTexture->getImageView();
  // diffuseInfo.sampler = floorObject.diffuseTexture->getSampler();
  // NtDescriptorWriter(*modelSetLayout, *modelPool)
  //     .writeImage(0, &diffuseInfo)
  //     .build(floorObject.materialDescriptorSet);
  // gameObjects.emplace(floorObject.getId(), std::move(floorObject));
  
  auto go_Atrium = NtGameObject::createGameObject();
  go_Atrium.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/sponza.obj"));
  go_Atrium.diffuseTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/sponza/sponza_floor_a_diff.tga"));

  VkDescriptorImageInfo diffuseInfo{};
  diffuseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  diffuseInfo.imageView = go_Atrium.diffuseTexture->getImageView();
  diffuseInfo.sampler = go_Atrium.diffuseTexture->getSampler();

  // go_Atrium.normalTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/bunny_normal.png"));

  // VkDescriptorImageInfo normalInfo{};
  // normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // normalInfo.imageView = go_Atrium.normalTexture->getImageView();
  // normalInfo.sampler = go_Atrium.normalTexture->getSampler();

  NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &diffuseInfo)
      // .writeImage(1, &normalInfo)
      .build(go_Atrium.materialDescriptorSet);

  go_Atrium.transform.scale = {0.2f, 0.2f, 0.2f};
  gameObjects.emplace(go_Atrium.getId(), std::move(go_Atrium));

  auto go_Bunny = NtGameObject::createGameObject();
  go_Bunny.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/bunny_low.obj"));
  go_Bunny.diffuseTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/bunny_albedo.jpeg"));

  // VkDescriptorImageInfo diffuseInfo{};
  diffuseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  diffuseInfo.imageView = go_Bunny.diffuseTexture->getImageView();
  diffuseInfo.sampler = go_Bunny.diffuseTexture->getSampler();

  go_Bunny.normalTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/bunny_normal.png"));

  VkDescriptorImageInfo normalInfo{};
  normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  normalInfo.imageView = go_Bunny.normalTexture->getImageView();
  normalInfo.sampler = go_Bunny.normalTexture->getSampler();

  NtDescriptorWriter(*modelSetLayout, *modelPool)
      .writeImage(0, &diffuseInfo)
      .writeImage(1, &normalInfo)
      .build(go_Bunny.materialDescriptorSet);

   // go_Bunny.transform.translation = {2.3f, 0.0f, 0.0f};
  gameObjects.emplace(go_Bunny.getId(), std::move(go_Bunny));

  // auto go_Kafka = NtGameObject::createGameObject();
  // go_Kafka.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/Kafka.obj"));
  // go_Kafka.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/viking_room.png"));
  // TODO: refactor
  // VkDescriptorImageInfo imageInfo{};
  // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // imageInfo.imageView = go_VikingRoom.texture->getImageView();
  // imageInfo.sampler = go_VikingRoom.texture->getSampler();
  // bool success = NtDescriptorWriter(*modelSetLayout, *modelPool)
  //     .writeImage(0, &imageInfo)
  //     .build(go_VikingRoom.materialDescriptorSet);
  // if (!success) {
  //     std::cerr << "Failed to build material descriptor set!" << std::endl;
  // }
  // go_Kafka.transform.translation = {0.0f, 0.0f, 5.0f};
  // go_Kafka.transform.scale = glm::vec3(.3f);
  // gameObjects.emplace(go_Kafka.getId(), std::move(go_Kafka));

  // auto go_DewStalker_ground = NtGameObject::createGameObject();
  // go_DewStalker_ground.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/dew_stalker_ground.obj"));
  // go_DewStalker_ground.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/dew_stalker_ground.png"));
  // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // imageInfo.imageView = go_DewStalker_ground.texture->getImageView();
  // imageInfo.sampler = go_DewStalker_ground.texture->getSampler();
  // NtDescriptorWriter(*modelSetLayout, *modelPool)
  //     .writeImage(0, &imageInfo)
  //     .build(go_DewStalker_ground.materialDescriptorSet);
  // go_DewStalker_ground.transform.translation = {-2.3f, 0.0f, 0.0f};
  // gameObjects.emplace(go_DewStalker_ground.getId(), std::move(go_DewStalker_ground));
  //
  // auto go_DewStalker_grass = NtGameObject::createGameObject();
  // go_DewStalker_grass.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/dew_stalker_grass.obj"));
  // go_DewStalker_grass.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/dew_stalker_grass.png"));
  // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // imageInfo.imageView = go_DewStalker_grass.texture->getImageView();
  // imageInfo.sampler = go_DewStalker_grass.texture->getSampler();
  // NtDescriptorWriter(*modelSetLayout, *modelPool)
  //     .writeImage(0, &imageInfo)
  //     .build(go_DewStalker_grass.materialDescriptorSet);
  // go_DewStalker_grass.transform.translation = {-2.3f, 0.0f, 0.0f};
  // gameObjects.emplace(go_DewStalker_grass.getId(), std::move(go_DewStalker_grass));
  //
  // auto go_DewStalker = NtGameObject::createGameObject();
  // go_DewStalker.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/dew_stalker.obj"));
  // go_DewStalker.texture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/textures/dew_stalker.png"));
  // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  // imageInfo.imageView = go_DewStalker.texture->getImageView();
  // imageInfo.sampler = go_DewStalker.texture->getSampler();
  // NtDescriptorWriter(*modelSetLayout, *modelPool)
  //     .writeImage(0, &imageInfo)
  //     .build(go_DewStalker.materialDescriptorSet);
  // go_DewStalker.transform.translation = {-2.3f, 0.0f, 0.0f};
  // gameObjects.emplace(go_DewStalker.getId(), std::move(go_DewStalker));
  
  auto PointLight1 = NtGameObject::makePointLight(2.5f, 0.5f);
  gameObjects.emplace(PointLight1.getId(), std::move(PointLight1));
  auto PointLight2 = NtGameObject::makePointLight(1.5f, 0.1f, glm::vec3(0.f, 1.f, 1.f));
  gameObjects.emplace(PointLight2.getId(), std::move(PointLight2));
  auto PointLight3 = NtGameObject::makePointLight(.5f, 0.1f, glm::vec3(1.f, 0.f, 0.f));
  gameObjects.emplace(PointLight3.getId(), std::move(PointLight3));

}

}
