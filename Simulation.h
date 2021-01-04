//
// Created by David Whiting on 2020-12-11.
//

#ifndef PARTICLES_PLUGIN_SIMULATION_H
#define PARTICLES_PLUGIN_SIMULATION_H
#include <JuceHeader.h>
#include "Vec.h"

constexpr int MAX_PARTICLES = 200;

struct Particle {
    Vec pos = {0,0};
    Vec vel = {0,0};
    double mass = 1.0;
    double hue = 0.0;
    double radius = 1.0;
    float lastCollided = 1000;
    int note = 32;
    bool enabled = false;
};

class ParticleSimulation {
private:
    friend class ParticleSimulationVisualiser;

    const int w = 1000;
    const int h = 1000;
    Particle particles[MAX_PARTICLES];
    Random rnd;

    float gravity = 0.0f;

    int particleGenerationMultiplier = 1;

    const int scale[5] = {0,2,3,7,10};

    int createNote() {
        return scale[rnd.nextInt(5)] + rnd.nextInt(4) * 12;
    }

    int findFreeParticle() {
        for (auto i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].enabled == false) return i;
        }
        return -1;
    }

    void createParticle(int noteNumber, float velocity) {
        int freeParticle = findFreeParticle();
        if (freeParticle != -1) {
            Particle &p = particles[freeParticle];
            p.pos = {rnd.nextFloat() * 200, rnd.nextFloat() * 200};
            p.vel = 4 * velocity * normalise({rnd.nextFloat(), rnd.nextFloat()});
            p.note = noteNumber;
            p.mass = 100000.0 / (110.0 * (pow(2.0, (p.note / 12.0))));
            p.hue = 30 + 360.0 * (p.note % 12) / 12.0;
            p.radius = sqrt(p.mass) * 4;
            p.lastCollided = 1000;
            p.enabled = true;
        }
    }
public:
    explicit ParticleSimulation() {}

    void addNote(int noteNumber, float velocity) {
        for (auto i = 0; i < particleGenerationMultiplier; i++) {
            createParticle(noteNumber, velocity);
        }
    }

    void setParticleMultiplier(int newValue) {
        particleGenerationMultiplier = newValue;
    }

    void removeNote(int noteNumber) {
        for (auto i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].enabled && particles[i].note == noteNumber) {
                particles[i].enabled = false;
            }
        }
    }

    inline float clamp(float value) {
        return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    }

    void setGravity(float newGravity) {
        gravity = newGravity;
    }


    void step(const std::function<void (int, float, float)> &collisionCallback, float fraction = 1.0f) {
//        if (targetParticles > nParticles) {
//            //createParticle();
//        }
//        if (targetParticles < nParticles) {
//            //removeParticle();
//        }
        for (auto i = 0 ; i < MAX_PARTICLES; i++) {
            Particle &p = particles[i];
            if (p.enabled) {
                p.pos += fraction * p.vel;
                p.vel.y = p.vel.y + 0.05 * gravity;
                if (p.pos.x < 0) p.vel.x = abs(p.vel.x);
                if (p.pos.y < 0) p.vel.y = abs(p.vel.y);
                if (p.pos.x > w) p.vel.x = -abs(p.vel.x);
                if (p.pos.y > h) p.vel.y = -abs(p.vel.y);
                p.lastCollided += fraction;
            }
        }
        for (auto i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].enabled) continue;
            for (auto j = 0; j < MAX_PARTICLES; j++) {
                if (i == j || !particles[j].enabled) continue;
                Particle &a = particles[i];
                Particle &b = particles[j];
                // check if particles are intersection
                if (dist(a.pos,b.pos) < (a.radius + b.radius)) {
                    // check to make sure they're not already moving away from each other (helps with glitches)
                    if (dist(a.pos, b.pos) > dist(a.pos + a.vel, b.pos + b.vel)) {
                        double massA = 2 * b.mass / (a.mass + b.mass);
                        double massB = 2 * a.mass / (a.mass + b.mass);
                        Vec dif = a.pos - b.pos;
                        double normalisedDotProduct = ((a.vel - b.vel) % dif) / (pow(abs(dif),2));

                        Vec aNewVel = a.vel - massA * normalisedDotProduct * dif;
                        Vec bNewVel = b.vel + massB * normalisedDotProduct * dif;

                        a.vel = aNewVel;
                        b.vel = bNewVel;

                        collisionCallback(a.note + 33, clamp(abs(a.vel)/10), a.pos.x/500.0f - 1.0f);
                        collisionCallback(b.note + 33, clamp(abs(b.vel)/10), b.pos.x/500.0f - 1.0f);

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
        for (const auto & p : sim.particles) {
            if (p.enabled) {
                if (p.lastCollided < 20) {
                    g.setColour(Colour::fromHSL(p.hue/360.0f, 1.0f, (20.0f - p.lastCollided)/20.0f, 1.0f));
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
#endif //PARTICLES_PLUGIN_SIMULATION_H
