/**************************************************************************/
/*  quaternion.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/math/math_funcs.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"

struct [[nodiscard]] Quaternion {
	union {
		// NOLINTBEGIN(modernize-use-default-member-init)
		struct {
			real_t x;
			real_t y;
			real_t z;
			real_t w;
		};
		real_t components[4] = { 0, 0, 0, 1.0 };
		// NOLINTEND(modernize-use-default-member-init)
	};

	_FORCE_INLINE_ real_t &operator[](int p_idx) {
		return components[p_idx];
	}
	_FORCE_INLINE_ const real_t &operator[](int p_idx) const {
		return components[p_idx];
	}
	_FORCE_INLINE_ real_t length_squared() const;
	bool is_equal_approx(const Quaternion &p_quaternion) const;
	bool is_same(const Quaternion &p_quaternion) const;
	bool is_finite() const;
	real_t length() const;
	void normalize();
	Quaternion normalized() const;
	bool is_normalized() const;
	Quaternion inverse() const;
	Quaternion log() const;
	Quaternion exp() const;
	_FORCE_INLINE_ real_t dot(const Quaternion &p_q) const;
	real_t angle_to(const Quaternion &p_to) const;

	Vector3 get_euler(EulerOrder p_order = EulerOrder::YXZ) const;
	static Quaternion from_euler(const Vector3 &p_euler);

	Quaternion slerp(const Quaternion &p_to, real_t p_weight) const;
	Quaternion slerpni(const Quaternion &p_to, real_t p_weight) const;
	Quaternion spherical_cubic_interpolate(const Quaternion &p_b, const Quaternion &p_pre_a, const Quaternion &p_post_b, real_t p_weight) const;
	Quaternion spherical_cubic_interpolate_in_time(const Quaternion &p_b, const Quaternion &p_pre_a, const Quaternion &p_post_b, real_t p_weight, real_t p_b_t, real_t p_pre_a_t, real_t p_post_b_t) const;

	Vector3 get_axis() const;
	real_t get_angle() const;

	_FORCE_INLINE_ void get_axis_angle(Vector3 &r_axis, real_t &r_angle) const {
		r_angle = 2 * Math::acos(w);
		real_t r = ((real_t)1) / Math::sqrt(1 - w * w);
		r_axis.x = x * r;
		r_axis.y = y * r;
		r_axis.z = z * r;
	}

	constexpr void operator*=(const Quaternion &p_q);
	constexpr Quaternion operator*(const Quaternion &p_q) const;

	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_v) const {
#ifdef MATH_CHECKS
		ERR_FAIL_COND_V_MSG(!is_normalized(), p_v, "The quaternion " + operator String() + " must be normalized.");
#endif
		Vector3 u(x, y, z);
		Vector3 uv = u.cross(p_v);
		return p_v + ((uv * w) + u.cross(uv)) * ((real_t)2);
	}

	_FORCE_INLINE_ Vector3 xform_inv(const Vector3 &p_v) const {
		return inverse().xform(p_v);
	}

	constexpr void operator+=(const Quaternion &p_q);
	constexpr void operator-=(const Quaternion &p_q);
	constexpr void operator*=(real_t p_s);
	constexpr void operator/=(real_t p_s);
	constexpr Quaternion operator+(const Quaternion &p_q2) const;
	constexpr Quaternion operator-(const Quaternion &p_q2) const;
	constexpr Quaternion operator-() const;
	constexpr Quaternion operator*(real_t p_s) const;
	constexpr Quaternion operator/(real_t p_s) const;

	constexpr bool operator==(const Quaternion &p_quaternion) const;
	constexpr bool operator!=(const Quaternion &p_quaternion) const;

	explicit operator String() const;

	constexpr Quaternion() :
			x(0), y(0), z(0), w(1) {}

	constexpr Quaternion(real_t p_x, real_t p_y, real_t p_z, real_t p_w) :
			x(p_x), y(p_y), z(p_z), w(p_w) {}

	Quaternion(const Vector3 &p_axis, real_t p_angle);

	constexpr Quaternion(const Quaternion &p_q) :
			x(p_q.x), y(p_q.y), z(p_q.z), w(p_q.w) {}

	constexpr void operator=(const Quaternion &p_q) {
		x = p_q.x;
		y = p_q.y;
		z = p_q.z;
		w = p_q.w;
	}

	Quaternion(const Vector3 &p_v0, const Vector3 &p_v1) { // Shortest arc.
#ifdef MATH_CHECKS
		ERR_FAIL_COND_MSG(p_v0.is_zero_approx() || p_v1.is_zero_approx(), "The vectors must not be zero.");
#endif
#ifdef REAL_T_IS_DOUBLE
		constexpr real_t ALMOST_ONE = 0.999999999999999;
#else
		constexpr real_t ALMOST_ONE = 0.99999975f;
#endif
		Vector3 n0 = p_v0.normalized();
		Vector3 n1 = p_v1.normalized();
		real_t d = n0.dot(n1);
		if (Math::abs(d) > ALMOST_ONE) {
			if (d >= 0) {
				return; // Vectors are same.
			}
			Vector3 axis = n0.get_any_perpendicular();
			x = axis.x;
			y = axis.y;
			z = axis.z;
			w = 0;
		} else {
			Vector3 c = n0.cross(n1);
			real_t s = Math::sqrt((1.0f + d) * 2.0f);
			real_t rs = 1.0f / s;

			x = c.x * rs;
			y = c.y * rs;
			z = c.z * rs;
			w = s * 0.5f;
		}
		normalize();
	}
};

real_t Quaternion::dot(const Quaternion &p_q) const {
	return x * p_q.x + y * p_q.y + z * p_q.z + w * p_q.w;
}

real_t Quaternion::length_squared() const {
	return dot(*this);
}

constexpr void Quaternion::operator+=(const Quaternion &p_q) {
	x += p_q.x;
	y += p_q.y;
	z += p_q.z;
	w += p_q.w;
}

constexpr void Quaternion::operator-=(const Quaternion &p_q) {
	x -= p_q.x;
	y -= p_q.y;
	z -= p_q.z;
	w -= p_q.w;
}

constexpr void Quaternion::operator*=(real_t p_s) {
	x *= p_s;
	y *= p_s;
	z *= p_s;
	w *= p_s;
}

constexpr void Quaternion::operator/=(real_t p_s) {
	*this *= (1 / p_s);
}

constexpr Quaternion Quaternion::operator+(const Quaternion &p_q2) const {
	const Quaternion &q1 = *this;
	return Quaternion(q1.x + p_q2.x, q1.y + p_q2.y, q1.z + p_q2.z, q1.w + p_q2.w);
}

constexpr Quaternion Quaternion::operator-(const Quaternion &p_q2) const {
	const Quaternion &q1 = *this;
	return Quaternion(q1.x - p_q2.x, q1.y - p_q2.y, q1.z - p_q2.z, q1.w - p_q2.w);
}

constexpr Quaternion Quaternion::operator-() const {
	const Quaternion &q2 = *this;
	return Quaternion(-q2.x, -q2.y, -q2.z, -q2.w);
}

constexpr Quaternion Quaternion::operator*(real_t p_s) const {
	return Quaternion(x * p_s, y * p_s, z * p_s, w * p_s);
}

constexpr Quaternion Quaternion::operator/(real_t p_s) const {
	return *this * (1 / p_s);
}

constexpr bool Quaternion::operator==(const Quaternion &p_quaternion) const {
	return x == p_quaternion.x && y == p_quaternion.y && z == p_quaternion.z && w == p_quaternion.w;
}

constexpr bool Quaternion::operator!=(const Quaternion &p_quaternion) const {
	return x != p_quaternion.x || y != p_quaternion.y || z != p_quaternion.z || w != p_quaternion.w;
}

constexpr void Quaternion::operator*=(const Quaternion &p_q) {
	real_t xx = w * p_q.x + x * p_q.w + y * p_q.z - z * p_q.y;
	real_t yy = w * p_q.y + y * p_q.w + z * p_q.x - x * p_q.z;
	real_t zz = w * p_q.z + z * p_q.w + x * p_q.y - y * p_q.x;
	w = w * p_q.w - x * p_q.x - y * p_q.y - z * p_q.z;
	x = xx;
	y = yy;
	z = zz;
}

constexpr Quaternion Quaternion::operator*(const Quaternion &p_q) const {
	Quaternion r = *this;
	r *= p_q;
	return r;
}

constexpr Quaternion operator*(real_t p_real, const Quaternion &p_quaternion) {
	return p_quaternion * p_real;
}
