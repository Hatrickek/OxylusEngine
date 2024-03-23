#include "FileSystem.h"

#include "Core/PlatformDetection.h"

#include "Utils/StringUtils.h"

#ifdef OX_PLATFORM_WINDOWS
  #include <ShlObj_core.h>
  #include <comdef.h>
  #include <shellapi.h>
#endif

#include <filesystem>

#include "App.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace ox {
std::pair<std::string, std::string> FileSystem::split_path(std::string_view full_path) {
  const size_t found = full_path.find_last_of("/\\");
  return {
    std::string(full_path.substr(0, found + 1)), // directory
    std::string(full_path.substr(found + 1)),    // file
  };
}

std::string FileSystem::get_file_extension(const std::string_view filepath) {
  const auto lastDot = filepath.find_last_of('.');
  return std::string(filepath.substr(lastDot + 1, filepath.size() - lastDot));
}

std::string FileSystem::get_file_name(const std::string_view filepath) {
  auto [dir, file] = split_path(filepath);
  const auto lastDot = file.rfind('.');
  return std::string(file.substr(0, lastDot));
}

std::string FileSystem::get_name_with_extension(const std::string_view filepath) { return split_path(filepath).second; }

std::string FileSystem::get_directory(std::string_view path) {
  if (path.empty()) {
    return std::string(path);
  }

  return split_path(path).first;
}

std::string FileSystem::append_paths(const std::string_view path, const std::string_view second_path) {
  if (path.empty() || second_path.empty())
    return std::string(path);

  std::string new_path(path);
  new_path.append("/").append(second_path);
  return preferred_path(new_path);
}

std::string FileSystem::preferred_path(const std::string_view path) {
  std::string new_path(path);
  StringUtils::replace_string(new_path, "\\", "/");
  return new_path;
}

void FileSystem::open_folder_select_file(std::string_view path) {
#ifdef OX_PLATFORM_WINDOWS
  std::string p_str(path);
  StringUtils::replace_string(p_str, "/", "\\");
  const _bstr_t widePath(p_str.c_str());
  if (const auto pidl = ILCreateFromPath(widePath)) {
    SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0); // TODO: check for result
    ILFree(pidl);
  }
#else
  OX_CORE_WARN("Not implemented on this platform!");
#endif
}

void FileSystem::open_file_externally(std::string_view path) {
#ifdef OX_PLATFORM_WINDOWS
  const _bstr_t widePath(path.data());
  ShellExecute(nullptr, LPCSTR(L"open"), widePath, nullptr, nullptr, SW_RESTORE);
#else
  OX_LOG_WARN("Not implemented on this platform!");
#endif
}

std::string FileSystem::read_file(std::string_view file_path) {
  OX_SCOPED_ZONE;
  std::ostringstream buf;
  const std::ifstream input(file_path.data());
  buf << input.rdbuf();
  return buf.str();
}

std::vector<uint8_t> FileSystem::read_file_binary(const std::string_view file_path) {
  std::ifstream file(file_path.data(), std::ios::binary | std::ios::ate);

  std::vector<uint8_t> data = {};
  if (file.is_open()) {
    const size_t dataSize = file.tellg();
    file.seekg(0, file.beg);
    data.resize(dataSize);
    file.read((char*)data.data(), dataSize);
  }
  file.close();

  return data;
}

std::string FileSystem::read_shader_file(const std::string& shader_file_name) {
  OX_SCOPED_ZONE;
  const auto path = get_shader_path(shader_file_name);
  auto value = read_file(path);
  OX_ASSERT(!value.empty(), "Shader file doesn't exist: {0}", shader_file_name);
  return value;
}

std::string FileSystem::get_shader_path(const std::string& shader_file_name) {
  OX_SCOPED_ZONE;
  const auto path_str = std::filesystem::path(App::get()->get_specification().assets_path) / "Shaders";
  const auto path = std::filesystem::path(path_str) / shader_file_name;
  return path.string();
}

bool FileSystem::write_file_binary(std::string_view file_path, const std::vector<uint8_t>& data) {
  std::ofstream file(file_path.data(), std::ios::binary | std::ios::trunc);
  if (file.is_open()) {
    file.write((const char*)data.data(), (std::streamsize)data.size());
    file.close();
    return true;
  }
  return false;
}

bool FileSystem::binary_to_header(std::string_view file_path, std::string_view data_name, const std::vector<uint8_t>& data) {
  std::string ss;
  ss += "const uint8_t ";
  ss += data_name;
  ss += "[] = {";
  for (size_t i = 0; i < data.size(); ++i) {
    if (i % 32 == 0) {
      ss += "\n";
    }
    ss += std::to_string((uint32_t)data[i]) + ",";
  }
  ss += "\n};\n";
  return write_file(file_path, ss, "// Oxylus generated header file");
}
} // namespace ox
