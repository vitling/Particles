

#include <JuceHeader.h>

#include <utility>
#include "Simulation.h"
#include "ParticleSynth.h"

class ParticlesPluginEditor;

inline auto param(const String& pid, float minValue, float maxValue, float def) {
    return std::make_unique<AudioParameterFloat>(pid, pid, minValue, maxValue, def);
}

class ParticlesAudioProcessor : public AudioProcessor, public AudioProcessorValueTreeState::Listener  {
private:
    friend class ParticlesPluginEditor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticlesAudioProcessor)
    ParticleSynth poly;
    ParticleSimulation sim;
    int stepCounter = 0;
    int samplesPerStep = 64;
    float noteLength = 0.1f;
    std::map<int, int> lastChannelForNote;

    MidiBuffer overflowBuffer;

    AudioProcessorValueTreeState state {
        *this,
        nullptr,
        "ParticleSim", {
            param("particle_multiplier", 1.0f, 20.0f, 5.0f),
            param("gravity", 0.0f, 2.0f, 0.0f),
            param("attack_time", 0.001f, 0.1f, 0.01f),
            param("decay_half_life", 0.001f, 0.5f, 0.05f),
            param("master_volume", -12.0f, 3.0f, 0.0f)
        }
    };

    /** Shortcut for getting true (non-normalised) values out of a parameter tree */
    float getParameterValue(StringRef parameterName) const {
        auto param = state.getParameter(parameterName);
        return param->convertFrom0to1(param->getValue());
    }

public:
    ParticlesAudioProcessor():
        AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)) {
        state.addParameterListener("particle_multiplier", this);
        state.addParameterListener("gravity", this);
        state.addParameterListener("attack_time", &poly);
        state.addParameterListener("decay_half_life", &poly);
    }

    ~ParticlesAudioProcessor() override = default;

    void parameterChanged (const String& parameterID, float newValue) override {
        if (parameterID == "particle_multiplier") {
            sim.setParticleMultiplier(static_cast<int>(newValue));
        } else if (parameterID == "gravity") {
            sim.setGravity(newValue);
        }
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        poly.setCurrentPlaybackSampleRate(sampleRate);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {return (layouts.getMainOutputChannels() == 2);}

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midiInput) override {
        int maximumMidiFutureInSamples = 40000;

        MidiBuffer midiOutput;

        midiOutput.addEvents(overflowBuffer, 0, maximumMidiFutureInSamples, 0);
        overflowBuffer.clear();

        auto nextMidiEvent = midiInput.findNextSamplePosition(0);
        int noteLengthSamples = noteLength * getSampleRate();

        for (auto i = 0; i < audio.getNumSamples(); i++) {
            while (nextMidiEvent != midiInput.end() && (*nextMidiEvent).samplePosition <= i) {
                const auto &event = (*nextMidiEvent);
                if (event.getMessage().isNoteOn()) {
                    sim.addNote(event.getMessage().getNoteNumber(), event.getMessage().getFloatVelocity());
                } else if (event.getMessage().isNoteOff()) {
                    sim.removeNote(event.getMessage().getNoteNumber());
                }
                nextMidiEvent++;
            }
            if (stepCounter++ >= samplesPerStep) {
                sim.step([&] (int midiNote, float velocity, float pan) {

                    lastChannelForNote[midiNote] = (lastChannelForNote[midiNote] + 1) % 16;

                    int ch = lastChannelForNote[midiNote] + 1;

                    midiOutput.addEvent(MidiMessage::controllerEvent(ch,10, static_cast<int>((pan + 1.0f)*64.0f)), i);
                    midiOutput.addEvent(MidiMessage::noteOn(ch, midiNote,velocity), i);
                    midiOutput.addEvent(MidiMessage::noteOff(ch, midiNote), i + noteLengthSamples);
                }, samplesPerStep/256.0f);
                stepCounter = 0;
            }
        }
        audio.clear();

        MidiBuffer blockOut;
        blockOut.addEvents(midiOutput, 0, audio.getNumSamples(), 0);
        overflowBuffer.addEvents(midiOutput, audio.getNumSamples(), maximumMidiFutureInSamples, -audio.getNumSamples());

        poly.renderNextBlock(audio, blockOut, 0,audio.getNumSamples());

        audio.applyGain(pow(10, getParameterValue("master_volume")/10));

        midiInput.clear();
        midiInput.addEvents(blockOut, 0, audio.getNumSamples(), 0);
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
        generateUI(proc.state, {"particle_multiplier", "gravity", "attack_time", "decay_half_life", "master_volume"});

        setSize(1000,1000);
        vis.setBounds(200,200,600,600);
        addAndMakeVisible(vis);

    }

    void generateUI(AudioProcessorValueTreeState& state, const std::initializer_list<String> parameters) {
        auto x = 0;
        auto cw = 100;
        auto ch = 200;
        for (auto &param: parameters) {
            auto control = std::make_unique<ParamControl>(*state.getParameter(param));
            addAndMakeVisible(control->slider);
            control->slider.setBounds(x,0,cw,ch-20);
            control->slider.setSliderStyle(Slider::LinearVertical);
            control->slider.setTextBoxStyle(Slider::TextBoxBelow, true, 50,20);
            control->label.setText(param,NotificationType::dontSendNotification);
            control->label.setJustificationType(Justification::centred);
            control->label.setBounds(x, ch-20,cw,20);
            addAndMakeVisible(control->slider);
            addAndMakeVisible(control->label);
            controls.push_back(std::move(control));
            x+=cw;
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




