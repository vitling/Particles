//
// Created by David Whiting on 2020-12-11.
//

#ifndef PARTICLES_PLUGIN_VEC_H
#define PARTICLES_PLUGIN_VEC_H
#include <cmath>

struct Vec {
    double x;
    double y;
};

Vec operator + (const Vec& a, const Vec &b) {
    return {a.x + b.x, a.y + b.y};
}

void operator += (Vec& a, const Vec &b) {
    a.x += b.x;
    a.y += b.y;
}

Vec operator - (const Vec& a, const Vec &b) {
    return {a.x - b.x, a.y - b.y};
}

void operator -= (Vec& a, const Vec &b) {
    a.x -= b.x;
    a.y -= b.y;
}

// Dot product
double operator % (const Vec& a, const Vec &b) {
    return a.x * b.x + a.y * b.y;
}

Vec operator * (double s, const Vec &v) {
    return {s * v.x, s * v.y};
}

float abs(const Vec &v) {
    return float(sqrt(v.x * v.x + v.y * v.y));
}

Vec normalise (const Vec &v) {
    auto mag = abs(v);
    return {mag == 0.0 ? 0.0 : v.x / mag, mag == 0.0 ? 0.0 : v.y / mag};
}


double dist(const Vec& a, const Vec& b) {
    return abs(a - b);
}
#endif //PARTICLES_PLUGIN_VEC_H
