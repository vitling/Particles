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

        // It is unclear from the JUCE documentation whether it's possible this could happen concurrently with an audio
        // buffer filling operation. If so, the simulation should be locked while it is rendered, but I didn't want to
        // add it pre-emptively because it could slow things down a little

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
