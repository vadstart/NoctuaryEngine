#pragma once

#include "nt_ecs.hpp"
#include "nt_material.hpp"
#include "nt_shadows.hpp"
#include "nt_window.hpp"
#include "nt_device.hpp"
#include "nt_renderer.hpp"
#include "nt_descriptors.hpp"
#include "nt_debugline_system.hpp"

#include <filesystem>
#include <memory>

using std::vector;

namespace nt
{
    class DebugSystem : public NtSystem
    {
        public:
        DebugSystem(NtNexus* nexus_ptr) : nexus(nexus_ptr) {};

        private:
            NtNexus* nexus;
    };

	class AstralApp
	{
	public:
		static constexpr int WIDTH = 1920;
		static constexpr int HEIGHT = 1080;

    AstralApp();
    ~AstralApp();

    AstralApp(const AstralApp&) = delete;
		AstralApp& operator=(const AstralApp&) = delete;

		void run();

	private:
    // Helper functions
    std::unique_ptr<NtModel> createModelFromFile(const std::string &filepath, MaterialType type = MaterialType::PBR) {
        return NtModel::createModelFromFile(
            ntDevice,
            filepath,
            type,
            modelSetLayout->getDescriptorSetLayout(),
            modelPool->getDescriptorPool(),
            boneSetLayout->getDescriptorSetLayout(),
            bonePool->getDescriptorPool());
    };
    std::unique_ptr<NtModel> createPlane(float size, const std::string &filepath, MaterialType type = MaterialType::PBR) {
        return NtModel::createPlane(
            ntDevice,
            size,
            filepath,
            type,
            modelSetLayout->getDescriptorSetLayout(),
            modelPool->getDescriptorPool());
    };

    NtWindow ntWindow{ WIDTH, HEIGHT, "🌋 You are wandering through the Astral Realm.." };
    NtDevice ntDevice{ntWindow};
    NtRenderer ntRenderer{ntWindow, ntDevice};

    // Descriptors
    std::unique_ptr<NtDescriptorPool> globalPool{};
    std::unique_ptr<NtDescriptorSetLayout> globalSetLayout;
    std::unique_ptr<NtDescriptorPool> modelPool{};
    std::unique_ptr<NtDescriptorSetLayout> modelSetLayout;
    std::unique_ptr<NtDescriptorPool> bonePool{};
    std::unique_ptr<NtDescriptorSetLayout> boneSetLayout;

    std::shared_ptr<NtMaterialLibrary> materialLibrary;

    NtShadowMap shadowMap{ntDevice, 1024, 1024};
    VkDescriptorSet imguiShadowMapTexture = VK_NULL_HANDLE;
    std::unique_ptr<NtLineRenderSystem> debugLineSystem;

    NtNexus Nexus;
	};
}
