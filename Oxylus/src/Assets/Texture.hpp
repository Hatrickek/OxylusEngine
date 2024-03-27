#pragma once
#include <string>
#include <vuk/Image.hpp>
#include <vuk/ImageAttachment.hpp>

#include "Asset.hpp"

#include "Core/Base.hpp"

namespace ox {
struct TextureLoadInfo {
  std::string path = {};
  uint32_t width = 0;
  uint32_t height = 0;
  void* data = nullptr;
  vuk::Format format = vuk::Format::eR8G8B8A8Unorm;
  bool generate_mips = true;
  bool generate_cubemap_from_hdr = true;
};

class Texture : public Asset {
public:
  Texture() = default;
  Texture(const std::string& file_path);
  Texture(const TextureLoadInfo& info);
  ~Texture();

  void create_texture(vuk::Extent3D extent,
                      vuk::Format format = vuk::Format::eR8G8B8A8Unorm,
                      vuk::ImageAttachment::Preset preset = vuk::ImageAttachment::Preset::eGeneric2D);
  void create_texture(uint32_t width, uint32_t height, const void* data, vuk::Format format = vuk::Format::eR8G8B8A8Unorm, bool generate_mips = true);
  void load(const std::string& file_path,
            vuk::Format format = vuk::Format::eR8G8B8A8Unorm,
            bool generate_cubemap_from_hdr = true,
            bool generate_mips = true);
  void load_from_memory(void* initial_data, size_t size);
  vuk::ImageAttachment as_attachment() const;

  const std::string& get_path() const { return path; }
  const vuk::Unique<vuk::Image>& get_image() const { return image; }
  const vuk::Unique<vuk::ImageView>& get_view() const { return view; }
  const vuk::Extent3D& get_extent() const { return extent; }

  explicit operator uint64_t() { return view->id; }

  static void create_white_texture();
  static Shared<Texture> get_white_texture() { return s_white_texture; }

  /// Loads the given file using stb. Returned data must be freed manually.
  static uint8_t* load_stb_image(const std::string& filename,
                                 uint32_t* width = nullptr,
                                 uint32_t* height = nullptr,
                                 uint32_t* bits = nullptr,
                                 bool srgb = true);
  static uint8_t* load_stb_image_from_memory(void* buffer,
                                             size_t len,
                                             uint32_t* width = nullptr,
                                             uint32_t* height = nullptr,
                                             uint32_t* bits = nullptr,
                                             bool flipY = false,
                                             bool srgb = true);

  static uint8_t* get_magenta_texture(uint32_t width, uint32_t height, uint32_t channels);

  static uint8_t* convert_to_four_channels(uint32_t width, uint32_t height, const uint8_t* three_channel_data);

private:
  std::string path = {};
  vuk::Unique<vuk::Image> image;
  vuk::Unique<vuk::ImageView> view;
  vuk::Extent3D extent;
  vuk::Format format;

  static Shared<Texture> s_white_texture;
};
} // namespace ox
