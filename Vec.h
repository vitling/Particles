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

double operator % (const Vec& a, const Vec &b) {
    return a.x * b.x + a.y * b.y;
}

Vec operator * (double s, const Vec &v) {
    return {s * v.x, s * v.y};
}

double abs(const Vec &v) {
    return sqrt(v.x * v.x + v.y * v.y);
}

double dist(const Vec& a, const Vec& b) {
    return abs(a - b);
}
#endif //PARTICLES_PLUGIN_VEC_H
