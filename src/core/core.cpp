#include "core.h"
#include <core/input.h>

#include "window.h"

namespace FlatEngine {
	EngineState engine_state;

	void SetEngineState(EngineState state) {
		if(state == PLAYING) {
			Input::SetCursorPosition(Window::GetWindowSize().x/2,Window::GetWindowSize().y/2);
		}
		engine_state = state;
	}
	EngineState GetEngineState() {
		return engine_state;
	}	
}
