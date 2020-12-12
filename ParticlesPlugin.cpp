

#include <JuceHeader.h>
#include "Vec.h"
#include "Simulation.h"

constexpr float TAU = MathConstants<float>::twoPi;
constexpr int MAX_POLYPHONY = 32;

class ParticleSynth : public Synthesiser {
private:
    class SineOsc {
    private:
        float angle = 0.0f;
        float frequency = 440.0f;
        float currentSampleRate = 44100;
    public:
        void setSampleRate(float sampleRate) {
            currentSampleRate = sampleRate;
        }
        float next() {
            angle += (frequency / currentSampleRate) * TAU;
            if (angle > TAU) angle -= TAU;
            return sin(angle);
        }

        void reset() {
            angle = 0.0f;
        }

        void setFrequency(float freq) {
            frequency = freq;
        }
    };
    class ParticleSound: public SynthesiserSound {
        bool appliesToNote(int midiNoteNumber) override { return true; }
        bool appliesToChannel(int midiChannel) override { return true; }
    };
    class ParticleVoice: public SynthesiserVoice {
    private:
        SineOsc osc;
        float level = 0.0f;
    public:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParticleVoice)
        ParticleVoice() = default;
        virtual ~ParticleVoice() = default;
        bool canPlaySound(SynthesiserSound *sound) override { return true; }
        void startNote(int midiNoteNumber, float velocity, SynthesiserSound *sound, int currentPitchWheelPosition) override {
            osc.setSampleRate(getSampleRate());
            osc.setFrequency(MidiMessage::getMidiNoteInHertz(midiNoteNumber));
            osc.reset();
            level = velocity;
        }

        void stopNote(float velocity, bool allowTailOff) override {
            if (!allowTailOff) {
                level = 0.0f;
                clearCurrentNote();
            }
        }
        void pitchWheelMoved(int newPitchWheelValue) override {}
        void controllerMoved(int controllerNumber, int newControllerValue) override {}
        void renderNextBlock(AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override {
            if (level >= 0.01f) {
                auto l = outputBuffer.getWritePointer(0);
                auto r = outputBuffer.getWritePointer(1);
                for (auto i = startSample; i < startSample + numSamples; i++) {
                    float sample = osc.next() * level;
                    level *= 0.9996f;
                    l[i] += sample * 0.2f;
                    r[i] += sample * 0.2f;
                }
                if (level < 0.01f) {
                    level = 0.0f;
                    clearCurrentNote();
                }
            }
        }
    };
public:
    ParticleSynth() {
        addSound(new ParticleSound);
        for (auto i = 0; i < MAX_POLYPHONY; i++) {
            addVoice(new ParticleVoice());
        }
    }
};

class ParticlesPluginEditor;

class ParticlesAudioProcessor : public AudioProcessor, public AudioProcessorValueTreeState::Listener  {
private:
    friend class ParticlesPluginEditor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticlesAudioProcessor)
    ParticleSynth poly;
    ParticleSimulation sim;
    int stepCounter = 0;
    int samplesPerStep = 256;
    AudioProcessorValueTreeState state;

public:
    ParticlesAudioProcessor():
        AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
        sim(30),
        state(*this, nullptr, "parameter_state", {
                std::make_unique<AudioParameterFloat>("particles", "Particles", NormalisableRange<float>(1,MAX_PARTICLES,1.0), 30.0)
        }){

        state.addParameterListener("particles", this);

    }

    void parameterChanged(const String &parameterID, float newValue) override {

        if (parameterID == "particles") {
            sim.setParticles(static_cast<int>(newValue));
        }
    }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        poly.setCurrentPlaybackSampleRate(sampleRate);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {return (layouts.getMainOutputChannels() == 2);}

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midi) override {
        for (auto i = 0; i < audio.getNumSamples()-1; i++) {
            if (stepCounter++ >= samplesPerStep) {
                sim.step([&] (int midiNote, float velocity) { midi.addEvent(MidiMessage::noteOn(1,midiNote,velocity), i); midi.addEvent(MidiMessage::noteOff(1,midiNote), i+1); });
                stepCounter = 0;
            }
        }
        audio.clear();
        poly.renderNextBlock(audio,midi, 0,audio.getNumSamples());
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
//        auto stateToSave = state.copyState();
//        std::unique_ptr<XmlElement> xml (stateToSave.createXml());
//        copyXmlToBinary(*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override {
//        std::unique_ptr<XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
//        if (xmlState != nullptr) {
//            if (xmlState->hasTagName(state.state.getType())) {
//                state.replaceState(ValueTree::fromXml(*xmlState));
//            }
//        }
    }

    const ParticleSimulation & simulation() {
        return sim;
    }
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParticlesAudioProcessor();
}

class ParticlesPluginEditor: public AudioProcessorEditor {
private:
    ParticleSimulationVisualiser vis;
    Slider particleSlider;
    SliderParameterAttachment particleSliderAttach;
public:
    explicit ParticlesPluginEditor(ParticlesAudioProcessor &proc):
    AudioProcessorEditor(proc),
    vis(proc.simulation()),
    particleSliderAttach(*proc.state.getParameter("particles"), particleSlider) {
        setSize(1000,1000);
        vis.setBounds(200,200,600,600);
        particleSlider.setBounds(0,200,200,600);
        particleSlider.setSliderStyle(Slider::LinearVertical);
        particleSlider.setTextBoxStyle(Slider::TextBoxBelow, true, 50,20);
        addAndMakeVisible(vis);
        addAndMakeVisible(particleSlider);
    }

    void paint(Graphics &g) override {
        ColourGradient grad(Colour::fromRGB(210,115,20), 0,0, Colour::fromRGB(104,217,240), 0,1000,false);
        grad.addColour(0.5f, Colour::fromRGB(153,70,171));
        g.setGradientFill(grad);
        g.fillAll();
    }


};

AudioProcessorEditor * ParticlesAudioProcessor::createEditor() { return new ParticlesPluginEditor(*this); }



