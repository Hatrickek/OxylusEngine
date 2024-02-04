#pragma once

#include <string>
#include <stdint.h>

namespace Ox {
class EditorPanel {
public:
  bool Visible;

  EditorPanel(const char* name = "Unnamed Panel", const char8_t* icon = u8"", bool defaultShow = false);
  virtual ~EditorPanel() = default;

  EditorPanel(const EditorPanel& other) = delete;
  EditorPanel(EditorPanel&& other) = delete;
  EditorPanel& operator=(const EditorPanel& other) = delete;
  EditorPanel& operator=(EditorPanel&& other) = delete;

  virtual void on_update() { }

  virtual void on_imgui_render() = 0;

  const char* get_name() const { return m_Name.c_str(); }
  const char* get_id() const { return m_ID.c_str(); }
  const char8_t* get_icon() const { return m_Icon; }

protected:
  bool on_begin(int32_t window_flags = 0);
  void on_end() const;

  std::string m_Name;
  const char8_t* m_Icon;
  std::string m_ID;

private:
  static uint32_t s_Count;
};
}
