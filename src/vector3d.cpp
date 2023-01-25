// Copyright (c) 2022, arch1t3cht <arch1t3cht@gmail.com>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

/// @file vector3d.cpp
/// @brief 3D mathematical vector used in visual typesetting
/// @ingroup utility visual_ts
///

#include "vector3d.h"

#include "utils.h"

#include <libaegisub/format.h>

#include <cmath>
#include <limits>

Vector3D::Vector3D()
: x(std::numeric_limits<float>::min())
, y(std::numeric_limits<float>::min())
, z(std::numeric_limits<float>::min())
{
}

Vector3D operator *(float f, Vector3D v) {
	return Vector3D(v.X() * f, v.Y() * f, v.Z() * f);
}

Vector3D operator /(float f, Vector3D v) {
	return Vector3D(f / v.X(), f / v.Y(), f / v.Z());
}

Vector3D operator +(float f, Vector3D v) {
	return Vector3D(v.X() + f, v.Y() + f, v.Z() + f);
}

Vector3D operator -(float f, Vector3D v) {
	return Vector3D(f - v.X(), f - v.Y(), f - v.Z());
}

Vector3D Vector3D::Unit() const {
	float len = Len();
	if (len == 0)
		return Vector3D(0, 0, 0);
	return *this / len;
}

Vector3D Vector3D::RotateX(float angle) const {
	return Vector3D(x, y * cos(angle) - z * sin(angle), y * sin(angle) + z * cos(angle));
}

Vector3D Vector3D::RotateY(float angle) const {
	return Vector3D(x * cos(angle) - z * sin(angle), y, x * sin(angle) + z * cos(angle));
}

Vector3D Vector3D::RotateZ(float angle) const {
	return Vector3D(x * cos(angle) - y * sin(angle), x * sin(angle) + y * cos(angle), z);
}

Vector3D Vector3D::Max(Vector3D param) const {
	return Vector3D(std::max(x, param.x), std::max(y, param.y), std::max(z, param.z));
}

Vector3D Vector3D::Min(Vector3D param) const {
	return Vector3D(std::min(x, param.x), std::min(y, param.y), std::max(z, param.z));
}

Vector3D Vector3D::Round(float step) const {
	return Vector3D(floorf(x / step + .5f) * step, floorf(y / step + .5f) * step, floorf(z / step + .5f));
}

Vector3D::operator bool() const {
	return *this != Vector3D();
}

std::string Vector3D::PStr(char sep) const {
	return "(" + Str(sep) + ")";
}

std::string Vector3D::DStr(char sep) const {
	return agi::format("%d%c%d%c%d", (int)x, sep, (int)y, sep, (int)z);
}

std::string Vector3D::Str(char sep) const {
	return float_to_string(x,2) + sep + float_to_string(y,2) + sep + float_to_string(z, 2);
}
