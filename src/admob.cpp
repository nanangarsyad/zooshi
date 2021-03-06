// Copyright 2016 Google Inc. All rights reserved.
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

#include "admob.h"

#include "mathfu/internal/disable_warnings_begin.h"

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/remote_config.h"

#include "mathfu/internal/disable_warnings_end.h"

#include "fplbase/utilities.h"
#include "remote_config.h"
#include "world.h"

#if defined(ZOOSHI_DEFINE_SIZED_OPERATOR_DELETE) && defined(__GNUC__)
  // For linking against Firebase libraries.
  // The Firebase libraries are built with Clang, which no longer emits a
  // sized operator delete (it used to emit a weakly linked one).
  // GCC does not emit one either, so we must define it here.
  // See b/33306060.
  void operator delete(void* ptr, unsigned long /*sz*/) {
    ::operator delete(ptr);
  }
#endif  // defined(ZOOSHI_DEFINE_SIZED_OPERATOR_DELETE) && defined(__GNUC__)

namespace fpl {
namespace zooshi {

namespace rewarded_video = firebase::admob::rewarded_video;

// Zooshi specific ad units that only serve test ads.
// In order to create your own ads, you'll need your own AdMob account.
// More information can be found at:
// https://support.google.com/admob/answer/2773509
const char* kAdMobAppID = "ca-app-pub-3940256099942544~1891588914";
const char* kRewardedVideoAdUnit = "ca-app-pub-3940256099942544/4705454513";

AdMobHelper::~AdMobHelper() {
  if (rewarded_video_available()) {
    rewarded_video::Destroy();
  }
}

// static
void AdMobHelper::InitializeCompletion(
    const firebase::Future<void>& completed_future, void* void_helper) {
  AdMobHelper* helper = static_cast<AdMobHelper*>(void_helper);
  if (completed_future.Error()) {
    fplbase::LogError("Failed to initialize rewarded video: %s",
                      completed_future.ErrorMessage());
    helper->rewarded_video_status_ = kAdMobStatusError;
  } else {
    rewarded_video::SetListener(&helper->listener_);
    helper->LoadNewRewardedVideo();
  }
}

void AdMobHelper::Initialize(const firebase::App& app) {
  firebase::admob::Initialize(app, kAdMobAppID);
  rewarded_video_status_ = kAdMobStatusInitializing;
  rewarded_video::Initialize().OnCompletion(InitializeCompletion,
                                            this);
}

// static
void AdMobHelper::LoadNewRewardedVideoCompletion(
    const firebase::Future<void>& completed_future, void* void_helper) {
  AdMobHelper* helper = static_cast<AdMobHelper*>(void_helper);
  if (completed_future.Error()) {
    fplbase::LogError("Failed to load rewarded video: %s",
                      completed_future.ErrorMessage());
    helper->rewarded_video_status_ = kAdMobStatusError;
  } else {
    helper->rewarded_video_status_ = kAdMobStatusAvailable;
  }
}

void AdMobHelper::LoadNewRewardedVideo() {
  rewarded_video_status_ = kAdMobStatusLoading;
  // TODO(amaurice) Fill in request with information.
  firebase::admob::AdRequest request;
  std::memset(&request, 0, sizeof(request));
  rewarded_video::LoadAd(kRewardedVideoAdUnit, request)
      .OnCompletion(LoadNewRewardedVideoCompletion, this);
}


// static
void AdMobHelper::ShowRewardedVideoCompletion(
    const firebase::Future<void>& completed_future, void* void_helper) {
  AdMobHelper* helper = static_cast<AdMobHelper*>(void_helper);
  if (completed_future.Error()) {
    fplbase::LogError("Failed to show rewarded video: %s",
                      completed_future.ErrorMessage());
    helper->rewarded_video_status_ = kAdMobStatusError;
    helper->listener_.set_expecting_state_change(false);
  }
}

void AdMobHelper::ShowRewardedVideo() {
  if (rewarded_video_status_ != kAdMobStatusAvailable) {
    fplbase::LogError("Unable to show rewarded video, not available");
    return;
  }

  rewarded_video_status_ = kAdMobStatusShowing;
  listener_.set_expecting_state_change(true);
#ifdef __ANDROID__
  firebase::admob::AdParent ad_parent = fplbase::AndroidGetActivity();
#else
  firebase::admob::AdParent ad_parent = nullptr;
#endif  // __ANDROID__
  rewarded_video::Show(ad_parent)
      .OnCompletion(ShowRewardedVideoCompletion, this);
}

bool AdMobHelper::CheckShowRewardedVideo() {
  // If still Loading, wait until it is finished.
  if (rewarded_video_status_ == kAdMobStatusLoading) {
    return false;
  } else if (rewarded_video_status_ == kAdMobStatusShowing) {
    if (!listener_.expecting_state_change() &&
        listener_.presentation_state() ==
            rewarded_video::kPresentationStateHidden) {
      rewarded_video_status_ = kAdMobStatusAvailable;
      return true;
    }
    return false;
  } else {
    // If we are not showing a rewarded video, return true.
    return true;
  }
}

RewardedVideoLocation AdMobHelper::GetRewardedVideoLocation() {
  auto location =
      firebase::remote_config::GetLong(kConfigRewardedVideoLocation);
  if (location < 0 || location >= kRewardedVideoLocationCount) {
    location = 0;
  }
  return static_cast<RewardedVideoLocation>(location);
}

RewardedVideoListener::RewardedVideoListener()
    : earned_reward_(false),
      expecting_state_change_(false),
      presentation_state_(rewarded_video::kPresentationStateHidden),
      reward_item_() {}

void RewardedVideoListener::OnPresentationStateChanged(
    rewarded_video::PresentationState state) {
  presentation_state_ = state;
  expecting_state_change_ = false;
}

void RewardedVideoListener::OnRewarded(rewarded_video::RewardItem reward) {
  earned_reward_ = true;
  reward_item_ = reward;
  fplbase::LogInfo("Rewarded Video: Earned Reward: %s: %f",
                   reward.reward_type.c_str(), reward.amount);
}

}  // zooshi
}  // fpl
