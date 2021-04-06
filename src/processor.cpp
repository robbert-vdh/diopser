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

#include "processor.h"

#include "editor.h"

using juce::uint32;

constexpr char filter_settings_group_name[] = "filters";
constexpr char filter_stages_param_name[] = "filter_stages";
constexpr char filter_frequency_param_name[] = "filter_freq";

LambdaParameterListener::LambdaParameterListener(
    fu2::unique_function<void(const juce::String&, float)> callback)
    : callback(std::move(callback)) {}

void LambdaParameterListener::parameterChanged(const juce::String& parameterID,
                                               float newValue) {
    callback(parameterID, newValue);
}

DiopserProcessor::DiopserProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(
          *this,
          nullptr,
          "parameters",
          {
              std::make_unique<juce::AudioProcessorParameterGroup>(
                  filter_settings_group_name,
                  "Filters",
                  " | ",
                  std::make_unique<juce::AudioParameterInt>(
                      filter_stages_param_name,
                      "Filter Stages",
                      0,
                      256,
                      0),
                  std::make_unique<juce::AudioParameterFloat>(
                      filter_frequency_param_name,
                      "Filter Frequency",
                      juce::NormalisableRange<float>(0.0, 10000.0),
                      20.0,
                      " Hz",
                      juce::AudioProcessorParameter::genericParameter,
                      [](float value, int /*max_length*/) -> juce::String {
                          return juce::String(value, 0);
                      })),
          }),
      // TODO: Is this how you're supposed to retrieve non-float parameters?
      //       Seems a bit excessive
      filter_stages(*dynamic_cast<juce::AudioParameterInt*>(
          parameters.getParameter(filter_stages_param_name))),
      filter_frequency(
          *parameters.getRawParameterValue(filter_frequency_param_name)),
      filter_stages_listener(
          [&](const juce::String& /*parameterID*/, float /*newValue*/) {
              // FIXME: We should do a lockfree resize of the filters vector
              //        here instead of doing allocations in `processBlock()`
          }) {
    parameters.addParameterListener(filter_stages_param_name,
                                    &filter_stages_listener);
}

DiopserProcessor::~DiopserProcessor() {}

const juce::String DiopserProcessor::getName() const {
    return JucePlugin_Name;
}

bool DiopserProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool DiopserProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool DiopserProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double DiopserProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int DiopserProcessor::getNumPrograms() {
    return 1;
}

int DiopserProcessor::getCurrentProgram() {
    return 0;
}

void DiopserProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String DiopserProcessor::getProgramName(int /*index*/) {
    return "default";
}

void DiopserProcessor::changeProgramName(int /*index*/,
                                         const juce::String& /*newName*/) {}

void DiopserProcessor::prepareToPlay(double sampleRate,
                                     int maximumExpectedSamplesPerBlock) {
    current_spec = juce::dsp::ProcessSpec{
        .sampleRate = sampleRate,
        .maximumBlockSize = static_cast<uint32>(maximumExpectedSamplesPerBlock),
        .numChannels = static_cast<uint32>(getMainBusNumInputChannels())};

    filters.resize(static_cast<size_t>(filter_stages));
    for (auto& filter : filters) {
        filter.prepare(current_spec);
        filter.setType(juce::dsp::FirstOrderTPTFilterType::allpass);
    }

    smoothed_filter_frequency.reset(sampleRate, 0.1);
}

void DiopserProcessor::releaseResources() {
    filters.clear();
}

bool DiopserProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
    // We can support any number of channels, as long as the main input and
    // output have the same number of channels
    return (layouts.getMainInputChannelSet() ==
            layouts.getMainOutputChannelSet()) &&
           !layouts.getMainInputChannelSet().isDisabled();
}

void DiopserProcessor::processBlockBypassed(
    juce::AudioBuffer<float>& /*buffer*/,
    juce::MidiBuffer& /*midiMessages*/) {
    // TODO: The default should be fine if we don't introduce any latency
}

void DiopserProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midiMessages*/) {
    juce::AudioBuffer<float> main_buffer = getBusBuffer(buffer, true, 0);
    juce::ScopedNoDenormals noDenormals;

    float** samples = buffer.getArrayOfWritePointers();
    const size_t input_channels =
        static_cast<size_t>(getMainBusNumInputChannels());
    const size_t output_channels =
        static_cast<size_t>(getMainBusNumOutputChannels());
    const size_t num_samples = static_cast<size_t>(buffer.getNumSamples());

    for (auto channel = input_channels; channel < output_channels; channel++) {
        buffer.clear(channel, 0.0f, num_samples);
    }

    // We'll temporarily update the number of filters here from the audio thread
    // as a proof of concept
    // FIXME: We should be doing this lockfree from another thread using two
    //        vectors of filters
    const size_t old_num_filters = filters.size();
    filters.resize(static_cast<size_t>(filter_stages));
    for (size_t i = old_num_filters; i < filters.size(); i++) {
        filters[i].prepare(current_spec);
        filters[i].setType(juce::dsp::FirstOrderTPTFilterType::allpass);
    }

    smoothed_filter_frequency.setTargetValue(filter_frequency);
    for (size_t sample = 0; sample < num_samples; sample++) {
        const float current_filter_frequency =
            smoothed_filter_frequency.getNextValue();

        for (size_t channel = 0; channel < input_channels; channel++) {
            for (auto& filter : filters) {
                filter.setCutoffFrequency(current_filter_frequency);
                samples[channel][sample] =
                    filter.processSample(channel, samples[channel][sample]);
            }
        }
    }
}

bool DiopserProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* DiopserProcessor::createEditor() {
    // TODO: Add an editor at some point
    // return new DiopserEditor(*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

void DiopserProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const std::unique_ptr<juce::XmlElement> xml =
        parameters.copyState().createXml();
    copyXmlToBinary(*xml, destData);
}

void DiopserProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(parameters.state.getType())) {
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DiopserProcessor();
}
