#pragma once
#include <string>
#include <vuk/Image.hpp>
#include <vuk/ImageAttachment.hpp>

#include "Asset.h"

#include "Core/Base.h"

namespace Oxylus {
struct TextureLoadInfo {
  std::string path = {};
  uint32_t width = 0;
  uint32_t height = 0;
  void* data = nullptr;
  vuk::Format format = vuk::Format::eR8G8B8A8Unorm;
  bool generate_mips = true;
  bool generate_cubemap_from_hdr = true;
};

class TextureAsset : public Asset {
public:
  TextureAsset() = default;
  TextureAsset(const std::string& file_path);
  TextureAsset(const TextureLoadInfo& info);
  ~TextureAsset();

  void create_texture(uint32_t x,
                      uint32_t y,
                      void* data,
                      vuk::Format format = vuk::Format::eR8G8B8A8Unorm,
                      bool generate_mips = true);
  void load(const std::string& file_path,
            vuk::Format format = vuk::Format::eR8G8B8A8Unorm,
            bool generate_cubemap_from_hdr = true,
            bool generate_mips = true);
  void load_from_memory(void* initial_data, size_t size);
  vuk::ImageAttachment as_attachment() const;

  const std::string& get_path() const { return path; }
  const vuk::Texture& get_texture() const { return texture; }

  static void create_white_texture();
  static Ref<TextureAsset> get_white_texture() { return s_white_texture; }

private:
  vuk::Texture texture;
  std::string path = {};
  static Ref<TextureAsset> s_white_texture;
};
}
