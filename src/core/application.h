#pragma once
#include "timestep.h"
#include "log.h"
namespace FlatEngine {
	class App {
	public:
		static void Init() {
		}
		static void Update() {
			Timestep::UpdateTime();
		}
	};
}