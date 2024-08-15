#pragma once

#include "nt_window.h"

namespace nt
{
	class AstralApp
	{
	public:
		static constexpr int WIDTH = 1024;
		static constexpr int HEIGHT = 768;

		void run();

	private:
		NtWindow ntWindow{ WIDTH, HEIGHT, "You are wandering through the Astral Realm.." };
	};
}
