#pragma once

#include <entt.hpp>

namespace Oxylus {
  using EventDispatcher = entt::dispatcher;

  /*enum class EventType {
    None = 0,
    ReloadSceneEvent,
  };

#define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; }\
								virtual EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }*/


  /*class Event {
  public:
    virtual ~Event() = default;
    bool Handled = false;

    virtual EventType GetEventType() const = 0;
    virtual const char* GetName() const = 0;
    virtual std::string ToString() const { return GetName(); }
  };

  class EventDispatcher {
  public:
    explicit EventDispatcher(Event& event)
      : m_Event(event) { }

    template <typename T, typename F>
    bool Dispatch(const F& func) {
      if (m_Event.GetEventType() == T::GetStaticType()) {
        m_Event.Handled |= func(static_cast<T&>(m_Event));
        return true;
      }
      return false;
    }

  private:
    Event& m_Event;
  };*/
}
