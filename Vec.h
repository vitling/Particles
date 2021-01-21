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

#ifndef PARTICLES_PLUGIN_VEC_H
#define PARTICLES_PLUGIN_VEC_H
#include <cmath>

/** Simple 2d vector representation with various overloaded operations
 *  I'm sure I could have found something in a library to do this, but it's also a pretty simple subset that I need
 *  for this plugin, so I wrote it myself to remove the need for additional dependencies
 */
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

/** Dot product */
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
