// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "game.h"
#include "fplbase/debug_markers.h"
#include "fplbase/utilities.h"
#include "states/game_menu_state.h"
#include "states/states_common.h"

#include <iostream>

using mathfu::vec2;
using mathfu::vec3;

namespace fpl {
namespace zooshi {

flatui::Event GameMenuState::PlayButtonSound(flatui::Event event,
                                          pindrop::SoundHandle& sound) {
  if (event & flatui::kEventWentUp) {
    audio_engine_->PlaySound(sound);
  }
  return event;
}

flatui::Event GameMenuState::TextButton(const char* text, float size,
                                     const flatui::Margin& margin) {
  return TextButton(text, size, margin, sound_click_);
}

flatui::Event GameMenuState::TextButton(const char* text, float size,
                                     const flatui::Margin& margin,
                                     pindrop::SoundHandle& sound) {
  return PlayButtonSound(flatui::TextButton(text, size, margin), sound);
}

flatui::Event GameMenuState::TextButton(const fplbase::Texture& texture,
                                     const flatui::Margin& texture_margin,
                                     const char* text, float size,
                                     const flatui::Margin& margin,
                                     const flatui::ButtonProperty property) {
  return TextButton(texture, texture_margin, text, size, margin, property,
                    sound_click_);
}

flatui::Event GameMenuState::TextButton(const fplbase::Texture& texture,
                                     const flatui::Margin& texture_margin,
                                     const char* text, float size,
                                     const flatui::Margin& margin,
                                     const flatui::ButtonProperty property,
                                     pindrop::SoundHandle& sound) {
  return PlayButtonSound(flatui::TextButton(texture, texture_margin, text, size,
                                         margin, property), sound);
}

flatui::Event GameMenuState::ImageButtonWithLabel(const fplbase::Texture& tex,
                                                  float size,
                                                  const flatui::Margin& margin,
                                                  const char* label) {
  return ImageButtonWithLabel(tex, size, margin, label, sound_click_);
}

flatui::Event GameMenuState::ImageButtonWithLabel(const fplbase::Texture& tex,
                                                  float size,
                                                  const flatui::Margin& margin,
                                                  const char* label,
                                                  pindrop::SoundHandle& sound) {
  flatui::StartGroup(flatui::kLayoutVerticalLeft, size, "ImageButtonWithLabel");
  flatui::SetMargin(margin);
  auto event = PlayButtonSound(flatui::CheckEvent(), sound);
  flatui::EventBackground(event);
  flatui::ImageBackground(tex);
  flatui::Label(label, size);
  flatui::EndGroup();
  return event;
}

MenuState GameMenuState::StartMenu(fplbase::AssetManager& assetman,
                                   flatui::FontManager& fontman,
                                   fplbase::InputSystem& input) {
  MenuState next_state = kMenuStateStart;

  PushDebugMarker("StartMenu");

  // Run() accepts a lambda function that is executed 2 times,
  // one for a layout pass and another one in a render pass.
  // In the lambda callback, the user can call Widget APIs to put widget in a
  // layout.
  flatui::Run(assetman, fontman, input, [&]() {
    flatui::StartGroup(flatui::kLayoutHorizontalTop, 0);

    // Background image.
    flatui::StartGroup(flatui::kLayoutVerticalCenter, 0);
    // Positioning the UI slightly above of the center.
    flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                       mathfu::vec2(0, -150));
    flatui::Image(*background_title_, 1400);
    flatui::EndGroup();

    flatui::SetTextColor(kColorBrown);
    flatui::SetTextFont(config_->menu_font()->c_str());

    // Menu items. Note that we are layering 2 layouts here
    // (background + menu items).
    flatui::StartGroup(flatui::kLayoutVerticalCenter, 0);
    flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                       mathfu::vec2(0, -150));
    flatui::SetMargin(flatui::Margin(200, 700, 200, 100));

    auto event = TextButton("Play Game", kMenuSize, flatui::Margin(0),
                            sound_start_);
    if (event & flatui::kEventWentUp) {
      next_state = kMenuStateFinished;
#ifdef ANDROID_GAMEPAD
      if (!flatui::IsLastEventPointerType()) {
        next_state = kMenuStateGamepad;
      }
#endif
    }
    if (fplbase::SupportsHeadMountedDisplay()) {
      event = TextButton("Cardboard", kMenuSize, flatui::Margin(0),
                         sound_start_);
      if (event & flatui::kEventWentUp) {
        next_state = kMenuStateCardboard;
      }
    }
#ifdef USING_GOOGLE_PLAY_GAMES
    auto logged_in = gpg_manager_->LoggedIn();
    event = TextButton(*image_gpg_, flatui::Margin(0, 50, 10, 0),
                       logged_in ? "Sign out" : "Sign in", kMenuSize,
                       flatui::Margin(0), flatui::kButtonPropertyImageLeft,
                       sound_select_);
    if (event & flatui::kEventWentUp) {
      gpg_manager_->ToggleSignIn();
    }
#endif
    event = TextButton("Options", kMenuSize, flatui::Margin(0));
    if (event & flatui::kEventWentUp) {
      next_state = kMenuStateOptions;
      options_menu_state_ = kOptionsMenuStateMain;
    }
    event = TextButton("Quit", kMenuSize, flatui::Margin(0), sound_exit_);
    if (event & flatui::kEventWentUp) {
      // The exit sound is actually around 1.2s but since we fade out the
      // audio as well as the screen it's reasonable to shorten the duration.
      static const int kFadeOutTimeMilliseconds = 1000;
      fader_->Start(
          kFadeOutTimeMilliseconds, mathfu::kZeros3f, kFadeOut,
          vec3(vec2(flatui::VirtualToPhysical(mathfu::kZeros2f)), 0.0f),
          vec3(vec2(flatui::VirtualToPhysical(flatui::GetVirtualResolution())),
               0.0f));
      next_state = kMenuStateQuit;
    }
    flatui::EndGroup();

    // Sushi selection is done offset to the right of the menu layout.
    const SushiConfig* current_sushi = world_->SelectedSushi();
    flatui::StartGroup(flatui::kLayoutVerticalCenter, 0);
    flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                          mathfu::vec2(375, 100));
    flatui::SetTextColor(kColorLightBrown);
    event =
        ImageButtonWithLabel(*button_back_, 60, flatui::Margin(60, 35, 40, 50),
                             current_sushi->name()->c_str());
    if (event & flatui::kEventWentUp) {
      next_state = kMenuStateOptions;
      options_menu_state_ = kOptionsMenuStateSushi;
    }
    flatui::EndGroup();
    flatui::EndGroup();
  });

  PopDebugMarker(); // StartMenu

  return next_state;
}

