#pragma once

namespace FlatEngine {
	enum EngineState {
		EDITING,
		PLAYING,
		PAUSED,
	};

	void SetEngineState(EngineState state);
	EngineState GetEngineState();
}
