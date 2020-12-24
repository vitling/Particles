//
// Created by David Whiting on 2020-12-12.
//

#ifndef PARTICLES_PLUGIN_PARTICLESYNTH_H
#define PARTICLES_PLUGIN_PARTICLESYNTH_H

#include <JuceHeader.h>

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
#endif //PARTICLES_PLUGIN_PARTICLESYNTH_H
