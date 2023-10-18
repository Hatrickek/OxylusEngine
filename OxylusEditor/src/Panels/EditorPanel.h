#pragma once

#include <string>
#include <stdint.h>

namespace Oxylus {
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

    const char* GetName() const {
      return m_Name.c_str();
    }

    const char8_t* GetIcon() const {
      return m_Icon;
    }

  protected:
    bool OnBegin(int32_t windowFlags = 0);
    void OnEnd() const;

    std::string m_Name;
    const char8_t* m_Icon;
    std::string m_ID;

  private:
    static uint32_t s_Count;
  };
}
