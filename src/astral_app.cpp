#include "astral_app.hpp"
#include "nt_camera_system.hpp"
#include "nt_buffer.hpp"
#include "nt_descriptors.hpp"
#include "nt_frame_info.hpp"
#include "nt_image.hpp"
#include "nt_input.hpp"
#include "nt_light_system.hpp"
#include "nt_render_system.hpp"
#include "nt_types.hpp"
#include "nt_utils.hpp"
#include "nt_components.hpp"

#include <chrono>
#include <cstdint>
#include <glm/fwd.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <ostream>

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
  // SETUP ImGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

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

  // Allocate memory for descriptor pools
  globalPool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NtSwapChain::MAX_FRAMES_IN_FLIGHT)
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, NtSwapChain::MAX_FRAMES_IN_FLIGHT * 2)
    .build();

  globalSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow map
    .build();


  modelPool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(100)
    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500)
    .build();

  modelSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Base color texture
    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Normal texture
    .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Metallic-roughness texture
    .build();


  boneSetLayout = NtDescriptorSetLayout::Builder(ntDevice)
    .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    .build();

  bonePool = NtDescriptorPool::Builder(ntDevice)
    .setMaxSets(100)
    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 500)
    .build();
}
AstralApp::~AstralApp() {

}

void AstralApp::run()
{
//  Buffers and descriptorSets


    GlobalUbo ubo{};
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
    std::vector<VkDescriptorSet> globalDescriptorSets(NtSwapChain::MAX_FRAMES_IN_FLIGHT);
    // Shadow map descriptor image info
    VkDescriptorImageInfo shadowMapImageInfo{};
    shadowMapImageInfo.sampler = shadowMap.getShadowSampler();
    shadowMapImageInfo.imageView = shadowMap.getShadowImageView();
    shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    for(int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();

      NtDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &shadowMapImageInfo)
        .build(globalDescriptorSets[i]);
    }

// ⌛
    auto currentTime = std::chrono::high_resolution_clock::now();

std::cout << "Aspect ratio: " << ntRenderer.getAspectRatio() << std::endl;

// ECS
    Astral.Init();

    // Component Types setup
    Astral.RegisterComponent<cName>();
    Astral.RegisterComponent<cTransform>();
    Astral.RegisterComponent<cLight>();
    Astral.RegisterComponent<cModel>();
    Astral.RegisterComponent<cPlayerController>();

    // System setup
    // GenericRenderSystem genericRenderSystem(ntDevice, *ntRenderer.getSwapChain(), globalSetLayout->getDescriptorSetLayout(),
    //     modelSetLayout->getDescriptorSetLayout(), boneSetLayout->getDescriptorSetLayout());
    auto debugSystem = Astral.RegisterSystem<DebugSystem>();
    NtSignature debugSignature;
    debugSignature.set(Astral.GetComponentType<cName>());
    Astral.SetSystemSignature<DebugSystem>(debugSignature);

    auto renderSystem = Astral.RegisterSystem<RenderSystem>(ntDevice, *ntRenderer.getSwapChain(), globalSetLayout->getDescriptorSetLayout(),
            modelSetLayout->getDescriptorSetLayout(), boneSetLayout->getDescriptorSetLayout());
    NtSignature renderSignature;
    renderSignature.set(Astral.GetComponentType<cModel>());
    Astral.SetSystemSignature<RenderSystem>(renderSignature);

    auto lightSystem = Astral.RegisterSystem<LightSystem>();
    NtSignature lightSignature;
    lightSignature.set(Astral.GetComponentType<cLight>());
    Astral.SetSystemSignature<LightSystem>(lightSignature);

    auto cameraSystem = Astral.RegisterSystem<CameraSystem>();
    NtSignature cameraSignature;
    cameraSignature.set(Astral.GetComponentType<cCamera>());
    Astral.SetSystemSignature<CameraSystem>(cameraSignature);

    // Spawning entities
    auto Camera = Astral.CreateEntity();
    Camera.AddComponent(cName{"Camera"})
        .AddComponent(cTransform{ glm::vec3(0.0f), glm::vec3(-12.5f, -7.8f, -10.8f) })
        .AddComponent(cCamera{ glm::radians(65.f), ntRenderer.getAspectRatio(), 0.1f, 1000.f });

    auto MoonlitCafe = Astral.CreateEntity();
    MoonlitCafe.AddComponent(cName{"MoonlitCafe"})
        .AddComponent(cTransform{ glm::vec3(0.0f),
            glm::vec3(glm::radians(90.0f), 0.0f, 0.0f) })
        .AddComponent(cModel{ createModelFromFile(getAssetPath("assets/meshes/MoonlitCafe/MoonlitCafe.gltf")) });

    auto Cassandra = Astral.CreateEntity();
    Cassandra.AddComponent(cName{"Cassandra"})
        .AddComponent(cTransform{ glm::vec3(0.0f, -1.5f, 0.0f),
            glm::vec3(glm::radians(90.0f), glm::radians(90.0f), 0.0f) })
        .AddComponent(cModel{ createModelFromFile(getAssetPath("assets/meshes/Cassandra/Cassandra_256.gltf")), true })
        .AddComponent(cPlayerController{5.0f, 10.0f});

    auto SunShadowCaster = Astral.CreateEntity();
    SunShadowCaster.AddComponent(cName{"Light.Sun"})
        .AddComponent(cTransform{ glm::vec3(0.0f), glm::vec3(1.0f, 1.0f, 0.5f) })
        .AddComponent(cLight{1.0f, glm::vec3(0.5f, 0.35f, 0.33f), eLightType::Directional });

    auto BarLight = Astral.CreateEntity();
    BarLight.AddComponent(cName{"Light.Bar"})
        .AddComponent(cTransform{ glm::vec3(3.5f, -7.5f, -7.2f) })
        .AddComponent(cLight{100.0f, glm::vec3(1.0f, 0.65f, 0.33f) });

