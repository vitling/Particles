

#include <JuceHeader.h>
#include "Vec.h"

constexpr int MAX_PARTICLES = 100;

class ParticleSimulationVisualiser;

struct Particle {
    Vec pos = {0,0};
    Vec vel = {0,0};
    double mass = 1.0;
    double hue = 0.0;
    double radius = 1.0;
    int lastCollided = 1000;
    int note = 32;
    bool enabled = false;
};

class ParticleSimulation {
private:
    friend class ParticleSimulationVisualiser;

    int nParticles;
    const int w = 1000;
    const int h = 1000;
    Particle particles[MAX_PARTICLES];
    Random rnd;


    const int scale[5] = {0,2,3,7,10};

    int createNote() {
        return scale[rnd.nextInt(5)] + rnd.nextInt(4) * 12;
    }

    void createParticle() {
        if (nParticles < MAX_PARTICLES) {
            Particle &p = particles[nParticles];
            p.pos = {rnd.nextFloat() * 1000, rnd.nextFloat() * 1000};
            p.vel = {5 * (rnd.nextFloat() - 0.5),5 * (rnd.nextFloat() - 0.5)};
            p.note = createNote();
            p.mass = 100000.0 / (110.0 * (pow(2.0, (p.note/12.0))));
            p.hue = 30 + 360.0 * (p.note % 12) / 12.0;
            p.radius = sqrt(p.mass);
            p.lastCollided = 1000;
            p.enabled = true;
            nParticles++;
        }
    }

    void removeParticle() {
        if (nParticles > 0) {
            nParticles--;
            particles[nParticles].enabled = false;
        }
    }
public:
    explicit ParticleSimulation(int initialParticles): nParticles(0) {
        for (auto i = 0 ; i < initialParticles; i++) {
            createParticle();
        }
    }

    inline float clamp(float value) {
        return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    }

    void step(const std::function<void (int, float)> &collisionCallback) {
        for (auto i = 0 ; i < nParticles; i++) {
            Particle &p = particles[i];
            p.pos += p.vel;
            if (p.pos.x < 0) p.vel.x = abs(p.vel.x);
            if (p.pos.y < 0) p.vel.y = abs(p.vel.y);
            if (p.pos.x > w) p.vel.x = -abs(p.vel.x);
            if (p.pos.y > h) p.vel.y = -abs(p.vel.y);
            p.lastCollided++;
        }
        for (auto i = 0; i < nParticles; i++) {
            for (auto j = i + 1; j < nParticles; j++) {
                Particle &a = particles[i];
                Particle &b = particles[j];
                if (dist(a.pos,b.pos) < (a.radius + b.radius)) {
                    if (dist(a.pos, b.pos) > dist(a.pos + a.vel, b.pos + b.vel)) {
                        double massA = 2 * b.mass / (a.mass + b.mass);
                        double massB = 2 * a.mass / (a.mass + b.mass);
                        Vec dif = a.pos - b.pos;
                        double normalisedDotProduct = ((a.vel - b.vel) % dif) / (pow(abs(dif),2));

                        Vec aNewVel = a.vel - massA * normalisedDotProduct * dif;
                        Vec bNewVel = b.vel + massB * normalisedDotProduct * dif;

                        a.vel = aNewVel;
                        b.vel = bNewVel;

                        collisionCallback(a.note + 33, clamp(abs(a.vel)/10));
                        collisionCallback(b.note + 33, clamp(abs(b.vel)/10));

                        a.lastCollided = 0;
                        b.lastCollided = 0;
                    }
                }
            }
        }
    }
};

class ParticleSimulationVisualiser : public Component, private Timer {
private:
    const ParticleSimulation &sim;
public:
    ParticleSimulationVisualiser(const ParticleSimulation &sim): sim(sim) {
        startTimerHz(60);
    }

    void timerCallback() override {
        repaint();
    }

    void paint(Graphics &g) override {
        g.fillAll(Colours::white.withAlpha(0.5f));
        for (int i = 0; i < sim.nParticles; i++) {
            const Particle &p = sim.particles[i];
            if (p.enabled) {
                if (p.lastCollided < 10) {
                    g.setColour(Colour::fromHSL(p.hue/360.0f, 1.0f, 0.1f * (10.0f - p.lastCollided), 1.0f));
                } else {
                    g.setColour(Colours::black.withAlpha(0.5f));
                }
                float x = p.pos.x * (getWidth()/1000.0);
                float y = p.pos.y * (getHeight()/1000.0);
                float rx = p.radius * (getWidth()/1000.0);
                float ry = p.radius * (getHeight()/1000.0);
                g.fillEllipse(x-rx, y-ry, rx *2, ry * 2);
            }
        }
    }
};

class ParticlesAudioProcessor;



constexpr float TAU = MathConstants<float>::twoPi;

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

constexpr int MAX_POLYPHONY = 16;

class ParticleSynth : public Synthesiser {
private:
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

class ParticlesAudioProcessor : public AudioProcessor {
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticlesAudioProcessor)
    ParticleSynth poly;
    ParticleSimulation sim;
    int stepCounter = 0;
    int samplesPerStep = 512;
public:
    ParticlesAudioProcessor():
        AudioProcessor(BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)), sim(60) {
    }
    //void parameterChanged(const String &parameterID, float newValue) override {}
    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        poly.setCurrentPlaybackSampleRate(sampleRate);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {return (layouts.getMainOutputChannels() == 2);}

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midi) override {
        for (auto i = 0; i < audio.getNumSamples()-1; i++) {
            if (stepCounter++ % samplesPerStep == 0) {
                sim.step([&] (int midiNote, float velocity) { midi.addEvent(MidiMessage::noteOn(1,midiNote,velocity), i); midi.addEvent(MidiMessage::noteOff(1,midiNote), i+1); });
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
public:
    ParticlesPluginEditor(ParticlesAudioProcessor &proc): AudioProcessorEditor(proc), vis(proc.simulation()) {
        setSize(1000,1000);
        vis.setBounds(200,200,600,600);
        addAndMakeVisible(vis);
    }

    void paint(Graphics &g) override {
        ColourGradient grad(Colour::fromRGB(210,115,20), 0,0, Colour::fromRGB(104,217,240), 0,1000,false);
        //grad.addColour(0.0f, Colour::fromRGB(210,115,20));
        grad.addColour(0.5f, Colour::fromRGB(153,70,171));
        //grad.addColour(1.0f, Colour::fromRGB(104,217,240));
        g.setGradientFill(grad);
        g.fillAll();
    }


};

AudioProcessorEditor * ParticlesAudioProcessor::createEditor() { return new ParticlesPluginEditor(*this); }



