#include "astral_app.hpp"
#include "generic_render_system.hpp"
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
#include <iostream>

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
  // init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
  init_info.DescriptorPoolSize = 1000;
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
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500)
    .build();

  bonePool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(100)
    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 500)
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
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Base color texture
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Normal texture
    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Metallic-roughness texture
    // .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Occlusion texture
    // .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Emissive texture
    .build();

  boneSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    .build();

  std::vector<VkDescriptorSet> globalDescriptorSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT);
  for(int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();

    NtDescriptorWriter(*globalSetLayout, *globalPool)
      .writeBuffer(0, &bufferInfo)
      .build(globalDescriptorSets[i]);
  }

  GlobalUbo ubo{};

  GenericRenderSystem genericRenderSystem(ntDevice, ntRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout(),
      modelSetLayout->getDescriptorSetLayout(), boneSetLayout->getDescriptorSetLayout());
  NtCamera camera{};

  auto viewerObject = NtGameObject::createGameObject();
  viewerObject.transform.rotation = {-1.2f, 2.8f, 0.0f};
  viewerObject.transform.translation = {-12.5f, -7.8f, -10.8f};
  auto targetObject = NtGameObject::createGameObject();
  targetObject.transform.translation = {-0.1f, -0.5f, 0};
  NtInputController inputController{};

  // Debug world grid
  // auto debugGridObject = NtGameObject::createGameObject();
  // debugGridObject.model = createGOPlane(1000.0f);

  loadGameObjects();

  // Temporary implementation of DeltaTime
  auto currentTime = std::chrono::high_resolution_clock::now();

  while (!ntWindow.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

    static int camProjType = 0;
    static int camControlType = 0;
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
      static_cast<nt::CameraProjectionType>(camProjType),
      static_cast<nt::CameraControlType>(camControlType)
    );

    if (autoRotate) {
      viewerObject.transform.rotation.y += autoRotateSpeed * deltaTime;
    }

    if (!camControlType)
        camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
    else camera.setViewTarget(viewerObject.transform.translation, targetObject.transform.translation);
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


        const char* renderModeItems[] = { "Lit", "Unlit", "Normals", "TangentNormals", "Depth", "Lighting", "LitWireframe", "Wireframe" };
        static int renderModeCurrent = 0;
        ImGui::Combo("##RenderMode", &renderModeCurrent, renderModeItems, IM_ARRAYSIZE(renderModeItems));
        ImGui::SetItemTooltip("View mode");
        genericRenderSystem.switchRenderMode(static_cast<RenderMode>(renderModeCurrent));

        // ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Camera"))
        {

          if (ImGui::Button("Reset")) {
              targetObject.transform.translation = {-0.05f, -.3f, 0.0f};
          }

          ImGui::RadioButton("FPS", &camControlType, 0); ImGui::SameLine();
          ImGui::RadioButton("Orbit", &camControlType, 1);

          if (camControlType == 1) {
              ImGui::RadioButton("Perspective", &camProjType, 0); ImGui::SameLine();
              ImGui::RadioButton("Orthographic", &camProjType, 1);
              ImGui::Checkbox("Auto Rotate", &autoRotate);ImGui::SameLine();
          }

          ImGui::Text("Position: %.1f %.1f %.1f", viewerObject.transform.translation.x, viewerObject.transform.translation.y, viewerObject.transform.translation.z);
          ImGui::Text("Rotation: %.1f %.1f %.1f", viewerObject.transform.rotation.x, viewerObject.transform.rotation.y, viewerObject.transform.rotation.z);

          ImGui::TreePop();
        }

        if (ImGui::TreeNode("Gamepad")) {
            if (inputController.gamepadConnected) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (ID: %d)", inputController.connectedGamepadId);
                const char* name = glfwGetGamepadName(inputController.connectedGamepadId);
                if (name) {
                    ImGui::Text("Name: %s", name);
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No gamepad detected");
            }

            // Gamepad configuration
            if (inputController.gamepadConnected) {
                float sensitivity = inputController.getGamepadSensitivity();
                if (ImGui::SliderFloat("Look Sensitivity", &sensitivity, 0.1f, 5.0f, "%.2f")) {
                    inputController.setGamepadSensitivity(sensitivity);
                }

                float moveSpeed = inputController.getGamepadMoveSpeed();
                if (ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 100.0f, "%.1f")) {
                    inputController.setGamepadMoveSpeed(moveSpeed);
                }

                float deadzone = inputController.getGamepadDeadzone();
                if (ImGui::SliderFloat("Stick Deadzone", &deadzone, 0.0f, 0.5f, "%.3f")) {
                    inputController.setGamepadDeadzone(deadzone);
                }

                float zoomSpeed = inputController.getGamepadZoomSpeed();
                if (ImGui::SliderFloat("Zoom Speed", &zoomSpeed, 0.1f, 10.0f, "%.2f")) {
                    inputController.setGamepadZoomSpeed(zoomSpeed);
                }

                if (ImGui::Button("Reset to Defaults")) {
                    inputController.setGamepadSensitivity(2.0f);
                    inputController.setGamepadMoveSpeed(25.0f);
                    inputController.setGamepadDeadzone(0.15f);
                    inputController.setGamepadZoomSpeed(2.0f);
                }

                // Live input display for debugging
                if (ImGui::CollapsingHeader("Live Input Debug")) {
                    float leftX = inputController.getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_X);
                    float leftY = inputController.getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_Y);
                    float rightX = inputController.getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_X);
                    float rightY = inputController.getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y);

                    ImGui::Text("Left Stick: (%.3f, %.3f)", leftX, leftY);
                    ImGui::Text("Right Stick: (%.3f, %.3f)", rightX, rightY);

                    ImGui::Text("Buttons:");
                    ImGui::Text("X: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_CROSS) ? "PRESSED" : "released");
                    ImGui::Text("O: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_CIRCLE) ? "PRESSED" : "released");
                    ImGui::Text("[]: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_SQUARE) ? "PRESSED" : "released");
                    ImGui::Text("^: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_TRIANGLE) ? "PRESSED" : "released");
                    ImGui::Text("Start: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_START) ? "PRESSED" : "released");
                    ImGui::Text("L1: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER) ? "PRESSED" : "released");
                    ImGui::Text("R1: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER) ? "PRESSED" : "released");
                    ImGui::Text("L2: %.2f", inputController.getGamepadAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER));
                    ImGui::Text("R2: %.2f", inputController.getGamepadAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER));
                    ImGui::Text("L3: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_LEFT_THUMB) ? "PRESSED" : "released");
                    ImGui::Text("R3: %s", inputController.isGamepadButtonPressed(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB) ? "PRESSED" : "released");
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Lighting"))
        {
          static bool alpha_preview = true;
          static bool alpha_half_preview = false;
          static bool drag_and_drop = false;
          static bool options_menu = false;
          static bool hdr = false;
          ImGuiColorEditFlags misc_flags = (hdr ? ImGuiColorEditFlags_HDR : 0) | (drag_and_drop ? 0 : ImGuiColorEditFlags_NoDragDrop) | (alpha_half_preview ? ImGuiColorEditFlags_AlphaPreviewHalf : (alpha_preview ? ImGuiColorEditFlags_AlphaPreview : 0)) | (options_menu ? 0 : ImGuiColorEditFlags_NoOptions);
          ImGui::ColorEdit4("Ambient", (float*)&ubo.ambientLightColor, ImGuiColorEditFlags_DisplayHSV |  misc_flags);

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

          uint32_t totalMeshCount = 0;
          for (const auto& kv : gameObjects) {
            auto &gobject = kv.second;
            if (gobject.model) {
              totalMeshCount += gobject.model->getMeshCount();
            }
          }
          ImGui::Text("Mesh count: %u", totalMeshCount);

          ImGui::TreePop();
        }
        ImGui::End();

        if (ImGui::Begin("Animation Debug")) {
            for (const auto& kv : gameObjects) {
              auto &gobject = kv.second;
              if (gobject.animator && gobject.animator->isPlaying()) {
                ImGui::Text("Playing: %s", gobject.animator->getCurrentAnimationName().c_str());
                ImGui::Text("Time: %.2f / %.2f",
                            gobject.animator->getCurrentTime(),
                            gobject.animator->getDuration());

                if (ImGui::Button("Reset")) {
                    gobject.animator->play("Idle", true);
                }
              }
            }
        }
        ImGui::End();

        }

    float aspect = ntRenderer.getAspectRatio();

    // Ortho projection and Auto Rotate can only be used with Orbital camera
    if (!camControlType) {
      camProjType = 0;
      autoRotate = false;
    }

    if (!camProjType) {
      camera.setPerspectiveProjection(glm::radians(65.f), aspect, 0.1f, 1500.f);
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
      float orbitRadius = 3.0f;
      float bounceAmplitude = 1.0f;
      float speed8 = .2f;
      float speedVert = 0.75f;

      int lightIndex = 0;
      for (auto& kv : frameInfo.gameObjects) {
          auto& obj = kv.second;

          // ANIMATION
          if (obj.animator && obj.model && obj.model->hasSkeleton()) {
            obj.animator->update(deltaTime);
            obj.model->updateSkeleton();
          }

          // if (obj.pointLight == nullptr || lightIndex >= ubo.numLights) continue;

          // float angleOffset = glm::two_pi<float>() * lightIndex / ubo.numLights;
          // float t = time * speed8 + angleOffset;
          // float t2 = time * speedVert + angleOffset;

          // glm::vec3 newPos;

          // Camera light
          // if (lightIndex == 5) {
          //     newPos = viewerObject.transform.translation;
          // }
          // else if (lightIndex == 4) {
          //     // Light 0 — figure-8 motion (centered at 0, -10, 0)
          //     float x = glm::sin(t) * 39.4f;
          //     float z = glm::sin(2.0f * t) * 9.5f + 1.0f;
          //     float y = -10.0f;
          //     newPos = glm::vec3(x, y, z);
          // } else {
              // Other lights — bounce vertically
          //     float x = obj.transform.translation.x;  // keep current X
          //     float z = obj.transform.translation.z;  // keep current Z
          //     float y = -3.0f + glm::sin(t2 * 2.0f) * 2.0f;
          //     newPos = glm::vec3(x, y, z);
          //     // }

          // // Update UBO and object transform
          // ubo.pointLights[lightIndex].position = glm::vec4(newPos, 1.0f);
          // obj.transform.translation = newPos;

          // lightIndex++;
          }


      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();

      // render
      ntRenderer.beginSwapChainRenderPass(commandBuffer);
      // genericRenderSystem.renderDebugGrid(frameInfo, debugGridObject, viewerObject.transform.translation);
      genericRenderSystem.renderGameObjects(frameInfo);
      genericRenderSystem.renderLightBillboards(frameInfo);

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
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Quad vertices (using a plane in the XZ plane)
  modelData.l_meshes[0].vertices = {
    {{-size, 0.0f, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, 0.0f, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, 0.0f,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size, 0.0f,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}
  };

  modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

  return std::make_unique<NtModel>(ntDevice, modelData);
}

std::unique_ptr<NtModel> AstralApp::createGOCube(float size) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Cube vertices
  modelData.l_meshes[0].vertices = {
    // Front face
    {{-size, -size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, -size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size,  size, size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

    // Back face
    {{ size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},

    // Left face
    {{-size, -size, -size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-size, -size,  size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-size,  size,  size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {{-size,  size, -size}, {1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},

    // Right face
    {{ size, -size,  size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
    {{ size, -size, -size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
    {{ size,  size, -size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
    {{ size,  size,  size}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},

    // Top face
    {{-size,  size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size,  size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

    // Bottom face
    {{-size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, -size, -size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{ size, -size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
    {{-size, -size,  size}, {1.0f, 1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}}
  };

  modelData.l_meshes[0].indices = {
    // Front face
    0, 1, 2, 2, 3, 0,
    // Back face
    4, 5, 6, 6, 7, 4,
    // Left face
    8, 9, 10, 10, 11, 8,
    // Right face
    12, 13, 14, 14, 15, 12,
    // Top face
    16, 17, 18, 18, 19, 16,
    // Bottom face
    20, 21, 22, 22, 23, 20
  };

  return std::make_unique<NtModel>(ntDevice, modelData);
  }

std::unique_ptr<NtModel> AstralApp::createBillboardQuad(float size) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Billboard quad vertices (facing forward, centered at origin)
  modelData.l_meshes[0].vertices = {
    {{-size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-left
    {{ size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-right
    {{ size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Top-right
    {{-size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}   // Top-left
  };

  modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

  modelData.l_materials.resize(1);
  NtMaterial::MaterialData materialData;
  materialData.name = "BillboardMaterial";
  materialData.pbrMetallicRoughness.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  modelData.l_materials[0] = std::make_shared<NtMaterial>(ntDevice, materialData);

  // Update the material with descriptor set
  modelData.l_materials[0]->updateDescriptorSet(modelSetLayout->getDescriptorSetLayout(), modelPool->getDescriptorPool());

  return std::make_unique<NtModel>(ntDevice, modelData);
}

std::unique_ptr<NtModel> AstralApp::createBillboardQuadWithTexture(float size, std::shared_ptr<NtImage> texture) {
  NtModel::Builder modelData{ntDevice};
  modelData.l_meshes.resize(1);

  // Billboard quad vertices (facing forward, centered at origin)
  modelData.l_meshes[0].vertices = {
    {{-size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-left
    {{ size, -size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Bottom-right
    {{ size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},  // Top-right
    {{-size,  size, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}   // Top-left
  };

  modelData.l_meshes[0].indices = {0, 1, 2, 2, 3, 0};

  modelData.l_materials.resize(1);
  NtMaterial::MaterialData materialData;
  materialData.name = "BillboardMaterial";
  materialData.pbrMetallicRoughness.baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  materialData.pbrMetallicRoughness.baseColorTexture = texture;
  modelData.l_materials[0] = std::make_shared<NtMaterial>(ntDevice, materialData);

  // Update the material with descriptor set
  modelData.l_materials[0]->updateDescriptorSet(modelSetLayout->getDescriptorSetLayout(), modelPool->getDescriptorPool());

  return std::make_unique<NtModel>(ntDevice, modelData);
}

void AstralApp::loadGameObjects() {
  auto go_MoonlitCafe = NtGameObject::createGameObject();
  go_MoonlitCafe.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/MoonlitCafe/MoonlitCafe.gltf"), modelSetLayout->getDescriptorSetLayout(), modelPool->getDescriptorPool());
  go_MoonlitCafe.transform.rotation = {glm::radians(90.0f), 0.0f, 0.0f};
  gameObjects.emplace(go_MoonlitCafe.getId(), std::move(go_MoonlitCafe));

  // Create light sprite texture
  std::shared_ptr<NtImage> lightSpriteTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/sprites/light.png"));

  auto go_Cassandra = NtGameObject::createGameObject(true);
  go_Cassandra.model = NtModel::createModelFromFile(ntDevice, getAssetPath("assets/meshes/Cassandra/Cassandra_256.gltf"),
      modelSetLayout->getDescriptorSetLayout(),
      modelPool->getDescriptorPool(),
      boneSetLayout->getDescriptorSetLayout(),
      bonePool->getDescriptorPool());
  go_Cassandra.transform.translation = {0.0f, -1.5f, 0.0f};
  go_Cassandra.transform.rotation = {glm::radians(90.0f), glm::radians(90.0f), 0.0f};
  // go_Cassandra.transform.scale = {0.85f, 0.85f, 0.85f};
  if (go_Cassandra.model->hasSkeleton()) {
      go_Cassandra.animator = std::make_unique<NtAnimator>(*go_Cassandra.model);

      go_Cassandra.animator->play("derp", true);
  }
  gameObjects.emplace(go_Cassandra.getId(), std::move(go_Cassandra));



  // auto PointLightCam = NtGameObject::makePointLight(20.0f, 0.0f);
  // PointLightCam.transform.translation = {0.0f, 0.0f, 0.0f};
  // PointLightCam.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  // gameObjects.emplace(PointLightCam.getId(), std::move(PointLightCam));

  auto PointLight1 = NtGameObject::makePointLight(10.0f, 0.0f, glm::vec3(1.0, 0.51, 0.17));
  PointLight1.transform.translation = {3.2f, -7.7f, -4.0f};
  PointLight1.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  gameObjects.emplace(PointLight1.getId(), std::move(PointLight1));

  auto PointLight2 = NtGameObject::makePointLight(5.0f, 0.0f, glm::vec3(1.0, 0.43, 0.03));
  PointLight2.transform.translation = {13.2f, -6.2f, 9.9f};
  PointLight2.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  gameObjects.emplace(PointLight2.getId(), std::move(PointLight2));
}

}