//  Debug stuff
    imguiShadowMapTexture = ImGui_ImplVulkan_AddTexture(
        shadowMap.getShadowDebugSampler(),
        shadowMap.getShadowImageView(),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    static glm::vec3 OrthoDir = glm::vec3(0.68, -0.8f, -0.46f);
    static float OrthoScale = 31.0f;
    static float OrthoNear = -30.0f;
    static float OrthoFar = 44.0f;
    static int selectedEntityID = -1;

  // ENGINE LOOP
  while (!ntWindow.shouldClose()) {
    glfwPollEvents();

// Time
    auto newTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
    currentTime = newTime;

// ImGUI
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

        // const char* renderModeItems[] = { "Lit", "Unlit", "Normals", "TangentNormals", "Depth", "Lighting", "LitWireframe", "Wireframe" };
        // static int renderModeCurrent = 0;
        // ImGui::Combo("##RenderMode", &renderModeCurrent, renderModeItems, IM_ARRAYSIZE(renderModeItems));
        // ImGui::SetItemTooltip("View mode");
        // genericRenderSystem.switchRenderMode(static_cast<RenderMode>(renderModeCurrent));

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

          ImGui::TreePop();
        }
        ImGui::End();

        if (ImGui::Begin("ShadowMap")) {
            ImGui::DragFloat3("Light Dir", glm::value_ptr(OrthoDir), 0.01f, -5.0f, 5.0f);
            ImGui::SliderFloat("Ortho Scale", &OrthoScale, 1.0f, 200.0f);
            ImGui::SliderFloat("Ortho Near", &OrthoNear, -100.0f, 100.0f);
            ImGui::SliderFloat("Ortho Far", &OrthoFar, 1.0f, 200.0f);
            uint16_t shad_deb_mult = 25;
            ImGui::Image((ImTextureID)(uint64_t)imguiShadowMapTexture, ImVec2(16 * shad_deb_mult, 9 * shad_deb_mult));
        }
        ImGui::End();

        if (ImGui::Begin("Entities")) {
            static ImGuiTextFilter filter;
            filter.Draw("##");
            for (auto const& entity : debugSystem->entities )
            {
                std::string displayName = Astral.GetComponent<cName>(entity).name + " (id=" + std::to_string(entity) + ")";
                if (filter.PassFilter(displayName.c_str())) {
                    if (ImGui::Selectable(displayName.c_str(), selectedEntityID == entity))
                        selectedEntityID = entity;
                }
            }
        }
        ImGui::End();

        if (ImGui::Begin("Selected Entity")) {
            if (selectedEntityID >= 0) {
                ImGui::Text("Entity ID: %d", selectedEntityID);
                ImGui::Separator();

                static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

                // Check and display each component type
                if (Astral.HasComponent<cName>(selectedEntityID)) {
                    auto& name = Astral.GetComponent<cName>(selectedEntityID);
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Name");
                    if (ImGui::BeginTable("NameComponent", 2, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Name");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(name.name.c_str());
                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                }

                if (Astral.HasComponent<cTransform>(selectedEntityID)) {
                    auto& transform = Astral.GetComponent<cTransform>(selectedEntityID);
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Transform");
                    if (ImGui::BeginTable("TransformComponent", 2, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Position");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.1f, %.1f, %.1f", transform.translation.x, transform.translation.y, transform.translation.z);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Rotation");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.1f, %.1f, %.1f", transform.rotation.x, transform.rotation.y, transform.rotation.z);

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Scale");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.1f, %.1f, %.1f", transform.scale.x, transform.scale.y, transform.scale.z);

                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                }

                if (Astral.HasComponent<cModel>(selectedEntityID)) {
                    auto& model = Astral.GetComponent<cModel>(selectedEntityID);
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Model");
                    if (ImGui::BeginTable("ModelComponent", 2, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Drop Shadow:");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", model.bDropShadow);
                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                }

                if (Astral.HasComponent<cLight>(selectedEntityID)) {
                    auto& light = Astral.GetComponent<cLight>(selectedEntityID);
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Light");
                    if (ImGui::BeginTable("LightComponent", 2, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Type");
                        ImGui::TableSetColumnIndex(1);
                        static std::string lightType;
                        switch (light.type)
                        {
                            case eLightType::Point:        lightType="Point"; break;
                            case eLightType::Spot:         lightType="Spot"; break;
                            case eLightType::Directional:  lightType="Directional"; break;
                            default:                       lightType="Unknown"; break;
                        }
                        ImGui::Text("%s", lightType.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Intensity");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%f", light.intensity);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Color");
                        ImGui::TableSetColumnIndex(1);
                        static ImVec4 light_color = { light.color.x, light.color.y, light.color.z, 1.0f };
                        ImGui::ColorButton("MyColor", light_color, ImGuiColorEditFlags_NoTooltip);
                        ImGui::SameLine();
                        ImGui::Text("%.2f, %.2f, %.2f", light.color.x, light.color.y, light.color.z);
                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                }
            }
        }
        ImGui::End();

        }
    // ---

// Input update
    inputController.update(
      &ntWindow,
      viewerObject,
      targetObject,
      deltaTime,
      io.MouseWheel,
      static_cast<nt::CameraControlType>(camControlType)
    );

// Camera update
    cameraSystem.update();

// EVERY FRAME
    if (auto commandBuffer = ntRenderer.beginFrame()) {
      int frameIndex = ntRenderer.getFrameIndex();
      FrameInfo frameInfo {
        frameIndex,
        deltaTime,
        commandBuffer,
        globalDescriptorSets[frameIndex]
      };

    // UBO
      // Update the camera
      ubo.projection = camera.getProjection();
      ubo.view = camera.getView();
      ubo.inverseView = camera.getInverseView();
      // Lighting
      lightSystem->updateLights(Astral, frameInfo, ubo, OrthoDir, OrthoScale, OrthoNear, OrthoFar);

      uboBuffers[frameIndex]->writeToBuffer(&ubo);
      uboBuffers[frameIndex]->flush();
      // ---

    // ANIMATION
      // for (auto& kv : frameInfo.gameObjects) {
      //     auto& obj = kv.second;
      //     if (obj.animator && obj.model && obj.model->hasSkeleton()) {
      //       obj.animator->update(deltaTime);
      //       obj.model->updateSkeleton();
      //     }
      // }
      // ---

    // RENDERING
      // PASS 1: Render shadow map
      ntRenderer.beginShadowRendering(commandBuffer, &shadowMap);

        renderSystem->switchRenderMode(RenderMode::ShadowMap);
        vkCmdSetDepthBias(commandBuffer, 1.25f, 0.0f, 1.75f);
        renderSystem->renderGameObjects(Astral, frameInfo);

      ntRenderer.endShadowRendering(commandBuffer, &shadowMap);

      // PASS 2: Sample from it and render main scene
      ntRenderer.beginMainRendering(commandBuffer);

        renderSystem->switchRenderMode(RenderMode::Lit);
        renderSystem->renderGameObjects(Astral, frameInfo);
        // genericRenderSystem.renderLightBillboards(frameInfo);

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

}