MenuState GameMenuState::OptionMenu(fplbase::AssetManager& assetman,
                                    flatui::FontManager& fontman,
                                    fplbase::InputSystem& input) {
  MenuState next_state = kMenuStateOptions;

  PushDebugMarker("OptionMenu");

  // FlatUI UI definitions.
  flatui::Run(assetman, fontman, input, [&]() {
    flatui::StartGroup(flatui::kLayoutOverlay, 0);
    flatui::StartGroup(flatui::kLayoutHorizontalTop, 0);
    // Background image. Note that we are layering 3 layouts here
    // (background + menu items + back button).
    flatui::StartGroup(flatui::kLayoutVerticalCenter, 0);
    // Positioning the UI slightly above of the center.
    flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                       mathfu::vec2(0, -150));
    flatui::Image(*background_options_, 1400);
    flatui::EndGroup();

    flatui::SetTextColor(kColorBrown);
    flatui::SetTextFont(config_->menu_font()->c_str());

    // Menu items.
    flatui::StartGroup(flatui::kLayoutVerticalCenter, 0);
    flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                       mathfu::vec2(0, -100));

    switch (options_menu_state_) {
      case kOptionsMenuStateMain:
        OptionMenuMain();
        break;
      case kOptionsMenuStateAbout:
        OptionMenuAbout();
        break;
      case kOptionsMenuStateLicenses:
        OptionMenuLicenses();
        break;
      case kOptionsMenuStateAudio:
        OptionMenuAudio();
        break;
      case kOptionsMenuStateRendering:
        OptionMenuRendering();
        break;
      case kOptionsMenuStateSushi:
        OptionMenuSushi();
        break;
      default:
        break;
    }

    flatui::EndGroup();

    // Foreground image (back button).
    flatui::StartGroup(flatui::kLayoutVerticalRight, 0);
    // Positioning the UI to up-left corner of the dialog.
    flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                       mathfu::vec2(-450, -250));
    flatui::SetTextColor(kColorLightBrown);

    auto event = ImageButtonWithLabel(*button_back_, 60,
                                      flatui::Margin(60, 35, 40, 50), "Back",
                                      sound_exit_);
    if (event & flatui::kEventWentUp) {
      // Save data when you leave the audio or rendering page.
      if (options_menu_state_ == kOptionsMenuStateAudio ||
          options_menu_state_ == kOptionsMenuStateRendering) {
        SaveData();
      }
      if (options_menu_state_ == kOptionsMenuStateMain ||
          options_menu_state_ == kOptionsMenuStateSushi) {
        next_state = kMenuStateStart;
      } else {
        options_menu_state_ = kOptionsMenuStateMain;
      }
    }
    flatui::EndGroup();
    flatui::EndGroup();

    flatui::EndGroup();  // Overlay group.
  });

  PopDebugMarker(); // OptionMenu

  return next_state;
}

