// Copyright (c) 2022,  arch1t3cht <arch1t3cht@gmail.com>
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

/// @file vector3d.h
/// @see vector3d.cpp
/// @ingroup utility visual_ts
///

#pragma once

#include <cmath>
#include <string>
#include "vector2d.h"

class Vector3D {
	float x, y, z;

public:
	float X() const { return x; }
	float Y() const { return y; }
	float Z() const { return z; }
	Vector2D XY() const { return Vector2D(x, y); }

	Vector3D();
	Vector3D(Vector2D xy) : x(xy.X()), y(xy.Y()), z(0.) { }
	Vector3D(Vector2D xy, float z) : x(xy.X()), y(xy.Y()), z(z) { }
	Vector3D(float x, float y, float z) : x(x), y(y), z(z) { }

	bool operator ==(const Vector3D r) const { return x == r.x && y == r.y; }
	bool operator !=(const Vector3D r) const { return x != r.x || y != r.y; }
	explicit operator bool() const;

	Vector3D operator -() const { return Vector3D(-x, -y, -z); }
	Vector3D operator +(const Vector3D r) const { return Vector3D(x + r.x, y + r.y, z + r.z); }
	Vector3D operator -(const Vector3D r) const { return Vector3D(x - r.x, y - r.y, z - r.z); }
	Vector3D operator *(const Vector3D r) const { return Vector3D(x * r.x, y * r.y, z * r.z); }
	Vector3D operator /(const Vector3D r) const { return Vector3D(x / r.x, y / r.y, z / r.z); }
	Vector3D operator +(float param) const { return Vector3D(x + param, y + param, z + param); }
	Vector3D operator -(float param) const { return Vector3D(x - param, y - param, z - param); }
	Vector3D operator *(float param) const { return Vector3D(x * param, y * param, z * param); }
	Vector3D operator /(float param) const { return Vector3D(x / param, y / param, z / param); }

	Vector3D Unit() const;

	Vector3D RotateX(float angle) const;
	Vector3D RotateY(float angle) const;
	Vector3D RotateZ(float angle) const;

	Vector3D Max(Vector3D param) const;
	Vector3D Min(Vector3D param) const;
	Vector3D Round(float step) const;

	Vector3D Cross(const Vector3D param) const { return Vector3D(y * param.z - z * param.y, z * param.x - x * param.z, x * param.y - y * param.x); }
	float Dot(const Vector3D param) const { return x * param.x + y * param.y + z * param.z; }

	float Len() const { return sqrt(x*x + y*y + z*z); }
	float SquareLen() const { return x*x + y*y + z*z; }

	/// Get as string with given separator
	std::string Str(char sep = ',') const;
	/// Get as string surrounded by parentheses with given separator
	std::string PStr(char sep = ',') const;
	/// Get as string with given separator with values rounded to ints
	std::string DStr(char sep = ',') const;
};

Vector3D operator * (float f, Vector3D v);
Vector3D operator / (float f, Vector3D v);
Vector3D operator + (float f, Vector3D v);
Vector3D operator - (float f, Vector3D v);
