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
#include <glm/gtc/type_ptr.hpp>
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
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

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
  init_info.DescriptorPoolSize = 1000;

  // Dynamic Rendering
  init_info.UseDynamicRendering = true;
  init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init_info.PipelineRenderingCreateInfo.pNext = nullptr;
  VkFormat colorFormat = ntRenderer.getSwapChain()->getSwapChainImageFormat();
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
  VkFormat depthFormat = ntRenderer.getSwapChain()->getSwapChainDepthFormat();
  init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

  init_info.MinImageCount = 2;
  init_info.ImageCount = ntRenderer.getSwapChainImageCount();
  init_info.MSAASamples = ntDevice.getMsaaSamples();
  init_info.Allocator = nullptr;
  ImGui_ImplVulkan_Init(&init_info);

  globalPool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT * 2)
    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, NtSwapChain::MAX_FRAMES_IN_FLIGHT * 4)
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

  // ENGINE INIT
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
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow map
    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow map (debug)
    .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow cube map
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
  // Shadow map descriptor image info
  VkDescriptorImageInfo shadowMapImageInfo{};
  shadowMapImageInfo.sampler = shadowMap.getShadowSampler();
  shadowMapImageInfo.imageView = shadowMap.getShadowImageView();
  shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo shadowMapDebugImageInfo{};
  shadowMapDebugImageInfo.sampler = shadowMap.getShadowDebugSampler();
  shadowMapDebugImageInfo.imageView = shadowMap.getShadowImageView();
  shadowMapDebugImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  // Shadow cube map descriptor
   VkDescriptorImageInfo shadowCubeMapImageInfo{};
   shadowCubeMapImageInfo.sampler = shadowCubeMap.getShadowCubeSampler();
   shadowCubeMapImageInfo.imageView = shadowCubeMap.getShadowCubeImageView();
   shadowCubeMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

   std::cout << "Shadow cube map info:" << std::endl;
     std::cout << "  Sampler: " << shadowCubeMapImageInfo.sampler << std::endl;
     std::cout << "  ImageView: " << shadowCubeMapImageInfo.imageView << std::endl;
     std::cout << "  Size: " << shadowCubeMap.getSize() << std::endl;

  for(int i = 0; i < globalDescriptorSets.size(); i++) {
    auto bufferInfo = uboBuffers[i]->descriptorInfo();

    NtDescriptorWriter(*globalSetLayout, *globalPool)
      .writeBuffer(0, &bufferInfo)
      .writeImage(1, &shadowMapImageInfo)
      .writeImage(2, &shadowMapDebugImageInfo)
      .writeImage(3, &shadowCubeMapImageInfo)
      .build(globalDescriptorSets[i]);
  }

  // ImGUI -> Debug ShadowMap
  imguiShadowMapTexture = ImGui_ImplVulkan_AddTexture(
      shadowMap.getShadowDebugSampler(),
      shadowMap.getShadowImageView(),
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  static glm::vec3 OrthoDir = glm::vec3(-0.55, -0.8f, 0.55f);
  static float OrthoScale = 31.0f;
  static float OrthoNear = -30.0f;
  static float OrthoFar = 44.0f;

  // Register each cubemap face with ImGui for debugging
    for (uint32_t face = 0; face < 6; ++face) {
        cubemapFaceDescriptorSets[face] = ImGui_ImplVulkan_AddTexture(
            shadowCubeMap.getShadowCubeDebugSampler(),
            shadowCubeMap.getFaceImageView(face), // Individual face view
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }

  // Create descriptor set layout for debug quad (just needs the shadow map texture)
  debugQuadSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    .build();

  // Create debug quad descriptor sets
  debugQuadDescriptorSets.resize(NtSwapChain::MAX_FRAMES_IN_FLIGHT);
  for(int i = 0; i < debugQuadDescriptorSets.size(); i++) {
    NtDescriptorWriter(*debugQuadSetLayout, *globalPool)
      .writeImage(0, &shadowMapDebugImageInfo)
      .build(debugQuadDescriptorSets[i]);
  }

  // Create debug quad system
  debugQuadSystem = std::make_unique<DebugQuadSystem>(
    ntDevice,
    debugQuadSetLayout->getDescriptorSetLayout());

  GlobalUbo ubo{};

  GenericRenderSystem genericRenderSystem(ntDevice, *ntRenderer.getSwapChain(), globalSetLayout->getDescriptorSetLayout(),
      modelSetLayout->getDescriptorSetLayout(), boneSetLayout->getDescriptorSetLayout());

  // CAMERA SETUP
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

  auto currentTime = std::chrono::high_resolution_clock::now();

  // ENGINE LOOP
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
        ImGui::PlotLines("##", frameTimes, IM_ARRAYSIZE(frameTimes), frameTimeOffset,
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

        // if (ImGui::Begin("Animation Debug")) {
        //     for (const auto& kv : gameObjects) {
        //       auto &gobject = kv.second;
        //       if (gobject.animator && gobject.animator->isPlaying()) {
        //         ImGui::Text("Playing: %s", gobject.animator->getCurrentAnimationName().c_str());
        //         ImGui::Text("Time: %.2f / %.2f",
        //                     gobject.animator->getCurrentTime(),
        //                     gobject.animator->getDuration());

        //         if (ImGui::Button("Reset")) {
        //             gobject.animator->play("Idle", true);
        //         }
        //       }
        //     }
        // }
        // ImGui::End();

        if (ImGui::Begin("ShadowMap")) {
            ImGui::DragFloat3("Light Dir", glm::value_ptr(OrthoDir), 0.01f, -5.0f, 5.0f);
            ImGui::SliderFloat("Ortho Scale", &OrthoScale, 1.0f, 200.0f);
            ImGui::SliderFloat("Ortho Near", &OrthoNear, -100.0f, 100.0f);
            ImGui::SliderFloat("Ortho Far", &OrthoFar, 1.0f, 200.0f);
            uint16_t shad_deb_mult = 25;
            ImGui::Image((ImTextureID)(uint64_t)imguiShadowMapTexture, ImVec2(16 * shad_deb_mult, 9 * shad_deb_mult));

            // Show cubemap faces in a grid
            // ImGui::Text("Point Light Cubemap Faces");
            // const char* faceNames[6] = {"+X (Right)", "-X (Left)", "+Y (Up)", "-Y (Down)", "+Z (Forward)", "-Z (Back)"};

            // // Display in 2x3 grid
            // for (uint32_t face = 0; face < 6; ++face) {
            //     ImTextureID cubeTexId = (ImTextureID)(uintptr_t)cubemapFaceDescriptorSets[face];

            //     // Start new row after 2 images
            //     if (face > 0 && face % 2 == 0) {
            //         // New row
            //     }

            //     ImGui::BeginGroup();
            //     ImGui::Text("%s", faceNames[face]);
            //     ImGui::Image(cubeTexId, ImVec2(200, 200));
            //     ImGui::EndGroup();

            //     // Same line for pairs
            //     if (face % 2 == 0 && face < 5) {
            //         ImGui::SameLine();
            //     }
            // }
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
      camera.setPerspectiveProjection(glm::radians(65.f), aspect, 0.1f, 1000.f);
    }
    else
    {
      float zoom = inputController.orthoZoomLevel;
      float halfHeight = zoom;
      float halfWidth = aspect * halfHeight;
      camera.setOrthographicProjection(-halfWidth, halfWidth, -halfHeight, halfHeight, -10.0f, 500.f);
    }

    // EVERY FRAME
    if (auto commandBuffer = ntRenderer.beginFrame()) {
      int frameIndex = ntRenderer.getFrameIndex();
      FrameInfo frameInfo {
        frameIndex,
        deltaTime,
        commandBuffer,
        camera,
        globalDescriptorSets[frameIndex],
        gameObjects
      };

      // UBO
      // ---
      // Update the camera
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      // Lighting
      genericRenderSystem.updateLights(frameInfo, ubo, OrthoDir, OrthoScale, OrthoNear, OrthoFar);

      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();
      // ---

      // ANIMATION
      for (auto& kv : frameInfo.gameObjects) {
          auto& obj = kv.second;
          if (obj.animator && obj.model && obj.model->hasSkeleton()) {
            obj.animator->update(deltaTime);
            obj.model->updateSkeleton();
          }
      }

      // RENDERING
      // ---
      // PASS 1: Render shadow map
      // TODO: Check if shadow caster is point light
        bool renderCubeShadows = false; // Set this based on shadow caster type

        genericRenderSystem.switchRenderMode(RenderMode::ShadowMap);
        if (renderCubeShadows) {
            // PASS 1: Render shadow cube map (6 faces)
            std::cout << "Rendering cubemap shadows..." << std::endl;

                        // Transition ALL cube faces to depth attachment ONCE before rendering
                        VkImageMemoryBarrier initialBarrier{};
                        initialBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        initialBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                        initialBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                        initialBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        initialBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        initialBarrier.image = shadowCubeMap.getShadowCubeImage();
                        initialBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                        initialBarrier.subresourceRange.baseMipLevel = 0;
                        initialBarrier.subresourceRange.levelCount = 1;
                        initialBarrier.subresourceRange.baseArrayLayer = 0;
                        initialBarrier.subresourceRange.layerCount = 6;  // ALL 6 faces
                        initialBarrier.srcAccessMask = 0;
                        initialBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                        vkCmdPipelineBarrier(
                            commandBuffer,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                            0, 0, nullptr, 0, nullptr, 1, &initialBarrier
                        );

                        // Now render each face
                        for (uint32_t face = 0; face < 6; ++face) {
                            std::cout << "  Rendering cube face " << face << std::endl;

                            // Don't do layout transition here, just begin rendering
                            VkRenderingAttachmentInfo depthAttachment{};
                            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                            depthAttachment.imageView = shadowCubeMap.getFaceImageView(face);
                            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                            depthAttachment.clearValue.depthStencil = {1.0f, 0};

                            VkRenderingInfo renderingInfo{};
                            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                            renderingInfo.renderArea = {{0, 0}, {shadowCubeMap.getSize(), shadowCubeMap.getSize()}};
                            renderingInfo.layerCount = 1;
                            renderingInfo.colorAttachmentCount = 0;
                            renderingInfo.pDepthAttachment = &depthAttachment;

                            ntDevice.vkCmdBeginRendering(commandBuffer, &renderingInfo);

                            VkViewport viewport{};
                            viewport.x = 0.0f;
                            viewport.y = 0.0f;
                            viewport.width = static_cast<float>(shadowCubeMap.getSize());
                            viewport.height = static_cast<float>(shadowCubeMap.getSize());
                            viewport.minDepth = 0.0f;
                            viewport.maxDepth = 1.0f;
                            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

                            VkRect2D scissor{{0, 0}, {shadowCubeMap.getSize(), shadowCubeMap.getSize()}};
                            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                            genericRenderSystem.setCubeFaceIndex(face);
                            vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
                            genericRenderSystem.renderGameObjects(frameInfo);

                            ntDevice.vkCmdEndRendering(commandBuffer);
                        }

                        // Transition ALL faces to shader read
                        ntRenderer.endShadowCubeRendering(commandBuffer, &shadowCubeMap);

             // std::cout << "Cubemap rendering complete" << std::endl;
            genericRenderSystem.setCubeFaceIndex(-1); // Reset to regular rendering
        } else {
            // Regular 2D shadow map rendering
            ntRenderer.beginShadowRendering(commandBuffer, &shadowMap);
            genericRenderSystem.switchRenderMode(RenderMode::ShadowMap);
            vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
            genericRenderSystem.renderGameObjects(frameInfo);
            ntRenderer.endShadowRendering(commandBuffer, &shadowMap);
        }

      // PASS 2: Sample from it and render main scene
      ntRenderer.beginMainRendering(commandBuffer);

        genericRenderSystem.switchRenderMode(RenderMode::Lit);
        // genericRenderSystem.renderDebugGrid(frameInfo, debugGridObject, viewerObject.transform.translation);
        genericRenderSystem.renderGameObjects(frameInfo);
        genericRenderSystem.renderLightBillboards(frameInfo);

        // debugQuadSystem->render(commandBuffer, debugQuadDescriptorSets[frameIndex]);

        if (ntWindow.getShowImGUI()) {
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ntRenderer.getCurrentCommandBuffer());
        }

      ntRenderer.endMainRendering(commandBuffer);
      // ---


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

      go_Cassandra.animator->play("Idle", true);
  }
  gameObjects.emplace(go_Cassandra.getId(), std::move(go_Cassandra));

  // Create light sprite texture
  std::shared_ptr<NtImage> lightSpriteTexture = NtImage::createTextureFromFile(ntDevice, getAssetPath("assets/sprites/light.png"));

  // auto PointLightCam = NtGameObject::makePointLight(20.0f, 0.0f);
  // PointLightCam.transform.translation = {0.0f, 0.0f, 0.0f};
  // PointLightCam.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  // gameObjects.emplace(PointLightCam.getId(), std::move(PointLightCam));



  auto PointLight1 = NtGameObject::makePointLight(100.0f, 0.0f, glm::vec3(1.0, 0.65, 0.33));
  PointLight1.transform.translation = {3.5f, -7.5f, -7.2f};
  PointLight1.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  gameObjects.emplace(PointLight1.getId(), std::move(PointLight1));

  auto PointLight2 = NtGameObject::makePointLight(75.0f, 0.0f, glm::vec3(1.0, 0.3, 0.03));
  PointLight2.transform.translation = {13.0f, -4.2f, 9.9f};
  PointLight2.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  gameObjects.emplace(PointLight2.getId(), std::move(PointLight2));

  auto directionalLight = NtGameObject::createGameObject();
  directionalLight.transform.translation = glm::vec3(1.0f, 1.0f, 0.5f); // Direction (will be normalized)
  directionalLight.color = glm::vec3(0.2f, 0.7f, 0.9f);
  directionalLight.pointLight = std::make_unique<PointLightComponent>();
  directionalLight.pointLight->lightIntensity = 1.0f;
  directionalLight.pointLight->lightType = 2;
  gameObjects.emplace(directionalLight.getId(), std::move(directionalLight));

  // Create a spotlight
  // auto spotlight = NtGameObject::createGameObject();
  // spotlight.transform.translation = glm::vec3(-6.4f, -7.3f, 3.2f); // Above the scene
  // spotlight.color = glm::vec3(0.0, 0.85, 0.55);
  // spotlight.model = createBillboardQuadWithTexture(1.0f, lightSpriteTexture);
  // spotlight.pointLight = std::make_unique<PointLightComponent>();
  // spotlight.pointLight->lightIntensity = 200.0f; // Spotlights need higher intensity
  // spotlight.pointLight->lightType = 1.0;
  // spotlight.pointLight->spotDirection = glm::vec3(4.0f, 1.0f, -0.55f); // Point down (local space)
  // spotlight.pointLight->spotInnerConeAngle = 5.0f; // Inner cone
  // spotlight.pointLight->spotOuterConeAngle = 45.0f; // Outer cone (with falloff)
  // gameObjects.emplace(spotlight.getId(), std::move(spotlight));

}

}