void GameMenuState::OptionMenuMain() {
  flatui::SetMargin(flatui::Margin(200, 300, 200, 100));

  flatui::StartGroup(flatui::kLayoutVerticalLeft, 50, "menu");
  flatui::SetMargin(flatui::Margin(0, 20, 0, 50));
  flatui::SetTextColor(kColorBrown);
  flatui::EndGroup();

  if (TextButton("About", kButtonSize, flatui::Margin(2),
                 sound_select_) & flatui::kEventWentUp) {
    options_menu_state_ = kOptionsMenuStateAbout;
  }

#ifdef USING_GOOGLE_PLAY_GAMES
  auto logged_in = gpg_manager_->LoggedIn();
  auto property = flatui::kButtonPropertyImageLeft;

  if (!logged_in) {
    flatui::SetTextColor(kColorLightGray);
    property |= flatui::kButtonPropertyDisabled;
  }
  auto event = TextButton(*image_leaderboard_, flatui::Margin(0, 25, 10, 0),
                          "Leaderboard", kButtonSize, flatui::Margin(0),
                          property);
  if (logged_in && (event & flatui::kEventWentUp)) {
    // Fill in Leaderboard list.
    auto leaderboard_config = config_->gpg_config()->leaderboards();
    gpg_manager_->ShowLeaderboards(
        leaderboard_config->LookupByKey(kGPGDefaultLeaderboard)->id()->c_str());
  }

  event = TextButton(*image_achievements_, flatui::Margin(0, 20, 0, 0),
                     "Achievements", kButtonSize, flatui::Margin(0), property);
  if (logged_in && (event & flatui::kEventWentUp)) {
    gpg_manager_->ShowAchievements();
  }
  flatui::SetTextColor(kColorBrown);
#endif

  if (TextButton("Licenses", kButtonSize, flatui::Margin(2)) &
      flatui::kEventWentUp) {
    scroll_offset_ = mathfu::kZeros2f;
    options_menu_state_ = kOptionsMenuStateLicenses;
  }

  if (TextButton("Audio", kButtonSize, flatui::Margin(2)) &
      flatui::kEventWentUp) {
    options_menu_state_ = kOptionsMenuStateAudio;
  }

  if (TextButton("Rendering", kButtonSize, flatui::Margin(2)) &
      flatui::kEventWentUp) {
    options_menu_state_ = kOptionsMenuStateRendering;
  }

#ifdef ANDROID_HMD
  // If the device supports a head mounted display allow the user to toggle
  // between gyroscopic and onscreen controls.
  if (fplbase::SupportsHeadMountedDisplay()) {
    const bool hmd_controller_enabled = world_->GetHmdControllerEnabled();
    if (TextButton(hmd_controller_enabled ? "Gyroscopic Controls" :
                   "Onscreen Controls", kButtonSize, flatui::Margin(2)) &
        flatui::kEventWentUp) {
      world_->SetHmdControllerEnabled(!hmd_controller_enabled);
      SaveData();
    }
  }
#endif  // ANDROID_HMD
}

void GameMenuState::OptionMenuAbout() {
  flatui::SetMargin(flatui::Margin(200, 400, 200, 100));

  flatui::StartGroup(flatui::kLayoutVerticalLeft, 50, "menu");
  flatui::SetMargin(flatui::Margin(0, 20, 0, 55));
  flatui::SetTextColor(kColorBrown);
  flatui::Label("About", kButtonSize);
  flatui::EndGroup();

  flatui::SetTextColor(kColorDarkGray);
  flatui::SetTextFont(config_->license_font()->c_str());

  flatui::StartGroup(flatui::kLayoutHorizontalCenter);
  flatui::SetMargin(flatui::Margin(50, 0, 0, 0));
  flatui::StartGroup(flatui::kLayoutVerticalCenter, 0, "scroll");
  flatui::StartScroll(kScrollAreaSize, &scroll_offset_);
  flatui::Label(about_text_.c_str(), 35, vec2(kScrollAreaSize.x(), 0));
  vec2 scroll_size = flatui::GroupSize();
  flatui::EndScroll();
  flatui::EndGroup();

  // Normalize the scroll offset to use for the scroll bar value.
  auto scroll_height = (scroll_size.y() - kScrollAreaSize.y());
  if (scroll_height > 0) {
    auto scrollbar_value = scroll_offset_.y() / scroll_height;
    flatui::ScrollBar(*scrollbar_back_, *scrollbar_foreground_,
                   vec2(35, kScrollAreaSize.y()),
                   kScrollAreaSize.y() / scroll_size.y(), "LicenseScrollBar",
                   &scrollbar_value);

    // Convert back the scroll bar value to the scroll offset.
    scroll_offset_.y() = scrollbar_value * scroll_height;
  }

  flatui::EndGroup();
  flatui::SetTextFont(config_->menu_font()->c_str());
}

