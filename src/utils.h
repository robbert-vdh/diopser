// Diopser: a phase rotation plugin
// Copyright (C) 2021 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <function2/function2.hpp>

/**
 * Run some function on the message thread. This function will be executed
 * synchronously and should thus run in constant time.
 */
class LambdaAsyncUpdater : public juce::AsyncUpdater {
   public:
    LambdaAsyncUpdater(fu2::unique_function<void()> callback);

    void handleAsyncUpdate() override;

   private:
    fu2::unique_function<void()> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LambdaAsyncUpdater)
};

/**
 * Run some function whenever a parameter changes. This function will be
 * executed synchronously and should thus run in constant time.
 */
class LambdaParameterListener
    : public juce::AudioProcessorValueTreeState::Listener {
   public:
    LambdaParameterListener(
        fu2::unique_function<void(const juce::String&, float)> callback);

    void parameterChanged(const juce::String& parameterID,
                          float newValue) override;

   private:
    fu2::unique_function<void(const juce::String&, float)> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LambdaParameterListener)
};
