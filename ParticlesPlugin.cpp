

#include <JuceHeader.h>

#include <utility>
#include "Simulation.h"
#include "ParticleSynth.h"

class ParticlesPluginEditor;

namespace Params {
    using StrConst = const char * const;
    StrConst MULTIPLIER = "particle_multiplier";
    StrConst GRAVITY = "gravity";
    StrConst ATTACK = "attack_time";
    StrConst DECAY = "decay_half_life";
    StrConst MASTER = "master_volume";
    StrConst WAVEFORM = "waveform";
    StrConst GENERATION = "particle_generation";
    StrConst SCALE = "scale";
    StrConst SIZE_BY_NOTE = "size_by_note";

    StringArray all() {
        return {
            MULTIPLIER,
            GRAVITY,
            ATTACK,
            DECAY,
            MASTER,
            WAVEFORM,
            GENERATION,
            SCALE,
            SIZE_BY_NOTE
        };
    }
}


inline auto numParam(const String& pid, const NormalisableRange<float>& range, float def) {
    return std::make_unique<AudioParameterFloat>(pid, pid, range, def);
}

inline auto selectionParam(const String& pid, const StringArray& choices) {
    return std::make_unique<AudioParameterChoice>(pid, pid, choices, 0);
}

inline auto boolParam(const String& pid, bool def) {
    return std::make_unique<AudioParameterBool>(pid, pid, def);
}

class ParticlesAudioProcessor : public AudioProcessor, public AudioProcessorValueTreeState::Listener  {
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
            numParam(Params::MULTIPLIER, {1.0f, 20.0f, 1.0f}, 5.0f),
            numParam(Params::GRAVITY, {0.0f, 2.0f}, 0.0f),
            numParam(Params::ATTACK, {0.001f, 0.1f}, 0.01f),
            numParam(Params::DECAY, {0.001f, 0.5f}, 0.05f),
            numParam(Params::MASTER, {-12.0f, 3.0f}, 0.0f),
            numParam(Params::WAVEFORM, {0.0f, 1.0f}, 0.0f),
            selectionParam(Params::GENERATION, {"top_left", "random_inside", "random_outside", "top_random"}),
            numParam(Params::SCALE, {0.1f, 2.0f}, 1.0f),
            boolParam(Params::SIZE_BY_NOTE, true)
        }
    };

    // Keep well-typed pointers to non-float parameters to avoid messy dynamic_casts in the parameterChanged function
    AudioParameterChoice *particleGeneration;
    AudioParameterBool *sizeByNote;

    // Shortcut for getting true (non-normalised) float values out of a parameter tree
    float getParameterValue(StringRef parameterName) const {
        auto param = state.getParameter(parameterName);
        return param->convertFrom0to1(param->getValue());
    }

    void addStateListeners(AudioProcessorValueTreeState::Listener * listener, const StringArray& parameters) {
        for (auto &p: parameters) {
            state.addParameterListener(p, listener);
        }
    }

public:
    ParticlesAudioProcessor():
        AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)) {

        addStateListeners(this, {
                Params::MULTIPLIER,
                Params::GRAVITY,
                Params::GENERATION,
                Params::SIZE_BY_NOTE,
                Params::SCALE
        });

        addStateListeners(&synth, {
                Params::ATTACK,
                Params::DECAY,
                Params::WAVEFORM
        });

        // ideally we wouldn't have to cast at all, but the AudioProcessorValueStateTree stores everything as a
        // RangedAudioParameter* so we cast here to fail fast rather than crash in the change handler
        particleGeneration = dynamic_cast<AudioParameterChoice*>(state.getParameter(Params::GENERATION));
        sizeByNote = dynamic_cast<AudioParameterBool*>(state.getParameter(Params::SIZE_BY_NOTE));
    }

    ~ParticlesAudioProcessor() override = default;

    void parameterChanged (const String& parameterID, float newValue) override {
        if (parameterID == Params::MULTIPLIER) {
            sim.setParticleMultiplier(static_cast<int>(newValue));
        } else if (parameterID == Params::GRAVITY) {
            sim.setGravity(newValue);
        } else if (parameterID == Params::GENERATION) {
            auto choice = particleGeneration->getCurrentChoiceName();
            if (choice == "top_left") sim.setGenerationRule(TOP_LEFT);
            else if (choice == "random_inside") sim.setGenerationRule(RANDOM_INSIDE);
            else if (choice == "random_outside") sim.setGenerationRule(RANDOM_OUTSIDE);
            else if (choice == "top_random") sim.setGenerationRule(TOP_RANDOM);
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
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {return (layouts.getMainOutputChannels() == 2);}

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

    AudioProcessorEditor* createEditor() override;

    // Various metadata about the plugin
    bool hasEditor() const override { return true; }
    const String getName() const override { return "Particles";}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {
        return 0.0;
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 1; }
    void setCurrentProgram (int index) override {}
    const String getProgramName (int index) override { return "Default Program"; }
    void changeProgramName (int index, const String& newName) override { }

    void getStateInformation (MemoryBlock& destData) override {
        auto stateToSave = state.copyState();
        std::unique_ptr<XmlElement> xml (stateToSave.createXml());
        copyXmlToBinary(*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override {
        std::unique_ptr<XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
        if (xmlState != nullptr) {
            if (xmlState->hasTagName(state.state.getType())) {
                state.replaceState(ValueTree::fromXml(*xmlState));
            }
        }
    }

    const ParticleSimulation & simulation() {
        return sim;
    }
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParticlesAudioProcessor();
}

class ParamControl {
public:
    Slider slider;
    SliderParameterAttachment attachment;
    Label label;
    explicit ParamControl(RangedAudioParameter& param): attachment(param, slider) {}
};

class ParticlesPluginEditor: public AudioProcessorEditor {
private:
    ParticleSimulationVisualiser vis;
    std::vector<std::unique_ptr<ParamControl>> controls;

public:
    explicit ParticlesPluginEditor(ParticlesAudioProcessor &proc):
    AudioProcessorEditor(proc),
    vis(proc.simulation()) {
        generateUI(proc.state, Params::all());
        setSize(800,600);
        vis.setBounds(200,0,600,600);
        addAndMakeVisible(vis);
    }

    void generateUI(AudioProcessorValueTreeState& state, StringArray parameters) {
        auto x = 0;
        auto y = 0;
        auto cw = 100;
        auto ch = 100;
        for (auto &param: parameters) {
            auto control = std::make_unique<ParamControl>(*state.getParameter(param));
            addAndMakeVisible(control->slider);
            control->slider.setBounds(x,y,cw,ch-20);
            control->slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
            control->slider.setTextBoxStyle(Slider::TextBoxBelow, true, 100,20);
            control->label.setText(param,NotificationType::dontSendNotification);
            control->label.setJustificationType(Justification::centred);
            control->label.setBounds(x,y + ch-20,cw,20);
            addAndMakeVisible(control->slider);
            addAndMakeVisible(control->label);
            controls.push_back(std::move(control));
            x+=cw;
            if (x >= 200) {
                x = 0;
                y+=ch;
            }
        }
    }

    void paint(Graphics &g) override {
        ColourGradient grad(Colour::fromRGB(210,115,20), 0,0, Colour::fromRGB(104,217,240), 0,1000,false);
        grad.addColour(0.5f, Colour::fromRGB(153,70,171));
        g.setGradientFill(grad);
        g.fillAll();
    }
};

AudioProcessorEditor * ParticlesAudioProcessor::createEditor() { return new ParticlesPluginEditor(*this); }




