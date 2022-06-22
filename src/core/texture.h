#pragma once
#include <iostream>

#include "log.h"
#include "stb_image.h"
#include "glad/glad.h"

namespace FlatEngine {
	struct Texture {
		unsigned int id;
		std::string type;
		std::string path;
	};

	unsigned int load_texture(const char* path, bool gammaCorrection);
}
