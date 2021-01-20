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

#ifndef PARTICLES_PLUGIN_PARTICLESYNTH_H
#define PARTICLES_PLUGIN_PARTICLESYNTH_H

#include <JuceHeader.h>

class ParticleSynth : public Synthesiser, public AudioProcessorValueTreeState::Listener {
private:
    static constexpr float TAU = MathConstants<float>::twoPi;
    static constexpr int MAX_POLYPHONY = 128;

    struct VoiceParams {
        float attackTime = 0.01f;
        float decayHalfLife = 0.05f;
        float waveform = 0.0f;
    };

    class OscCycler {
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
            return angle;
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
        OscCycler cycler;
        float level = 0.0f;
        float attack = 0.0f;

        float nextPanValue = 0.0f;
        float currentPanValue = 0.0f;

        const VoiceParams& voiceParams;
    public:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParticleVoice)
        ParticleVoice(const VoiceParams& voiceParams): voiceParams(voiceParams) { };

        virtual ~ParticleVoice() = default;
        bool canPlaySound(SynthesiserSound *sound) override { return true; }
        void startNote(int midiNoteNumber, float velocity, SynthesiserSound *sound, int currentPitchWheelPosition) override {
            Random rnd;
            currentPanValue = nextPanValue;
            cycler.setSampleRate(getSampleRate());
            cycler.setFrequency(MidiMessage::getMidiNoteInHertz(midiNoteNumber) * (0.995f + 0.01f * rnd.nextFloat()));
            cycler.reset();
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

        constexpr static inline float saw(float a) {
            return 2.0f * (a / TAU) - 1.0f;
        }

        inline float oscFunction(float a) const {
            return (1.0f - voiceParams.waveform) * sin(a) + voiceParams.waveform * saw(a);
        }

        void renderNextBlock(AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override {
            auto [lAmp, rAmp] = equalPower(currentPanValue);
            auto sampleRate = getSampleRate();
            auto attackIncrement = 1.0f / (sampleRate * voiceParams.attackTime);
            auto decayFactor = pow(0.5f, 1.0f / (sampleRate * voiceParams.decayHalfLife));
            if (level > 0.00f) {
                auto l = outputBuffer.getWritePointer(0);
                auto r = outputBuffer.getWritePointer(1);
                for (auto i = startSample; i < startSample + numSamples; i++) {
                    float sample = oscFunction(cycler.next()) * level * std::min(attack, 1.0f);
                    level *= decayFactor;
                    attack += attackIncrement;

                    l[i] += lAmp * sample * 0.2f;
                    r[i] += rAmp * sample * 0.2f;
                }
                if (level < 0.01f) {
                    level -= 0.001f;
                }
                if (level < 0.0f) {
                    level = 0.0f;
                    clearCurrentNote();
                }
            }
        }
    };

    VoiceParams params;
public:
    ParticleSynth() {
        addSound(new ParticleSound);
        for (auto i = 0; i < MAX_POLYPHONY; i++) {
            auto newVoice = new ParticleVoice(params);
            addVoice(newVoice);
        }
    }

    void parameterChanged (const String& parameterID, float newValue) override {
        if (parameterID == "attack_time") {
            params.attackTime = newValue;
        } else if (parameterID == "decay_half_life") {
            params.decayHalfLife = newValue;
        } else if (parameterID == "waveform") {
            params.waveform = newValue;
        }
    }
};
#endif //PARTICLES_PLUGIN_PARTICLESYNTH_H
