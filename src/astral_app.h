#pragma once

#include "nt_pipeline.h"
#include "nt_window.h"
#include "nt_device.h"
#include "nt_swap_chain.h"

#include <memory>
#include <vector>

using std::vector;

namespace nt
{
	class AstralApp
	{
	public:
		static constexpr int WIDTH = 1024;
		static constexpr int HEIGHT = 768;

    AstralApp();
    ~AstralApp();

    AstralApp(const AstralApp&) = delete;
		AstralApp& operator=(const AstralApp&) = delete;

    
		void run();

	private:
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void drawFrame();

		NtWindow ntWindow{ WIDTH, HEIGHT, "You are wandering through the Astral Realm.." };
    NtDevice ntDevice{ntWindow};
    NtSwapChain ntSwapChain{ntDevice, ntWindow.getExtent()};
    std::unique_ptr<NtPipeline> ntPipeline;
    VkPipelineLayout pipelineLayout;
    vector<VkCommandBuffer> commandBuffers;

	};
}