void GameMenuState::OptionMenuLicenses() {
  flatui::SetMargin(flatui::Margin(200, 300, 200, 100));

  flatui::StartGroup(flatui::kLayoutVerticalLeft, 50, "menu");
  flatui::SetMargin(flatui::Margin(0, 20, 0, 55));
  flatui::SetTextColor(kColorBrown);
  flatui::Label("Licenses", kButtonSize);
  flatui::EndGroup();

  flatui::SetTextColor(kColorDarkGray);
  flatui::SetTextFont(config_->license_font()->c_str());

  flatui::StartGroup(flatui::kLayoutHorizontalCenter);
  flatui::SetMargin(flatui::Margin(50, 0, 0, 0));
  flatui::StartGroup(flatui::kLayoutVerticalCenter, 0, "scroll");
  // This check event makes the scroll group controllable with a gamepad.
  flatui::StartScroll(kScrollAreaSize, &scroll_offset_);
  auto event = flatui::CheckEvent(true);
  if (!flatui::IsLastEventPointerType())
    flatui::EventBackground(event);
  flatui::Label(license_text_.c_str(), 25, vec2(kScrollAreaSize.x(), 0));
  vec2 scroll_size = flatui::GroupSize();
  flatui::EndScroll();
  flatui::EndGroup();

  // Normalize the scroll offset to use for the scroll bar value.
  auto scroll_height = (scroll_size.y() - kScrollAreaSize.y());
  if (scroll_height > 0) {
    auto scrollbar_value = scroll_offset_.y() / scroll_height;
    flatui::ScrollBar(*scrollbar_back_, *scrollbar_foreground_,
                   vec2(35, kScrollAreaSize.y()),
                   kScrollAreaSize.y() / scroll_size.y(), "LicenseScrollBar",
                   &scrollbar_value);

    // Convert back the scroll bar value to the scroll offset.
    scroll_offset_.y() = scrollbar_value * scroll_height;
  }

  flatui::EndGroup();
  flatui::SetTextFont(config_->menu_font()->c_str());
}

void GameMenuState::OptionMenuAudio() {
  auto original_music_volume = slider_value_music_;
  auto original_effect_volume = slider_value_effect_;
  flatui::SetMargin(flatui::Margin(200, 200, 200, 100));

  flatui::StartGroup(flatui::kLayoutVerticalLeft, 50, "menu");
  flatui::SetMargin(flatui::Margin(0, 50, 0, 50));
  flatui::SetTextColor(kColorBrown);
  flatui::Label("Audio", kButtonSize);
  flatui::EndGroup();

  flatui::StartGroup(flatui::kLayoutHorizontalCenter, 20);
  flatui::Label("Music volume", kAudioOptionButtonSize);
  flatui::SetMargin(flatui::Margin(0, 40, 0, 0));
  flatui::Slider(*slider_back_, *slider_knob_, vec2(400, 60), 0.6f,
                 "MusicVolume", &slider_value_music_);

  flatui::EndGroup();
  flatui::StartGroup(flatui::kLayoutHorizontalCenter, 20);
  flatui::Label("Effect volume", kAudioOptionButtonSize);
  flatui::SetMargin(flatui::Margin(0, 40, 0, 0));
  auto event = flatui::Slider(*slider_back_, *slider_knob_, vec2(400, 60), 0.6f,
                           "EffectVolume", &slider_value_effect_);
  if (event & (flatui::kEventWentUp | flatui::kEventEndDrag)) {
    audio_engine_->PlaySound(sound_adjust_);
  }
  flatui::EndGroup();

  if (original_music_volume != slider_value_music_ ||
      original_effect_volume != slider_value_effect_) {
    UpdateVolumes();
  }
}

