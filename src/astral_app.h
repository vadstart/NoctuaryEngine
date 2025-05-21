#pragma once

#include "nt_game_object.h"
#include "nt_window.h"
#include "nt_device.h"
#include "nt_renderer.h"

#include <memory>

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
    void loadGameObjects();

    NtWindow ntWindow{ WIDTH, HEIGHT, "🌋 You are wandering through the Astral Realm.." };
    NtDevice ntDevice{ntWindow};
    NtRenderer ntRenderer{ntWindow, ntDevice};

    std::vector<NtGameObject> gameObjects;
	};
}
