#pragma once

#include <glm/glm.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <ryml.hpp>
#include <c4/yml/std/string.hpp>


namespace glm {
  inline void write(ryml::NodeRef* n, glm::vec2 const& val) {
    *n |= ryml::SEQ;
    *n |= ryml::_WIP_STYLE_FLOW_SL;
    n->append_child() << val.x;
    n->append_child() << val.y;
  }

  inline void write(ryml::NodeRef* n, glm::vec3 const& val) {
    *n |= ryml::SEQ;
    *n |= ryml::_WIP_STYLE_FLOW_SL;
    n->append_child() << val.x;
    n->append_child() << val.y;
    n->append_child() << val.z;
  }

  inline void write(ryml::NodeRef* n, glm::vec4 const& val) {
    *n |= ryml::SEQ;
    *n |= ryml::_WIP_STYLE_FLOW_SL;
    n->append_child() << val.x;
    n->append_child() << val.y;
    n->append_child() << val.z;
    n->append_child() << val.w;
  }

  inline void write(ryml::NodeRef* n, glm::quat const& val) {
    *n |= ryml::SEQ;
    *n |= ryml::_WIP_STYLE_FLOW_SL;
    n->append_child() << val.w;
    n->append_child() << val.x;
    n->append_child() << val.y;
    n->append_child() << val.z;
  }

  inline void write(ryml::NodeRef* n, glm::mat3 const& val) {
    *n |= ryml::SEQ;
    *n |= ryml::_WIP_STYLE_FLOW_SL;
    n->append_child() << val[0];
    n->append_child() << val[1];
    n->append_child() << val[2];
  }

  inline void write(ryml::NodeRef* n, glm::mat4 const& val) {
    *n |= ryml::SEQ;
    *n |= ryml::_WIP_STYLE_FLOW_SL;
    n->append_child() << val[0];
    n->append_child() << val[1];
    n->append_child() << val[2];
    n->append_child() << val[3];
  }

  inline bool read(ryml::ConstNodeRef const& n, glm::vec2* val) {
    if (n.num_children() < 2)
      return false;
    n[0] >> val->x;
    n[1] >> val->y;
    return true;
  }

  inline bool read(ryml::ConstNodeRef const& n, glm::vec3* val) {
    if (n.num_children() < 3)
      return false;
    n[0] >> val->x;
    n[1] >> val->y;
    n[2] >> val->z;
    return true;
  }

  inline bool read(ryml::ConstNodeRef const& n, glm::vec4* val) {
    if (n.num_children() < 4)
      return false;
    n[0] >> val->x;
    n[1] >> val->y;
    n[2] >> val->z;
    n[3] >> val->w;
    return true;
  }

  inline bool read(ryml::ConstNodeRef const& n, glm::quat* val) {
    if (n.num_children() < 4)
      return false;
    n[0] >> val->w;
    n[1] >> val->x;
    n[2] >> val->y;
    n[3] >> val->z;
    return true;
  }

  inline bool read(ryml::ConstNodeRef const& n, glm::mat3* val) {
    if (n.num_children() < 3)
      return false;
    n[0] >> (*val)[0];
    n[1] >> (*val)[1];
    n[2] >> (*val)[2];
    return true;
  }

  inline bool read(ryml::ConstNodeRef const& n, glm::mat4* val) {
    if (n.num_children() < 4)
      return false;
    n[0] >> (*val)[0];
    n[1] >> (*val)[1];
    n[2] >> (*val)[2];
    n[3] >> (*val)[3];
    return true;
  }
}

namespace Oxylus {
  template <typename T>
  static void TryLoad(ryml::ConstNodeRef node, const char* name, T& value) {
    const auto n = c4::to_csubstr(name);
    if (node.has_child(n))
      node[n] >> value;
  }
}
