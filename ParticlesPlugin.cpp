

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
    int samplesPerStep = 256;

    AudioProcessorValueTreeState state {
        *this,
        nullptr,
        "ParticleSim", {
            param("particles", 1.0f, MAX_PARTICLES, 30.0f)
        }
    };

public:
    ParticlesAudioProcessor():
        AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
        sim(30) {
        state.addParameterListener("particles", this);
    }

    ~ParticlesAudioProcessor() override = default;

    void parameterChanged (const String& parameterID, float newValue) override {
        if (parameterID == "particles") {
            sim.setParticles(static_cast<int>(newValue));
        }
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        poly.setCurrentPlaybackSampleRate(sampleRate);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {return (layouts.getMainOutputChannels() == 2);}

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midiInput) override {
        MidiBuffer midiOutput;

        auto nextMidiEvent = midiInput.findNextSamplePosition(0);

        for (auto i = 0; i < audio.getNumSamples()-1; i++) {
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
                sim.step([&] (int midiNote, float velocity) { midiOutput.addEvent(MidiMessage::noteOn(1,midiNote,velocity), i); midiOutput.addEvent(MidiMessage::noteOff(1,midiNote), i+1); });
                stepCounter = 0;
            }
        }
        audio.clear();
        poly.renderNextBlock(audio,midiOutput, 0,audio.getNumSamples());
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
        auto particleSlider = std::make_unique<ParamControl>(*proc.state.getParameter("particles"));
        addAndMakeVisible(particleSlider->slider);
        particleSlider->slider.setBounds(0,200,200,600);
        particleSlider->slider.setSliderStyle(Slider::LinearVertical);
        particleSlider->slider.setTextBoxStyle(Slider::TextBoxBelow, true, 50, 20);
        controls.push_back(std::move(particleSlider));

        setSize(1000,1000);
        vis.setBounds(200,200,600,600);
        addAndMakeVisible(vis);

    }

    void paint(Graphics &g) override {
        ColourGradient grad(Colour::fromRGB(210,115,20), 0,0, Colour::fromRGB(104,217,240), 0,1000,false);
        grad.addColour(0.5f, Colour::fromRGB(153,70,171));
        g.setGradientFill(grad);
        g.fillAll();
    }
};

AudioProcessorEditor * ParticlesAudioProcessor::createEditor() { return new ParticlesPluginEditor(*this); }




