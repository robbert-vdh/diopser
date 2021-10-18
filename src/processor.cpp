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
constexpr char filter_resonance_param_name[] = "filter_res";
constexpr char filter_spread_param_name[] = "filter_spread";

/**
 * When the filter cutoff or resonance parameters change, we'll interpolate
 * between the old and the new values over the course of this time span to
 * prevent clicks.
 */
constexpr float filter_smoothing_secs = 0.1f;

/**
 * The default filter resonance. This value should minimize the amount of
 * resonances. In the GUI we should also be snapping to this value.
 *
 * The actual default neutral Q-value would be `sqrt(2) / 2`, but this value
 * produces slightly less ringing.
 */
constexpr float default_filter_resonance = 0.5f;

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
                  // TODO: Some combinations of parameters can cause really loud
                  //       resonance. We should limit the resonance and filter
                  //       stages parameter ranges in the GUI until the user
                  //       unlocks.
                  std::make_unique<juce::AudioParameterInt>(
                      filter_stages_param_name,
                      "Filter Stages",
                      0,
                      512,
                      0),
                  // For some reason Disperser's frequency is a bit off, but
                  // ours is actually correct with respect to 440 Hz = A tuning.
                  // TODO: Figure out some way to get rid of the resonances when
                  //       sweep the frequency down when using a large number of
                  //       stages
                  std::make_unique<juce::AudioParameterFloat>(
                      filter_frequency_param_name,
                      "Filter Frequency",
                      juce::NormalisableRange<float>(5.0f,
                                                     20000.0f,
                                                     1.0f,
                                                     0.2f),
                      200.0f,
                      " Hz",
                      juce::AudioProcessorParameter::genericParameter,
                      [](float value, int /*max_length*/) -> juce::String {
                          return juce::String(value, 0);
                      }),
                  // TODO: Perhaps display this range as something nicer
                  std::make_unique<juce::AudioParameterFloat>(
                      filter_resonance_param_name,
                      "Filter Resonance",
                      juce::NormalisableRange<float>(0.01f, 30.0f, 0.01f, 0.2f),
                      default_filter_resonance),
                  std::make_unique<juce::AudioParameterFloat>(
                      filter_spread_param_name,
                      "Filter spread",
                      juce::NormalisableRange<
                          float>(-5000.0f, 5000.0f, 1.0f, 0.3f, true),
                      0.0f,
                      " Hz",
                      juce::AudioProcessorParameter::genericParameter,
                      [](float value, int /*max_length*/) -> juce::String {
                          return juce::String(value, 0);
                      })),
              std::make_unique<juce::AudioParameterBool>(
                  "please_ignore",
                  "Don't touch this",
                  true,
                  "",
                  [](float value, int /*max_length*/) -> juce::String {
                      return (value > 0.0) ? "please don't" : "stop it";
                  }),
          }),
      // TODO: Is this how you're supposed to retrieve non-float parameters?
      //       Seems a bit excessive
      filter_stages(*dynamic_cast<juce::AudioParameterInt*>(
          parameters.getParameter(filter_stages_param_name))),
      filter_frequency(
          *parameters.getRawParameterValue(filter_frequency_param_name)),
      filter_resonance(
          *parameters.getRawParameterValue(filter_resonance_param_name)),
      filter_spread(*parameters.getRawParameterValue(filter_spread_param_name)),
      filter_stages_updater([&]() { update_and_swap_filters(); }),
      filter_stages_listener(
          [&](const juce::String& /*parameter_id*/, float /*new_value*/) {
              // Resize our filter vector from a background thread
              filter_stages_updater.triggerAsyncUpdate();
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

    // After initializing the filters we make an explicit call to
    // `filters.get()` to swap the two filters in case we get a parameter change
    // before the first processing cycle. Updating the filters will also set the
    // `is_initialized` flag to `false`, so the filter coefficients will be
    // initialized during the first processing cycle.
    update_and_swap_filters();
    filters.get();

    // The filter parameter will be smoothed to prevent clicks during automation
    smoothed_filter_frequency.reset(sampleRate, filter_smoothing_secs);
    smoothed_filter_resonance.reset(sampleRate, filter_smoothing_secs);
    smoothed_filter_spread.reset(sampleRate, filter_smoothing_secs);
}

void DiopserProcessor::releaseResources() {
    filters.clear([](Filters& filters) {
        filters.stages.clear();
        filters.stages.shrink_to_fit();
    });
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

    // TODO: Is there a way to get the VST3 silence flags? Carla, and perhaps
    //        also some other hosts, enable a lot more channels than the user is
    //        likely going to use, so we'll end up wasting a ton of resources on
    //        processing silcence.
    float** samples = buffer.getArrayOfWritePointers();
    const size_t input_channels =
        static_cast<size_t>(getMainBusNumInputChannels());
    const size_t output_channels =
        static_cast<size_t>(getMainBusNumOutputChannels());
    const size_t num_samples = static_cast<size_t>(buffer.getNumSamples());

    for (auto channel = input_channels; channel < output_channels; channel++) {
        buffer.clear(channel, 0.0f, num_samples);
    }

    // Our filter structure gets updated from a background thread whenever the
    // `filter_stages` parameter changes
    Filters& filters = this->filters.get();

    smoothed_filter_frequency.setTargetValue(filter_frequency);
    smoothed_filter_resonance.setTargetValue(filter_resonance);
    smoothed_filter_spread.setTargetValue(filter_spread);
    for (size_t sample_idx = 0; sample_idx < num_samples; sample_idx++) {
        const bool should_update_filters =
            !filters.is_initialized ||
            smoothed_filter_frequency.isSmoothing() ||
            smoothed_filter_resonance.isSmoothing() ||
            smoothed_filter_spread.isSmoothing();
        const float current_filter_frequency =
            smoothed_filter_frequency.getNextValue();
        const float current_filter_resonance =
            smoothed_filter_resonance.getNextValue();
        const float current_filter_spread =
            smoothed_filter_spread.getNextValue();

        if (should_update_filters && !filters.stages.empty()) {
            // We can use a single set of coefficients as a cache locality
            // optimization if spread has been disabled
            const bool use_single_set_of_coefficients =
                current_filter_spread == 0.0f;
            if (use_single_set_of_coefficients) {
                *filters.stages[0].coefficients =
                    juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
                        getSampleRate(), current_filter_frequency,
                        current_filter_resonance);
            }

            // The filter spread will be logarithmic because that sounds a bit
            // more natural. We also need to make sure the spread range stays in
            // the normal ranges to prevent the filters from crapping out. This
            // does cause the range to shift slightly with high spread values
            // and low or high frequency values. Ideally we would want to
            // prevent this in the GUI.
            // TODO: When adding a GUI, prevent spread values that would cause
            //       the frequency range to be shifted
            const float below_nyquist_frequency =
                static_cast<float>(getSampleRate()) / 2.1f;
            const float min_filter_frequency_log = std::log(std::clamp(
                current_filter_frequency - (current_filter_spread / 2.0f), 5.0f,
                below_nyquist_frequency));
            const float max_filter_frequency_log = std::log(std::clamp(
                current_filter_frequency + (current_filter_spread / 2.0f), 5.0f,
                below_nyquist_frequency));
            const float log_filter_frequency_delta =
                max_filter_frequency_log - min_filter_frequency_log;

            const size_t num_stages = filters.stages.size();
            for (size_t stage_idx = 0; stage_idx < num_stages; stage_idx++) {
                auto& stage = filters.stages[stage_idx];

                if (!use_single_set_of_coefficients) {
                    // TODO: Maybe add back the option for simple linear
                    //       skewing. Or use the same skew scheme JUCE's
                    //       parameter range uses and make the skew factor
                    //       configurable.
                    const float frequency_offset_factor =
                        num_stages == 1 ? 0.5f
                                        : (static_cast<float>(stage_idx) /
                                           static_cast<float>(num_stages - 1));

                    *stage.coefficients =
                        juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
                            getSampleRate(),
                            std::exp(min_filter_frequency_log +
                                     (log_filter_frequency_delta *
                                      frequency_offset_factor)),
                            current_filter_resonance);
                }

                const auto& coefficients = use_single_set_of_coefficients
                                               ? filters.stages[0].coefficients
                                               : stage.coefficients;
                for (auto& filter : stage.channels) {
                    filter.coefficients = coefficients;
                }
            }
        }

        filters.is_initialized = true;
        for (auto& stage : filters.stages) {
            for (size_t channel = 0; channel < input_channels; channel++) {
                // TODO: We should add a dry-wet control, could be useful for
                //       automation
                // TODO: Oh and we should _definitely_ have some kind of 'safe
                //       mode' limiter enabled by default
                samples[channel][sample_idx] =
                    stage.channels[channel].processSample(
                        samples[channel][sample_idx]);
            }
        }
    }
}

bool DiopserProcessor::hasEditor() const {
    return false;
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

void DiopserProcessor::update_and_swap_filters() {
    filters.modify_and_swap([this](Filters& filters) {
        filters.is_initialized = false;
        filters.stages.resize(static_cast<size_t>(filter_stages));

        for (auto& stage : filters.stages) {
            // The actual coefficients for each stage are initialized on the
            // next processing cycle thanks to `filters.is_initialized`
            if (!stage.coefficients) {
                // The actual values here don't matter and we can just use
                // any 6 length array
                stage.coefficients = new juce::dsp::IIR::Coefficients<float>(
                    FilterStage::Coefficients{});
            }

            stage.channels.resize(
                static_cast<size_t>(getMainBusNumOutputChannels()));
            for (auto& filter : stage.channels) {
                filter.prepare(current_spec);
                filter.coefficients = stage.coefficients;
                filter.reset();
            }
        }
    });
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DiopserProcessor();
}
