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

#ifndef PARTICLES_PLUGIN_PARTICLESIMULATION_H
#define PARTICLES_PLUGIN_PARTICLESIMULATION_H
#include <JuceHeader.h>
#include "Vec.h"

// I'm aware that the way I do this is sort of a manual implementation of polymorphism; but given this is a simple
// function substitution in a CPU-heavy simulation, I decided to keep it as a simple enum switch instead of what would
// resolve to a virtual function table lookup
enum class ParticleOrigin {
    TOP_LEFT,
    RANDOM_INSIDE,
    RANDOM_OUTSIDE,
    TOP_RANDOM
};

class ParticleSimulation {
private:
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

    static constexpr int MAX_PARTICLES = 200;

    friend class ParticleSimulationVisualiser;

    const float w = 1000;
    const float h = 1000;

    // We do not "add" or "remove" particles, but keep them all initialized and flag them as 'enabled' or not
    Particle particles[MAX_PARTICLES];
    Random rnd;

    float gravity = 0.0f;

    int particleGenerationMultiplier = 5;

    ParticleOrigin particleOrigin = ParticleOrigin::RANDOM_INSIDE;

    bool sizeByNote = true;
    float particleScale = 1.0f;

    // Find the first particle in the array with 'enabled' set to false
    int findFreeParticle() {
        for (auto i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].enabled) return i;
        }
        return -1;
    }

    void generateTopLeft(Particle &p, float velocity) {
        p.pos = {rnd.nextFloat() * 200, rnd.nextFloat() * 200};
        p.vel = 4 * velocity * normalise({rnd.nextFloat(), rnd.nextFloat()});
    }

    void generateRandomInside(Particle &p, float velocity) {
        p.pos = {rnd.nextFloat() * w, rnd.nextFloat() * h};
        p.vel = 4 * velocity * normalise({rnd.nextFloat() - 0.5f, rnd.nextFloat() - 0.5f});
    }

    void generateRandomOutside(Particle &p, float velocity) {
        if (rnd.nextBool()) {
            p.pos = {
                    rnd.nextFloat() * 100 + (rnd.nextBool() ? -100.0f : w),
                    rnd.nextFloat() * h
            };
        } else {
            p.pos = {
                    rnd.nextFloat() * w,
                    rnd.nextFloat() * 100+ (rnd.nextBool() ? -100.0f : h)
            };
        }
        p.vel = 4 * velocity * normalise({rnd.nextFloat() - 0.5f, rnd.nextFloat() - 0.5f});
    }

    void generateTopRandom(Particle &p, float velocity) {
        p.pos = {rnd.nextFloat() * w, - rnd.nextFloat() * 100};
        p.vel = 4 * velocity * normalise({rnd.nextFloat() - 0.5f, rnd.nextFloat()});
    }

    void setParticleProperties(Particle &p, int noteNumber) {
        p.note = noteNumber;
        p.mass = sizeByNote ? particleScale * 100000.0 / (110.0 * (pow(2.0, (p.note / 12.0)))) : 300.0f * particleScale;
        p.hue = 30 + 360.0 * (p.note % 12) / 12.0;
        p.radius =  sqrt(p.mass) * 4;
        p.lastCollided = 1000;
        p.enabled = true;
    }

    void setupParticle(Particle &p, int noteNumber, float velocity) {
        setParticleProperties(p, noteNumber);
        switch (particleOrigin) {
            case ParticleOrigin::TOP_LEFT:
                generateTopLeft(p, velocity);
                break;
            case ParticleOrigin::RANDOM_INSIDE:
                generateRandomInside(p, velocity);
                break;
            case ParticleOrigin::RANDOM_OUTSIDE:
                generateRandomOutside(p, velocity);
                break;
            case ParticleOrigin::TOP_RANDOM:
                generateTopRandom(p, velocity);
                break;
        }
    }

    void createParticle(int noteNumber, float velocity) {
        int freeParticle = findFreeParticle();
        if (freeParticle != -1) {
            setupParticle(particles[freeParticle], noteNumber, velocity);
        }
    }

    static inline float clamp(float value) {
        return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
    }

public:
    explicit ParticleSimulation() {}

    void addNote(int noteNumber, float velocity) {
        for (auto i = 0; i < particleGenerationMultiplier; i++) {
            createParticle(noteNumber, velocity);
        }
    }

    void removeNote(int noteNumber) {
        for (auto & particle : particles) {
            if (particle.enabled && particle.note == noteNumber) {
                particle.enabled = false;
            }
        }
    }

    void setParticleMultiplier(int newValue) {
        particleGenerationMultiplier = newValue;
    }

    void setGravity(float newGravity) {
        gravity = newGravity;
    }

    void setParticleOrigin(ParticleOrigin genRule) {
        particleOrigin = genRule;
    }

    void setSizeByNote(bool changeSizeByNote) {
        sizeByNote = changeSizeByNote;
    }

    void setScale(float scale) {
        particleScale = scale;
    }


    // Step the simulation. The callback takes a midi note, a clamped velocity and a pan value
    void step(const std::function<void (int, float, float)> &collisionCallback, float timeScale = 1.0f) {
        for (auto & p : particles) {
            if (p.enabled) {
                p.pos += timeScale * p.vel;
                p.vel.y = p.vel.y + 0.05 * gravity;
                if (p.pos.x < 0) p.vel.x = abs(p.vel.x);
                if (p.pos.y < 0) p.vel.y = abs(p.vel.y);
                if (p.pos.x > w) p.vel.x = -abs(p.vel.x);
                if (p.pos.y > h) p.vel.y = -abs(p.vel.y);
                p.lastCollided += timeScale;
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


#endif //PARTICLES_PLUGIN_PARTICLESIMULATION_H
