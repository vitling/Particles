/*
    Copyright 2021 David Whiting

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <JuceHeader.h>

#include <utility>
#include "ParticleSimulation.h"
#include "ParticleSimulationVisualiser.h"
#include "ParticleSynth.h"
#include "BasicStereoSynthPlugin.h"

namespace Params {
    using StrConst = const char * const;
    StrConst MULTIPLIER = "particle_multiplier";
    StrConst GRAVITY = "gravity";
    StrConst ATTACK = "attack_time";
    StrConst DECAY = "decay_half_life";
    StrConst MASTER = "master_volume";
    StrConst WAVEFORM = "waveform";
    StrConst ORIGIN = "particle_origin";
    StrConst SCALE = "scale";
    StrConst SIZE_BY_NOTE = "size_by_note";

    StringArray all() {
        return {
            MULTIPLIER,
            ORIGIN,
            GRAVITY,
            SCALE,
            SIZE_BY_NOTE,
            WAVEFORM,
            ATTACK,
            DECAY,
            MASTER,
        };
    }

    namespace Origin {
        StrConst TOP_LEFT = "Top Left";
        StrConst RANDOM_INSIDE = "Random Inside";
        StrConst RANDOM_OUTSIDE = "Random Outside";
        StrConst TOP_RANDOM = "Top Random";
        StringArray all() {
            return {TOP_LEFT, TOP_RANDOM, RANDOM_INSIDE, RANDOM_OUTSIDE};
        }
    }
}


inline auto param(const String& pid, const String& name, const NormalisableRange<float>& range, float def) {
    return std::make_unique<AudioParameterFloat>(pid, name, range, def);
}

inline auto param(const String& pid, const String& name, const StringArray& choices, const String& def) {
    int defaultIndex = choices.indexOf(def);
    // if you pass an invalid default then just use the first one
    if (defaultIndex == -1) defaultIndex = 0;
    return std::make_unique<AudioParameterChoice>(pid, name, choices, defaultIndex);
}

inline auto param(const String& pid, const String& name, bool def) {
    return std::make_unique<AudioParameterBool>(pid, name, def);
}



class ParticlesAudioProcessor : public BasicStereoSynthPlugin, public AudioProcessorValueTreeState::Listener  {
private:
    friend class ParticlesPluginEditor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticlesAudioProcessor)

    const int samplesPerSimulationStep = 64;

    ParticleSynth synth;
    ParticleSimulation sim;

    // Keep track of samples so we know when to step the simulation
    int sampleStepCounter = 0;

    // Duration in seconds between note on and note off. This is useful primarily when taking the midi side output and
    // using with another synth
    const float noteLength = 0.1f;

    // Used to cycle channels for the same note, meaning that multiple copies of the same note can play at a time
    std::map<int, int> lastChannelForNote;

    // When the note off for a note is past the current chunk of samples, we must store it in a buffer to use next time
    MidiBuffer overflowBuffer;

    AudioProcessorValueTreeState state {
        *this,
        nullptr,
        "ParticleSim", {
            param(Params::MULTIPLIER, "Particle Multiplier", {1.0f, 20.0f, 1.0f}, 5.0f),
            param(Params::GRAVITY, "Gravity", {0.0f, 2.0f, 0.01f}, 0.0f),
            param(Params::ATTACK, "Attack Time(s)", {0.001f, 0.1f, 0.001f}, 0.01f),
            param(Params::DECAY, "Decay half-life(s)", {0.001f, 0.5f, 0.001f}, 0.05f),
            param(Params::MASTER, "Master Volume (dB)", {-12.0f, 3.0f, 0.01f}, 0.0f),
            param(Params::WAVEFORM, "Sin->Saw", {0.0f, 1.0f, 0.01f}, 0.0f),
            param(Params::ORIGIN, "Particle Origin" , Params::Origin::all(), Params::Origin::RANDOM_INSIDE),
            param(Params::SCALE, "Particle Scale Factor", {0.1f, 2.0f, 0.01f}, 1.0f),
            param(Params::SIZE_BY_NOTE, "Note->Size", true)
        }
    };

    // Mapping between text value of origin parameter and ParticleOrigin enum value
    std::map<String, ParticleOrigin> originMapping = {
            {Params::Origin::TOP_RANDOM, ParticleOrigin::TOP_RANDOM},
            {Params::Origin::RANDOM_OUTSIDE, ParticleOrigin::RANDOM_OUTSIDE},
            {Params::Origin::RANDOM_INSIDE, ParticleOrigin::RANDOM_INSIDE},
            {Params::Origin::TOP_LEFT, ParticleOrigin::TOP_LEFT}
    };

    // Keep well-typed pointers to non-float parameters to avoid messy dynamic_casts in the parameterChanged function
    AudioParameterChoice *particleOrigin;
    AudioParameterBool *sizeByNote;

    void addStateListeners(AudioProcessorValueTreeState::Listener * listener, const StringArray& parameters) {
        for (auto &p: parameters) {
            state.addParameterListener(p, listener);
        }
    }

public:
    ParticlesAudioProcessor(): BasicStereoSynthPlugin("Particles") {

        addStateListeners(this, {
                Params::MULTIPLIER,
                Params::GRAVITY,
                Params::ORIGIN,
                Params::SIZE_BY_NOTE,
                Params::SCALE
        });

        addStateListeners(&synth, {
                Params::ATTACK,
                Params::DECAY,
                Params::WAVEFORM
        });

        // ideally we wouldn't have to cast at all, but the AudioProcessorValueStateTree stores everything as a
        // RangedAudioParameter* so we cast here to fail fast if we fuck up rather than crash in the change handler
        particleOrigin = dynamic_cast<AudioParameterChoice*>(state.getParameter(Params::ORIGIN));
        sizeByNote = dynamic_cast<AudioParameterBool*>(state.getParameter(Params::SIZE_BY_NOTE));
    }

    ~ParticlesAudioProcessor() override = default;

    AudioProcessorValueTreeState & parameterState() override { return state; }

    void parameterChanged (const String& parameterID, float newValue) override {
        if (parameterID == Params::MULTIPLIER) {
            sim.setParticleMultiplier(static_cast<int>(newValue));
        } else if (parameterID == Params::GRAVITY) {
            sim.setGravity(newValue);
        } else if (parameterID == Params::ORIGIN) {
            auto choice = particleOrigin->getCurrentChoiceName();
            sim.setParticleOrigin(originMapping[choice]);
        } else if (parameterID == Params::SIZE_BY_NOTE) {
            sim.setSizeByNote(sizeByNote->get());
        } else if (parameterID == Params::SCALE) {
            sim.setScale(newValue);
        }
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        synth.setCurrentPlaybackSampleRate(sampleRate);
    }
    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midiInput) override {
        int maximumMidiFutureInSamples = 40000;

        MidiBuffer simulationMidiEvents;

        simulationMidiEvents.addEvents(overflowBuffer, 0, maximumMidiFutureInSamples, 0);
        overflowBuffer.clear();

        auto nextMidiEvent = midiInput.findNextSamplePosition(0);
        int noteLengthSamples = int(noteLength * getSampleRate());

        for (auto i = 0; i < audio.getNumSamples(); i++) {

            // Process midi input to add/remove particles from the simulation
            while (nextMidiEvent != midiInput.end() && (*nextMidiEvent).samplePosition <= i) {
                const auto &event = (*nextMidiEvent);
                if (event.getMessage().isNoteOn()) {
                    sim.addNote(event.getMessage().getNoteNumber(), event.getMessage().getFloatVelocity());
                } else if (event.getMessage().isNoteOff()) {
                    sim.removeNote(event.getMessage().getNoteNumber());
                }
                nextMidiEvent++;
            }

            // Step simulation when appropriate to produce midi data to feed to the synthesiser
            if (sampleStepCounter++ >= samplesPerSimulationStep) {

                // The simuation is coded with an assumption of 256 samples per step, so if we configure a different
                // precision here we need to apply a scaling factor
                float simulationTimeScale = float(samplesPerSimulationStep) / 256.0f;

                sim.step([&] (int midiNote, float velocity, float pan) {

                    // Cycle round all 16 channels, allowing up to 16 copies of the same note playing simultanously
                    lastChannelForNote[midiNote] = (lastChannelForNote[midiNote] + 1) % 16;

                    // MidiMessage understanding of 'channel' is 1-based, not 0-based
                    int ch = lastChannelForNote[midiNote] + 1;

                    simulationMidiEvents.addEvent(MidiMessage::controllerEvent(ch, 10, static_cast<int>((pan + 1.0f)*64.0f)), i);
                    simulationMidiEvents.addEvent(MidiMessage::noteOn(ch, midiNote,velocity), i);
                    simulationMidiEvents.addEvent(MidiMessage::noteOff(ch, midiNote), i + noteLengthSamples);
                }, simulationTimeScale);
                sampleStepCounter = 0;
            }
        }
        audio.clear();

        MidiBuffer midiEventsForCurrentSampleRange;
        midiEventsForCurrentSampleRange.addEvents(simulationMidiEvents, 0, audio.getNumSamples(), 0);

        // Anything past the current sample range gets put into the overflow buffer for the next cycle
        overflowBuffer.addEvents(simulationMidiEvents, audio.getNumSamples(), maximumMidiFutureInSamples, -audio.getNumSamples());

        synth.renderNextBlock(audio, midiEventsForCurrentSampleRange, 0,audio.getNumSamples());

        audio.applyGain(pow(10, getParameterValue(Params::MASTER)/10));

        // If we want to allow midi "sidechain" output (the only way to support plugin midi effects in some hosts) then
        // we need to leave some midi data in the buffer that we were given at the start
        midiInput.clear();
        midiInput.addEvents(midiEventsForCurrentSampleRange, 0, audio.getNumSamples(), 0);
    }

    // TODO derive from attack/decay settings
    double getTailLengthSeconds() const override {
        return 0.5;
    }

    AudioProcessorEditor* createEditor() override;
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParticlesAudioProcessor();
}

class ParticlesPluginEditor: public AudioProcessorEditor {
private:
    struct ParameterControl {
        Slider slider;
        SliderParameterAttachment attachment;
        Label label;
        explicit ParameterControl(RangedAudioParameter& param): attachment(param, slider) {}
    };

    ParticleSimulationVisualiser simulationVisualiser;
    std::vector<std::unique_ptr<ParameterControl>> parameterControls;
public:
    explicit ParticlesPluginEditor(ParticlesAudioProcessor &proc):
            AudioProcessorEditor(proc),
            simulationVisualiser(proc.sim) {
        // Default size on the small side (in case of small screen)
        setSize(800,600);

        // Allow user to resize within sensible limits so that we can still show all controls and a reasonable
        // picture of the simulation
        setResizable(true, true);
        setResizeLimits(700, 500, 1400, 1200);

        // Create default rotary controllers for all parameters exposed in the parameter state
        createSimpleControls(proc.state, Params::all());

        addAndMakeVisible(simulationVisualiser);

        // Don't wait until resize to set the bounds of subcomponents
        doLayout();
    }

    virtual ~ParticlesPluginEditor() = default;

    void createSimpleControls(AudioProcessorValueTreeState& state, const StringArray& parameters) {
        for (auto &param: parameters) {
            // Each parameter gets a rotary slider and a label, which the editor takes ownership of via a vector of
            // unique_ptrs so they get cleaned up automatically at destruction
            auto control = std::make_unique<ParameterControl>(*state.getParameter(param));

            control->slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
            control->slider.setTextBoxStyle(Slider::TextBoxBelow, false, 100,20);
            addAndMakeVisible(control->slider);

            control->label.setText(state.getParameter(param)->getName(20),NotificationType::dontSendNotification);
            control->label.setJustificationType(Justification::centred);
            addAndMakeVisible(control->label);

            parameterControls.push_back(std::move(control));

        }
    }

    void doLayout() {
        auto bounds = getLocalBounds();

        // Lay out parameter controls in a grid
        const auto controlWidth = 100, controlHeight = 100, controlPanelWidth = 200;
        auto x = 0, y = 0;
        for (auto &control: parameterControls) {
            control->slider.setBounds(x,y,controlWidth,controlHeight-20);
            control->label.setBounds(x,y + controlHeight-20,controlWidth,20);
            x+=controlWidth;
            if (x >= controlPanelWidth) {
                x = 0;
                y+=controlHeight;
            }
        }

        // Use the rest of the available space right of the control panel for the simulation visualiser
        simulationVisualiser.setBounds(controlPanelWidth,0,bounds.getWidth()-controlPanelWidth, bounds.getHeight());
    }

    void resized() override {
        doLayout();
    }

    ColourGradient colourfulBackground() {
        ColourGradient grad(Colour::fromRGB(210,115,20), 0,0, Colour::fromRGB(104,217,240), 0, float(getHeight()),false);
        grad.addColour(0.5f, Colour::fromRGB(153,70,171));
        return grad;
    }

    void paint(Graphics &g) override {
        g.setGradientFill(colourfulBackground());
        g.fillAll();
    }
};

AudioProcessorEditor * ParticlesAudioProcessor::createEditor() { return new ParticlesPluginEditor(*this); }




