#include "LuaAudioBindings.h"

#include <sol/state.hpp>

#include "LuaHelpers.h"

#include "Assets/AssetManager.h"

#include "Audio/AudioSource.h"

#include "Scene/Components.h"
#include "Scene/Entity.h"

namespace Ox::LuaBindings {
void bind_audio(const Shared<sol::state>& state) {
  auto audio_source = state->new_usertype<AudioSource>("AudioSource");
  SET_TYPE_FUNCTION(audio_source, AudioSource, get_path);
  SET_TYPE_FUNCTION(audio_source, AudioSource, play);
  SET_TYPE_FUNCTION(audio_source, AudioSource, pause);
  SET_TYPE_FUNCTION(audio_source, AudioSource, un_pause);
  SET_TYPE_FUNCTION(audio_source, AudioSource, stop);
  SET_TYPE_FUNCTION(audio_source, AudioSource, is_playing);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_config);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_volume);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_pitch);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_looping);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_spatialization);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_attenuation_model);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_roll_off);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_min_gain);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_max_gain);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_min_distance);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_max_distance);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_cone);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_doppler_factor);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_position);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_direction);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_velocity);

  auto audio_source_component_type = state->new_usertype<AudioSourceComponent>("AudioSourceComponent");
  SET_COMPONENT_TYPE_ID(audio_source_component_type, AudioSourceComponent);
  SET_TYPE_FIELD(audio_source_component_type, AudioSourceComponent, config);
  SET_TYPE_FIELD(audio_source_component_type, AudioSourceComponent, source);
  register_meta_component<AudioSourceComponent>();

  auto audio_source_config_type = state->new_usertype<AudioSourceConfig>("AudioSourceConfig");
  const std::initializer_list<std::pair<sol::string_view, AttenuationModelType>> attenuation_model_type = {
    {"Inverse", AttenuationModelType::Inverse},
    {"Linear", AttenuationModelType::Linear},
    {"Exponential", AttenuationModelType::Exponential}
  };
  state->new_enum<AttenuationModelType, true>("AttenuationModelType", attenuation_model_type);

  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, volume_multiplier);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, pitch_multiplier);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, play_on_awake);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, looping);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, spatialization);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, attenuation_model);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, roll_off);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, min_gain);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, max_gain);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, min_distance);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, max_distance);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, cone_inner_angle);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, cone_outer_angle);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, cone_outer_gain);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, doppler_factor);

  sol::usertype<AudioListenerComponent> audio_listener_component_type = state->new_usertype<AudioListenerComponent>("AudioListenerComponent");
  SET_COMPONENT_TYPE_ID(audio_listener_component_type, AudioListenerComponent);
  SET_TYPE_FIELD(audio_listener_component_type, AudioListenerComponent, active);
  SET_TYPE_FIELD(audio_listener_component_type, AudioListenerComponent, config);
  SET_TYPE_FIELD(audio_listener_component_type, AudioListenerComponent, listener);
  register_meta_component<AudioListenerComponent>();
  sol::usertype<AudioListenerConfig> audio_listener_config_type = state->new_usertype<AudioListenerConfig>("AudioListenerConfig");
  SET_TYPE_FIELD(audio_listener_config_type, AudioListenerConfig, cone_inner_angle);
  SET_TYPE_FIELD(audio_listener_config_type, AudioListenerConfig, cone_outer_angle);
  SET_TYPE_FIELD(audio_listener_config_type, AudioListenerConfig, cone_outer_gain);
}
}
