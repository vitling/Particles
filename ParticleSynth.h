//
// Created by David Whiting on 2020-12-12.
//

#ifndef PARTICLES_PLUGIN_PARTICLESYNTH_H
#define PARTICLES_PLUGIN_PARTICLESYNTH_H

#include <JuceHeader.h>

constexpr float TAU = MathConstants<float>::twoPi;
constexpr int MAX_POLYPHONY = 64;

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
        float attack = 0.0f;
        float attackTime = 0.01f;
        float decayHalfLife = 0.05f;

        float nextPanValue = 0.0f;
        float currentPanValue = 0.0f;
    public:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParticleVoice)
        ParticleVoice() = default;
        virtual ~ParticleVoice() = default;
        bool canPlaySound(SynthesiserSound *sound) override { return true; }
        void startNote(int midiNoteNumber, float velocity, SynthesiserSound *sound, int currentPitchWheelPosition) override {
            Random rnd;
            currentPanValue = nextPanValue;
            osc.setSampleRate(getSampleRate());
            osc.setFrequency(MidiMessage::getMidiNoteInHertz(midiNoteNumber) * (0.995f + 0.01f * rnd.nextFloat()));
            osc.reset();
            attack = 0.0f;
            level = velocity;
        }

        void stopNote(float velocity, bool allowTailOff) override {
            if (!allowTailOff) {
                level = 0.0f;
                clearCurrentNote();
            }
        }
        void pitchWheelMoved(int newPitchWheelValue) override {}
        constexpr static int PAN_CC = 10;
        void controllerMoved(int controllerNumber, int newControllerValue) override {
            if (controllerNumber == PAN_CC) {
                nextPanValue = (newControllerValue / 64.0f) - 1.0f;
            }
        }

        static std::tuple<float,float> equalPower(float normalisedAngle) {
            const float factor = sqrt(2.0f)/2.0f;
            const float angleRadians = normalisedAngle * (TAU / 8.0f);
            return {
                factor * (cos(angleRadians) - sin(angleRadians)),
                factor * (cos(angleRadians) + sin(angleRadians))
            };
        }

        void renderNextBlock(AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override {
            auto [lAmp, rAmp] = equalPower(currentPanValue);
            auto sampleRate = getSampleRate();
            auto attackIncrement = 1.0f / (sampleRate * attackTime);
            auto decayFactor = pow(0.5f, 1.0f / (sampleRate * decayHalfLife));
            if (level >= 0.01f) {
                auto l = outputBuffer.getWritePointer(0);
                auto r = outputBuffer.getWritePointer(1);
                for (auto i = startSample; i < startSample + numSamples; i++) {
                    float sample = osc.next() * level * std::min(attack, 1.0f);
                    level *= decayFactor;
                    attack += attackIncrement;

                    l[i] += lAmp * sample * 0.2f;
                    r[i] += rAmp * sample * 0.2f;
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
