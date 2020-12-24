//
// Created by David Whiting on 2020-12-11.
//

#ifndef PARTICLES_PLUGIN_SIMULATION_H
#define PARTICLES_PLUGIN_SIMULATION_H
#include <JuceHeader.h>
#include "Vec.h"

constexpr int MAX_PARTICLES = 400;

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

    const int w = 1000;
    const int h = 1000;
    Particle particles[MAX_PARTICLES];
    Random rnd;

    std::list<Particle*> activeParticles;
    std::list<Particle*> inactiveParticles;


    const int scale[5] = {0,2,3,7,10};

    int createNote() {
        return scale[rnd.nextInt(5)] + rnd.nextInt(4) * 12;
    }

    void createParticle(int noteNumber, float velocity) {
        if (!inactiveParticles.empty()) {
            Particle* next = inactiveParticles.back();
            inactiveParticles.pop_back();
            activeParticles.push_back(next);

            Particle &p = *next;
            p.pos = {rnd.nextFloat() * 1000, rnd.nextFloat() * 1000};
            p.vel = {4 * velocity * (rnd.nextFloat() - 0.5), 4 * velocity * (rnd.nextFloat() - 0.5)};
            p.note = noteNumber;
            p.mass = 100000.0 / (110.0 * (pow(2.0, (p.note / 12.0))));
            p.hue = 30 + 360.0 * (p.note % 12) / 12.0;
            p.radius = sqrt(p.mass);
            p.lastCollided = 1000;
            p.enabled = true;
        }
    }
public:
    explicit ParticleSimulation(int initialParticles) {
        for (int i = 0 ; i < MAX_PARTICLES; i++) {
            inactiveParticles.push_back(&particles[i]);
        }
    }

    static constexpr int PARTICLE_MULTIPLIER = 10;

    void addNote(int noteNumber, float velocity) {
        for (auto i = 0; i < PARTICLE_MULTIPLIER; i++) {
            createParticle(noteNumber, velocity);
        }
    }

    void removeNote(int noteNumber) {
        for (auto iterator = activeParticles.begin(); iterator != activeParticles.end(); iterator++) {
            Particle* particle = (*iterator);
            if (particle->note == noteNumber) {
                particles->enabled = false;
                inactiveParticles.push_back(particle);
                activeParticles.erase(iterator);
            }
        }
    }

    inline float clamp(float value) {
        return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    }

    void setParticles(int p) {
        //targetParticles = p;
    }

    void step(const std::function<void (int, float)> &collisionCallback) {
//        if (targetParticles > nParticles) {
//            //createParticle();
//        }
//        if (targetParticles < nParticles) {
//            //removeParticle();
//        }
        for (auto particle : activeParticles) {
            Particle &p = *particle;
            p.pos += p.vel;
            if (p.pos.x < 0) p.vel.x = abs(p.vel.x);
            if (p.pos.y < 0) p.vel.y = abs(p.vel.y);
            if (p.pos.x > w) p.vel.x = -abs(p.vel.x);
            if (p.pos.y > h) p.vel.y = -abs(p.vel.y);
            p.lastCollided++;
        }
        for (auto pa : activeParticles) {
            for (auto pb : activeParticles) {
                if (pa == pb) continue;
                Particle &a = *pa;
                Particle &b = *pb;
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
        for (auto part : sim.activeParticles) {
            const Particle &p = *part;
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