void GameMenuState::OptionMenuRendering() {
  flatui::SetMargin(flatui::Margin(200, 200, 200, 100));

  flatui::StartGroup(flatui::kLayoutVerticalLeft, 50, "menu");
  flatui::SetMargin(flatui::Margin(0, 50, 0, 50));
  flatui::SetTextColor(kColorBrown);
  flatui::Label("Rendering", kButtonSize);
  flatui::EndGroup();

  bool render_shadows = world_->RenderingOptionEnabled(kShadowEffect);
  bool apply_phong = world_->RenderingOptionEnabled(kPhongShading);
  bool apply_specular = world_->RenderingOptionEnabled(kSpecularEffect);

  bool render_shadows_cardboard =
      world_->RenderingOptionEnabledCardboard(kShadowEffect);
  bool apply_phong_cardboard =
      world_->RenderingOptionEnabledCardboard(kPhongShading);
  bool apply_specular_cardboard =
      world_->RenderingOptionEnabledCardboard(kSpecularEffect);

  flatui::StartGroup(flatui::kLayoutHorizontalTop, 10);
  flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter, mathfu::kZeros2f);

  if (fplbase::SupportsHeadMountedDisplay()) {
    flatui::StartGroup(flatui::kLayoutVerticalLeft, 20);
    flatui::SetMargin(flatui::Margin(0, 50, 0, 50));
    flatui::Image(*cardboard_logo_, kButtonSize);
    flatui::CheckBox(*button_checked_, *button_unchecked_, "", kButtonSize,
                     flatui::Margin(0), &render_shadows_cardboard);
    flatui::CheckBox(*button_checked_, *button_unchecked_, "", kButtonSize,
                     flatui::Margin(0), &apply_phong_cardboard);
    flatui::CheckBox(*button_checked_, *button_unchecked_, "", kButtonSize,
                     flatui::Margin(0), &apply_specular_cardboard);
    flatui::EndGroup();
  }

  flatui::StartGroup(flatui::kLayoutVerticalCenter, 20);
  flatui::StartGroup(flatui::kLayoutVerticalLeft, 20);
  flatui::SetMargin(flatui::Margin(0, 70 + kButtonSize, 0, 50));
  flatui::CheckBox(*button_checked_, *button_unchecked_, "Shadows", kButtonSize,
                   flatui::Margin(6, 0), &render_shadows);
  flatui::CheckBox(*button_checked_, *button_unchecked_, "Phong Shading",
                   kButtonSize, flatui::Margin(6, 0), &apply_phong);
  flatui::CheckBox(*button_checked_, *button_unchecked_, "Specular",
                   kButtonSize, flatui::Margin(6, 0), &apply_specular);
  flatui::EndGroup();
  flatui::EndGroup();
  flatui::EndGroup();

  world_->SetRenderingOptionCardboard(kShadowEffect, render_shadows_cardboard);
  world_->SetRenderingOptionCardboard(kPhongShading, apply_phong_cardboard);
  world_->SetRenderingOptionCardboard(kSpecularEffect,
                                      apply_specular_cardboard);
  world_->SetRenderingOption(kShadowEffect, render_shadows);
  world_->SetRenderingOption(kPhongShading, apply_phong);
  world_->SetRenderingOption(kSpecularEffect, apply_specular);

  SaveData();
}

void GameMenuState::OptionMenuSushi() {
  flatui::SetMargin(flatui::Margin(200, 400, 200, 100));

  // Render information about the currently selected sushi.
  auto current_sushi = world_->SelectedSushi();
  flatui::StartGroup(flatui::kLayoutVerticalCenter, 10, "menu");
  flatui::PositionGroup(flatui::kAlignCenter, flatui::kAlignCenter,
                        mathfu::vec2(30, -210));
  flatui::SetTextColor(kColorBrown);
  flatui::Label(current_sushi->name()->c_str(), kButtonSize);
  flatui::SetTextColor(kColorDarkGray);
  flatui::Label(current_sushi->description()->c_str(), kButtonSize - 5);
  flatui::EndGroup();

  // Render the different sushi types that can be selected.
  flatui::StartGroup(flatui::kLayoutVerticalCenter, 20);
  flatui::SetTextColor(kColorLightBrown);
  const size_t kSushiPerLine = 3;
  for (size_t i = 0; i < config_->sushi_config()->size(); i += kSushiPerLine) {
    flatui::StartGroup(flatui::kLayoutHorizontalCenter, 20);
    for (size_t j = 0;
         j < kSushiPerLine && i + j < config_->sushi_config()->size(); ++j) {
      auto event = ImageButtonWithLabel(
          *button_back_, 60, flatui::Margin(60, 35, 40, 50),
          config_->sushi_config()->Get(i + j)->name()->c_str());
      if (event & flatui::kEventWentUp) {
        world_->sushi_index = i + j;
      }
    }
    flatui::EndGroup();
  }
  flatui::EndGroup();
}

}  // zooshi
}  // fpl
