#pragma once
#include <string>
#include <vuk/Image.hpp>
#include <vuk/ImageAttachment.hpp>

#include "Core/Base.h"

namespace Oxylus {
struct TextureLoadInfo {
  std::string Path = {};
  uint32_t Width = 0;
  uint32_t Height = 0;
  void* Data = nullptr;
  vuk::Format Format = vuk::Format::eR8G8B8A8Unorm;
};

class TextureAsset {
public:
  TextureAsset() = default;
  TextureAsset(const std::string& path);
  TextureAsset(const TextureLoadInfo& info);
  ~TextureAsset();

  void CreateTexture(uint32_t x, uint32_t y, void* data, vuk::Format format = vuk::Format::eR8G8B8A8Unorm);
  void Load(const std::string& path, vuk::Format format = vuk::Format::eR8G8B8A8Unorm);
  void LoadFromMemory(void* initialData, size_t size);
  vuk::ImageAttachment AsAttachment() const;

  const std::string& GetPath() const { return m_Path; }
  const vuk::Texture& GetTexture() const { return m_Texture; }

  static void CreateBlankTexture();
  static void CreateWhiteTexture();
  static Ref<TextureAsset> GetPurpleTexture() { return s_PurpleTexture; }
  static Ref<TextureAsset> GetWhiteTexture() { return s_WhiteTexture; }

private:
  vuk::Texture m_Texture;
  std::string m_Path = {};
  static Ref<TextureAsset> s_PurpleTexture;
  static Ref<TextureAsset> s_WhiteTexture;
};
}
