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
#include <juce_dsp/juce_dsp.h>
#include <function2/function2.hpp>

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

class DiopserProcessor : public juce::AudioProcessor {
   public:
    DiopserProcessor();
    ~DiopserProcessor() override;

    void prepareToPlay(double sampleRate,
                       int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlockBypassed(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlockBypassed;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

   private:
    /**
     * Add, remove, and initialize the all-pass filters based on the
     * `filter_stages` parameter.
     */
    void init_filters();

    /**
     * The current processing spec. Needed when adding more filters when the
     * number of stages changes.
     */
    juce::dsp::ProcessSpec current_spec;

    /**
     * Our all-pass filters. The vector is indexed by
     * `[filter_idx][channel_idx]`. The number of filters and the frequency of
     * the filters is controlled using the `filter_stages` and
     * `filter_frequency` parameters.
     */
    std::vector<std::vector<juce::dsp::IIR::Filter<float>>> filters;
    /**
     * All filters will use these same filter coefficients, so we can just
     * update the coefficients for all filters in one place. This especially
     * makes automation a lot more cache friendly.
     */
    juce::dsp::IIR::Filter<float>::CoefficientsPtr filter_coefficients;

    juce::AudioProcessorValueTreeState parameters;

    juce::AudioParameterInt& filter_stages;
    std::atomic<float>& filter_frequency;
    std::atomic<float>& filter_resonance;
    juce::SmoothedValue<float> smoothed_filter_frequency;
    juce::SmoothedValue<float> smoothed_filter_resonance;

    /**
     * Will add or remove filters when the number of filter stages changes.
     */
    LambdaParameterListener filter_stages_listener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiopserProcessor)
};
