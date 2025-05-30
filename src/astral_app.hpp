#pragma once

#include "nt_game_object.hpp"
#include "nt_window.hpp"
#include "nt_device.hpp"
#include "nt_renderer.hpp"

#include <filesystem>

using std::vector;

namespace nt
{
	class AstralApp
	{
	public:
		static constexpr int WIDTH = 1280;
		static constexpr int HEIGHT = 1024;

    AstralApp();
    ~AstralApp();

    AstralApp(const AstralApp&) = delete;
		AstralApp& operator=(const AstralApp&) = delete;

    
		void run();

	private:
    void loadGameObjects();

    NtWindow ntWindow{ WIDTH, HEIGHT, "🌋 You are wandering through the Astral Realm.." };
    NtDevice ntDevice{ntWindow};
    NtRenderer ntRenderer{ntWindow, ntDevice};

    std::vector<NtGameObject> gameObjects;
	};
}
