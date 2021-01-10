//
// Created by David Whiting on 2021-01-10.
//

#ifndef PARTICLES_PLUGIN_PARTICLESIMULATIONVISUALISER_H
#define PARTICLES_PLUGIN_PARTICLESIMULATIONVISUALISER_H

#include <JuceHeader.h>
#include "ParticleSimulation.h"

class ParticleSimulationVisualiser : public Component, private Timer {
private:
    const ParticleSimulation &sim;
    const StringArray noteNames = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
public:
    ParticleSimulationVisualiser(const ParticleSimulation &sim): sim(sim) {
        startTimerHz(60);
    }

    void timerCallback() override {
        repaint();
    }

    String getNoteName(int note) {
        int octave = (note / 12) - 1;
        int positionInOctave = note % 12;
        String noteName = noteNames[positionInOctave];
        noteName += octave;
        return noteName;
    }

    void paint(Graphics &g) override {
        g.fillAll(Colours::white.withAlpha(0.5f));
        g.setFont(g.getCurrentFont().withHeight(8));
        for (const auto & particle : sim.particles) {
            if (particle.enabled) {
                if (particle.lastCollided < 20) {
                    g.setColour(Colour::fromHSL(particle.hue / 360.0f, 1.0f, (20.0f - particle.lastCollided) / 20.0f, 1.0f));
                } else {
                    g.setColour(Colours::black.withAlpha(0.5f));
                }
                float x = particle.pos.x * (getWidth() / 1000.0);
                float y = particle.pos.y * (getHeight() / 1000.0);
                float rx = particle.radius * (getWidth() / 1000.0);
                float ry = particle.radius * (getHeight() / 1000.0);
                g.fillEllipse(x-rx, y-ry, rx *2, ry * 2);
                g.setColour(Colours::white);
                g.drawText(getNoteName(particle.note), int(x - rx), int(y - ry), int(rx * 2), int(ry * 2), Justification::centred, false);
            }
        }
    }
};

#endif //PARTICLES_PLUGIN_PARTICLESIMULATIONVISUALISER_H
