#include "Math.h"

void Math::AnglesInterpolate(vec3_t start, vec3_t end, vec3_t _, float frac) {
	float d, ang1, ang2;
	for (int i = 0; i < 3; i++) {
		ang1 = start[i];
		ang2 = end[i];
		d = ang1 - ang2;
		if (d > 180.0f) d -= 360.0f;
		else if (d < -180.0f) d += 360.0f;
		_[i] = ang2 + d * frac;
	}
}

void Math::AnglesQuaternion(const vec3_t angles, vec4_t _) {
	float sr, sp, sy, cr, cp, cy;
#ifdef XASH_VECTORIZE_SINCOS
	SinCosFastVector3(angles[2] * 0.5f, angles[1] * 0.5f, angles[0] * 0.5f,
		&sy, &sp, &sr,
		&cy, &cp, &cr);
#else
	float angle = angles[2] * 0.5f;
	SinCos(angle, &sy, &cy);
	angle = angles[1] * 0.5f;
	SinCos(angle, &sp, &cp);
	angle = angles[0] * 0.5f;
	SinCos(angle, &sr, &cr);
#endif
	_[0] = sr * cp * cy - cr * sp * sy; // X
	_[1] = cr * sp * cy + sr * cp * sy; // Y
	_[2] = cr * cp * sy - sr * sp * cy; // Z
	_[3] = cr * cp * cy + sr * sp * sy; // W
}

void Math::AnglesVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	static float sr, sp, sy, cr, cp, cy;
#ifdef XASH_VECTORIZE_SINCOS
	SinCosFastVector3(Math_DEG2RAD(angles[M_YAW]), Math_DEG2RAD(angles[M_PITCH]), Math_DEG2RAD(angles[M_ROLL]),
		&sy, &sp, &sr,
		&cy, &cp, &cr);
#else
	SinCos(Math_DEG2RAD(angles[M_YAW]), &sy, &cy);
	SinCos(Math_DEG2RAD(angles[M_PITCH]), &sp, &cp);
	SinCos(Math_DEG2RAD(angles[M_ROLL]), &sr, &cr);
#endif
	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right) {
		right[0] = -1.0f * sr * sp * cy + -1.0f * cr * -sy;
		right[1] = -1.0f * sr * sp * sy + -1.0f * cr * cy;
		right[2] = -1.0f * sr * cp;
	}
	if (up) {
		up[0] = cr * sp * cy + -sr * -sy;
		up[1] = cr * sp * sy + -sr * cy;
		up[2] = cr * cp;
	}
}

float Math::ApproachValue(float target, float value, float speed) {
	float delta = target - value;
	return delta > speed ? value + speed
		: delta < -speed ? value - speed
		: target;
}

bool Math::BoundsAndSphereIntersect(const vec3_t mins, const vec3_t maxs, const vec3_t origin, float radius) {
	if (mins[0] > origin[0] + radius || mins[1] > origin[1] + radius || mins[2] > origin[2] + radius) return false;
	if (maxs[0] < origin[0] - radius || maxs[1] < origin[1] - radius || maxs[2] < origin[2] - radius) return false;
	return true;
}

bool Math::BoundsIntersect(const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2) {
	if (mins1[0] > maxs2[0] || mins1[1] > maxs2[1] || mins1[2] > maxs2[2]) return false;
	if (maxs1[0] < mins2[0] || maxs1[1] < mins2[1] || maxs1[2] < mins2[2]) return false;
	return true;
}

unsigned short Math::FloatToHalf(float value) {
	unsigned int i = *((unsigned int*)&value);
	unsigned int e = (i >> 23) & 0x00ff;
	unsigned int m = i & 0x007fffff;
	unsigned short h = e <= 127 - 15
		? ((m | 0x00800000) >> (127 - 14 - e)) >> 13
		: (i >> 13) & 0x3fff;
	h |= (i >> 16) & 0xc000;
	return h;
}

float Math::HalfToFloat(unsigned short value) {
	unsigned int f = (value << 16) & 0x80000000;
	unsigned int em = value & 0x7fff;
	if (em > 0x03ff)
		f |= (em << 13) + ((127 - 15) << 23);
	else {
		unsigned int m = em & 0x03ff;
		if (m != 0) {
			unsigned int e = (em >> 10) & 0x1f;
			while (!(m & 0x0400)) { m <<= 1; e--; }
			m &= 0x3ff;
			f |= ((e + (127 - 14)) << 23) | (m << 13);
		}
	}
	return *((float*)&f);
}

void Math::Matrix3x4_ConcatTransforms(matrix3x4 _, cmatrix3x4 m1, cmatrix3x4 m2) {
	_[0][0] = m1[0][0] * m2[0][0] + m1[0][1] * m2[1][0] + m1[0][2] * m2[2][0];
	_[0][1] = m1[0][0] * m2[0][1] + m1[0][1] * m2[1][1] + m1[0][2] * m2[2][1];
	_[0][2] = m1[0][0] * m2[0][2] + m1[0][1] * m2[1][2] + m1[0][2] * m2[2][2];
	_[0][3] = m1[0][0] * m2[0][3] + m1[0][1] * m2[1][3] + m1[0][2] * m2[2][3] + m1[0][3];
	_[1][0] = m1[1][0] * m2[0][0] + m1[1][1] * m2[1][0] + m1[1][2] * m2[2][0];
	_[1][1] = m1[1][0] * m2[0][1] + m1[1][1] * m2[1][1] + m1[1][2] * m2[2][1];
	_[1][2] = m1[1][0] * m2[0][2] + m1[1][1] * m2[1][2] + m1[1][2] * m2[2][2];
	_[1][3] = m1[1][0] * m2[0][3] + m1[1][1] * m2[1][3] + m1[1][2] * m2[2][3] + m1[1][3];
	_[2][0] = m1[2][0] * m2[0][0] + m1[2][1] * m2[1][0] + m1[2][2] * m2[2][0];
	_[2][1] = m1[2][0] * m2[0][1] + m1[2][1] * m2[1][1] + m1[2][2] * m2[2][1];
	_[2][2] = m1[2][0] * m2[0][2] + m1[2][1] * m2[1][2] + m1[2][2] * m2[2][2];
	_[2][3] = m1[2][0] * m2[0][3] + m1[2][1] * m2[1][3] + m1[2][2] * m2[2][3] + m1[2][3];
}

void Math::Matrix3x4_CreateFromEntity(matrix3x4 _, const vec3_t angles, const vec3_t origin, float scale) {
	float angle, sr, sp, sy, cr, cp, cy;
	if (angles[M_ROLL]) {
#ifdef XASH_VECTORIZE_SINCOS
		SinCosFastVector3(Math_DEG2RAD(angles[M_YAW]), Math_DEG2RAD(angles[M_PITCH]), Math_DEG2RAD(angles[M_ROLL]),
			&sy, &sp, &sr,
			&cy, &cp, &cr);
#else
		angle = angles[M_YAW] * (M_PI2 / 360.0f);
		SinCos(angle, &sy, &cy);
		angle = angles[M_PITCH] * (M_PI2 / 360.0f);
		SinCos(angle, &sp, &cp);
		angle = angles[M_ROLL] * (M_PI2 / 360.0f);
		SinCos(angle, &sr, &cr);
#endif
		_[0][0] = (cp * cy) * scale;
		_[0][1] = (sr * sp * cy + cr * -sy) * scale;
		_[0][2] = (cr * sp * cy + -sr * -sy) * scale;
		_[0][3] = origin[0];
		_[1][0] = (cp * sy) * scale;
		_[1][1] = (sr * sp * sy + cr * cy) * scale;
		_[1][2] = (cr * sp * sy + -sr * cy) * scale;
		_[1][3] = origin[1];
		_[2][0] = (-sp) * scale;
		_[2][1] = (sr * cp) * scale;
		_[2][2] = (cr * cp) * scale;
		_[2][3] = origin[2];
	}
	else if (angles[M_PITCH]) {
#ifdef XASH_VECTORIZE_SINCOS
		SinCosFastVector2(Math_DEG2RAD(angles[M_YAW]), Math_DEG2RAD(angles[M_PITCH]),
			&sy, &sp,
			&cy, &cp);
#else
		angle = angles[M_YAW] * (M_PI2 / 360.0f);
		SinCos(angle, &sy, &cy);
		angle = angles[M_PITCH] * (M_PI2 / 360.0f);
		SinCos(angle, &sp, &cp);
#endif
		_[0][0] = (cp * cy) * scale;
		_[0][1] = (-sy) * scale;
		_[0][2] = (sp * cy) * scale;
		_[0][3] = origin[0];
		_[1][0] = (cp * sy) * scale;
		_[1][1] = (cy)*scale;
		_[1][2] = (sp * sy) * scale;
		_[1][3] = origin[1];
		_[2][0] = (-sp) * scale;
		_[2][1] = 0.0f;
		_[2][2] = (cp)*scale;
		_[2][3] = origin[2];
	}
	else if (angles[M_YAW]) {
		angle = angles[M_YAW] * (M_PI2 / 360.0f);
		SinCos(angle, &sy, &cy);
		_[0][0] = (cy)*scale;
		_[0][1] = (-sy) * scale;
		_[0][2] = 0.0f;
		_[0][3] = origin[0];
		_[1][0] = (sy)*scale;
		_[1][1] = (cy)*scale;
		_[1][2] = 0.0f;
		_[1][3] = origin[1];
		_[2][0] = 0.0f;
		_[2][1] = 0.0f;
		_[2][2] = scale;
		_[2][3] = origin[2];
	}
	else {
		_[0][0] = scale;
		_[0][1] = 0.0f;
		_[0][2] = 0.0f;
		_[0][3] = origin[0];
		_[1][0] = 0.0f;
		_[1][1] = scale;
		_[1][2] = 0.0f;
		_[1][3] = origin[1];
		_[2][0] = 0.0f;
		_[2][1] = 0.0f;
		_[2][2] = scale;
		_[2][3] = origin[2];
	}
}

void Math::Matrix3x4_FromOriginQuat(matrix3x4 _, const vec4_t quaternion, const vec3_t origin) {
	_[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
	_[1][0] = 2.0f * quaternion[0] * quaternion[1] + 2.0f * quaternion[3] * quaternion[2];
	_[2][0] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];
	_[0][1] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
	_[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
	_[2][1] = 2.0f * quaternion[1] * quaternion[2] + 2.0f * quaternion[3] * quaternion[0];
	_[0][2] = 2.0f * quaternion[0] * quaternion[2] + 2.0f * quaternion[3] * quaternion[1];
	_[1][2] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
	_[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];
	_[0][3] = origin[0];
	_[1][3] = origin[1];
	_[2][3] = origin[2];
}

const matrix3x4 Math::Matrix3x4_Identity = {
	{ 1, 0, 0, 0 },	// PITCH [forward], org[0]
	{ 0, 1, 0, 0 },	// YAW [right]    , org[1]
	{ 0, 0, 1, 0 },	// ROLL [up]      , org[2]
};

void Math::Matrix3x4_InvertSimple(matrix3x4 _, cmatrix3x4 m) {
	// we only support uniform scaling, so assume the first row is enough (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which makes the sqrt a waste of time)
	float scale = 1.0f / (m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
	// invert the rotation by transposing and multiplying by the squared recipricol of the input matrix scale as described above
	_[0][0] = m[0][0] * scale;
	_[0][1] = m[1][0] * scale;
	_[0][2] = m[2][0] * scale;
	_[1][0] = m[0][1] * scale;
	_[1][1] = m[1][1] * scale;
	_[1][2] = m[2][1] * scale;
	_[2][0] = m[0][2] * scale;
	_[2][1] = m[1][2] * scale;
	_[2][2] = m[2][2] * scale;
	// invert the translate
	_[0][3] = -(m[0][3] * _[0][0] + m[1][3] * _[0][1] + m[2][3] * _[0][2]);
	_[1][3] = -(m[0][3] * _[1][0] + m[1][3] * _[1][1] + m[2][3] * _[1][2]);
	_[2][3] = -(m[0][3] * _[2][0] + m[1][3] * _[2][1] + m[2][3] * _[2][2]);
}

void Math::Matrix3x4_OriginFromMatrix(cmatrix3x4 m, float* _) {
	_[0] = m[0][3];
	_[1] = m[1][3];
	_[2] = m[2][3];
}

void Math::Matrix3x4_SetOrigin(matrix3x4 _, float x, float y, float z) {
	_[0][3] = x;
	_[1][3] = y;
	_[2][3] = z;
}

void Math::Matrix3x4_TransformPositivePlane(cmatrix3x4 m, const vec3_t normal, float d, vec3_t _, float* dist) {
	float scale = sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
	float iscale = 1.0f / scale;
	_[0] = (normal[0] * m[0][0] + normal[1] * m[0][1] + normal[2] * m[0][2]) * iscale;
	_[1] = (normal[0] * m[1][0] + normal[1] * m[1][1] + normal[2] * m[1][2]) * iscale;
	_[2] = (normal[0] * m[2][0] + normal[1] * m[2][1] + normal[2] * m[2][2]) * iscale;
	*dist = d * scale + (_[0] * m[0][3] + _[1] * m[1][3] + _[2] * m[2][3]);
}

void Math::Matrix3x4_VectorIRotate(cmatrix3x4 m, const float v[3], float _[3]) {
	_[0] = v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0];
	_[1] = v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1];
	_[2] = v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2];
}

void Math::Matrix3x4_VectorITransform(cmatrix3x4 m, const float v[3], float _[3]) {
	vec3_t	dir;
	dir[0] = v[0] - m[0][3];
	dir[1] = v[1] - m[1][3];
	dir[2] = v[2] - m[2][3];
	_[0] = dir[0] * m[0][0] + dir[1] * m[1][0] + dir[2] * m[2][0];
	_[1] = dir[0] * m[0][1] + dir[1] * m[1][1] + dir[2] * m[2][1];
	_[2] = dir[0] * m[0][2] + dir[1] * m[1][2] + dir[2] * m[2][2];
}

void Math::Matrix3x4_VectorRotate(cmatrix3x4 m, const float v[3], float _[3]) {
	_[0] = v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2];
	_[1] = v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2];
	_[2] = v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2];
}

void Math::Matrix3x4_VectorTransform(cmatrix3x4 m, const float v[3], float _[3]) {
	_[0] = v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + m[0][3];
	_[1] = v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + m[1][3];
	_[2] = v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + m[2][3];
}

void Math::Matrix4x4_Concat(matrix4x4 _, const matrix4x4 m1, const matrix4x4 m2) {
	_[0][0] = m1[0][0] * m2[0][0] + m1[0][1] * m2[1][0] + m1[0][2] * m2[2][0] + m1[0][3] * m2[3][0];
	_[0][1] = m1[0][0] * m2[0][1] + m1[0][1] * m2[1][1] + m1[0][2] * m2[2][1] + m1[0][3] * m2[3][1];
	_[0][2] = m1[0][0] * m2[0][2] + m1[0][1] * m2[1][2] + m1[0][2] * m2[2][2] + m1[0][3] * m2[3][2];
	_[0][3] = m1[0][0] * m2[0][3] + m1[0][1] * m2[1][3] + m1[0][2] * m2[2][3] + m1[0][3] * m2[3][3];
	_[1][0] = m1[1][0] * m2[0][0] + m1[1][1] * m2[1][0] + m1[1][2] * m2[2][0] + m1[1][3] * m2[3][0];
	_[1][1] = m1[1][0] * m2[0][1] + m1[1][1] * m2[1][1] + m1[1][2] * m2[2][1] + m1[1][3] * m2[3][1];
	_[1][2] = m1[1][0] * m2[0][2] + m1[1][1] * m2[1][2] + m1[1][2] * m2[2][2] + m1[1][3] * m2[3][2];
	_[1][3] = m1[1][0] * m2[0][3] + m1[1][1] * m2[1][3] + m1[1][2] * m2[2][3] + m1[1][3] * m2[3][3];
	_[2][0] = m1[2][0] * m2[0][0] + m1[2][1] * m2[1][0] + m1[2][2] * m2[2][0] + m1[2][3] * m2[3][0];
	_[2][1] = m1[2][0] * m2[0][1] + m1[2][1] * m2[1][1] + m1[2][2] * m2[2][1] + m1[2][3] * m2[3][1];
	_[2][2] = m1[2][0] * m2[0][2] + m1[2][1] * m2[1][2] + m1[2][2] * m2[2][2] + m1[2][3] * m2[3][2];
	_[2][3] = m1[2][0] * m2[0][3] + m1[2][1] * m2[1][3] + m1[2][2] * m2[2][3] + m1[2][3] * m2[3][3];
	_[3][0] = m1[3][0] * m2[0][0] + m1[3][1] * m2[1][0] + m1[3][2] * m2[2][0] + m1[3][3] * m2[3][0];
	_[3][1] = m1[3][0] * m2[0][1] + m1[3][1] * m2[1][1] + m1[3][2] * m2[2][1] + m1[3][3] * m2[3][1];
	_[3][2] = m1[3][0] * m2[0][2] + m1[3][1] * m2[1][2] + m1[3][2] * m2[2][2] + m1[3][3] * m2[3][2];
	_[3][3] = m1[3][0] * m2[0][3] + m1[3][1] * m2[1][3] + m1[3][2] * m2[2][3] + m1[3][3] * m2[3][3];
}

void Math::Matrix4x4_ConcatTransforms(matrix4x4 _, cmatrix4x4 m1, cmatrix4x4 m2) {
	_[0][0] = m1[0][0] * m2[0][0] + m1[0][1] * m2[1][0] + m1[0][2] * m2[2][0];
	_[0][1] = m1[0][0] * m2[0][1] + m1[0][1] * m2[1][1] + m1[0][2] * m2[2][1];
	_[0][2] = m1[0][0] * m2[0][2] + m1[0][1] * m2[1][2] + m1[0][2] * m2[2][2];
	_[0][3] = m1[0][0] * m2[0][3] + m1[0][1] * m2[1][3] + m1[0][2] * m2[2][3] + m1[0][3];
	_[1][0] = m1[1][0] * m2[0][0] + m1[1][1] * m2[1][0] + m1[1][2] * m2[2][0];
	_[1][1] = m1[1][0] * m2[0][1] + m1[1][1] * m2[1][1] + m1[1][2] * m2[2][1];
	_[1][2] = m1[1][0] * m2[0][2] + m1[1][1] * m2[1][2] + m1[1][2] * m2[2][2];
	_[1][3] = m1[1][0] * m2[0][3] + m1[1][1] * m2[1][3] + m1[1][2] * m2[2][3] + m1[1][3];
	_[2][0] = m1[2][0] * m2[0][0] + m1[2][1] * m2[1][0] + m1[2][2] * m2[2][0];
	_[2][1] = m1[2][0] * m2[0][1] + m1[2][1] * m2[1][1] + m1[2][2] * m2[2][1];
	_[2][2] = m1[2][0] * m2[0][2] + m1[2][1] * m2[1][2] + m1[2][2] * m2[2][2];
	_[2][3] = m1[2][0] * m2[0][3] + m1[2][1] * m2[1][3] + m1[2][2] * m2[2][3] + m1[2][3];
}

void Math::Matrix4x4_ConvertToEntity(cmatrix4x4 m, vec3_t angles, vec3_t origin) {
	float xyDist = sqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0]);
	// enough here to get angles?
	if (xyDist > 0.001f) {
		angles[0] = Math_RAD2DEG(atan2(-m[2][0], xyDist));
		angles[1] = Math_RAD2DEG(atan2(m[1][0], m[0][0]));
		angles[2] = Math_RAD2DEG(atan2(m[2][1], m[2][2]));
	}
	else {	// forward is mostly Z, gimbal lock
		angles[0] = Math_RAD2DEG(atan2(-m[2][0], xyDist));
		angles[1] = Math_RAD2DEG(atan2(-m[0][1], m[1][1]));
		angles[2] = 0.0f;
	}
	origin[0] = m[0][3];
	origin[1] = m[1][3];
	origin[2] = m[2][3];
}

void Math::Matrix4x4_CreateFromEntity(matrix4x4 _, const vec3_t angles, const vec3_t origin, float scale) {
	float angle, sr, sp, sy, cr, cp, cy;
	if (angles[M_ROLL]) {
#ifdef XASH_VECTORIZE_SINCOS
		SinCosFastVector3(Math_DEG2RAD(angles[M_YAW]), Math_DEG2RAD(angles[M_PITCH]), Math_DEG2RAD(angles[M_ROLL]),
			&sy, &sp, &sr,
			&cy, &cp, &cr);
#else
		angle = angles[M_YAW] * (M_PI2 / 360.0f);
		SinCos(angle, &sy, &cy);
		angle = angles[M_PITCH] * (M_PI2 / 360.0f);
		SinCos(angle, &sp, &cp);
		angle = angles[M_ROLL] * (M_PI2 / 360.0f);
		SinCos(angle, &sr, &cr);
#endif
		_[0][0] = (cp * cy) * scale;
		_[0][1] = (sr * sp * cy + cr * -sy) * scale;
		_[0][2] = (cr * sp * cy + -sr * -sy) * scale;
		_[0][3] = origin[0];
		_[1][0] = (cp * sy) * scale;
		_[1][1] = (sr * sp * sy + cr * cy) * scale;
		_[1][2] = (cr * sp * sy + -sr * cy) * scale;
		_[1][3] = origin[1];
		_[2][0] = (-sp) * scale;
		_[2][1] = (sr * cp) * scale;
		_[2][2] = (cr * cp) * scale;
		_[2][3] = origin[2];
		_[3][0] = 0.0f;
		_[3][1] = 0.0f;
		_[3][2] = 0.0f;
		_[3][3] = 1.0f;
	}
	else if (angles[M_PITCH]) {
#ifdef XASH_VECTORIZE_SINCOS
		SinCosFastVector2(Math_DEG2RAD(angles[M_YAW]), Math_DEG2RAD(angles[M_PITCH]),
			&sy, &sp,
			&cy, &cp);
#else
		angle = angles[M_YAW] * (M_PI2 / 360.0f);
		SinCos(angle, &sy, &cy);
		angle = angles[M_PITCH] * (M_PI2 / 360.0f);
		SinCos(angle, &sp, &cp);
#endif
		_[0][0] = (cp * cy) * scale;
		_[0][1] = (-sy) * scale;
		_[0][2] = (sp * cy) * scale;
		_[0][3] = origin[0];
		_[1][0] = (cp * sy) * scale;
		_[1][1] = (cy)*scale;
		_[1][2] = (sp * sy) * scale;
		_[1][3] = origin[1];
		_[2][0] = (-sp) * scale;
		_[2][1] = 0.0f;
		_[2][2] = (cp)*scale;
		_[2][3] = origin[2];
		_[3][0] = 0.0f;
		_[3][1] = 0.0f;
		_[3][2] = 0.0f;
		_[3][3] = 1.0f;
	}
	else if (angles[M_YAW]) {
		angle = angles[M_YAW] * (M_PI2 / 360.0f);
		SinCos(angle, &sy, &cy);
		_[0][0] = (cy)*scale;
		_[0][1] = (-sy) * scale;
		_[0][2] = 0.0f;
		_[0][3] = origin[0];
		_[1][0] = (sy)*scale;
		_[1][1] = (cy)*scale;
		_[1][2] = 0.0f;
		_[1][3] = origin[1];
		_[2][0] = 0.0f;
		_[2][1] = 0.0f;
		_[2][2] = scale;
		_[2][3] = origin[2];
		_[3][0] = 0.0f;
		_[3][1] = 0.0f;
		_[3][2] = 0.0f;
		_[3][3] = 1.0f;
	}
	else {
		_[0][0] = scale;
		_[0][1] = 0.0f;
		_[0][2] = 0.0f;
		_[0][3] = origin[0];
		_[1][0] = 0.0f;
		_[1][1] = scale;
		_[1][2] = 0.0f;
		_[1][3] = origin[1];
		_[2][0] = 0.0f;
		_[2][1] = 0.0f;
		_[2][2] = scale;
		_[2][3] = origin[2];
		_[3][0] = 0.0f;
		_[3][1] = 0.0f;
		_[3][2] = 0.0f;
		_[3][3] = 1.0f;
	}
}

void Math::Matrix4x4_CreateTranslate(matrix4x4 _, double x, double y, double z) {
	_[0][0] = 1.0f;
	_[0][1] = 0.0f;
	_[0][2] = 0.0f;
	_[0][3] = x;
	_[1][0] = 0.0f;
	_[1][1] = 1.0f;
	_[1][2] = 0.0f;
	_[1][3] = y;
	_[2][0] = 0.0f;
	_[2][1] = 0.0f;
	_[2][2] = 1.0f;
	_[2][3] = z;
	_[3][0] = 0.0f;
	_[3][1] = 0.0f;
	_[3][2] = 0.0f;
	_[3][3] = 1.0f;
}

void Math::Matrix4x4_FromOriginQuat(matrix4x4 _, const vec4_t quaternion, const vec3_t origin) {
	_[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
	_[1][0] = 2.0f * quaternion[0] * quaternion[1] + 2.0f * quaternion[3] * quaternion[2];
	_[2][0] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];
	_[0][3] = origin[0];
	_[0][1] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
	_[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
	_[2][1] = 2.0f * quaternion[1] * quaternion[2] + 2.0f * quaternion[3] * quaternion[0];
	_[1][3] = origin[1];
	_[0][2] = 2.0f * quaternion[0] * quaternion[2] + 2.0f * quaternion[3] * quaternion[1];
	_[1][2] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
	_[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];
	_[2][3] = origin[2];
	_[3][0] = 0.0f;
	_[3][1] = 0.0f;
	_[3][2] = 0.0f;
	_[3][3] = 1.0f;
}

const matrix4x4 Math::Matrix4x4_Identity = {
	{ 1, 0, 0, 0 },	// PITCH
	{ 0, 1, 0, 0 },	// YAW
	{ 0, 0, 1, 0 },	// ROLL
	{ 0, 0, 0, 1 },	// ORIGIN
};


bool Math::Matrix4x4_InvertFull(matrix4x4 _, cmatrix4x4 m) {
	float* temp;
	float* r[4];
	float rtemp[4][8];
	float m_[4];
	float s;
	r[0] = rtemp[0];
	r[1] = rtemp[1];
	r[2] = rtemp[2];
	r[3] = rtemp[3];
	r[0][0] = m[0][0];
	r[0][1] = m[0][1];
	r[0][2] = m[0][2];
	r[0][3] = m[0][3];
	r[0][4] = 1.0f;
	r[0][5] = 0.0f;
	r[0][6] = 0.0f;
	r[0][7] = 0.0f;
	r[1][0] = m[1][0];
	r[1][1] = m[1][1];
	r[1][2] = m[1][2];
	r[1][3] = m[1][3];
	r[1][5] = 1.0f;
	r[1][4] = 0.0f;
	r[1][6] = 0.0f;
	r[1][7] = 0.0f;
	r[2][0] = m[2][0];
	r[2][1] = m[2][1];
	r[2][2] = m[2][2];
	r[2][3] = m[2][3];
	r[2][6] = 1.0f;
	r[2][4] = 0.0f;
	r[2][5] = 0.0f;
	r[2][7] = 0.0f;
	r[3][0] = m[3][0];
	r[3][1] = m[3][1];
	r[3][2] = m[3][2];
	r[3][3] = m[3][3];
	r[3][4] = 0.0f;
	r[3][5] = 0.0f;
	r[3][6] = 0.0f;
	r[3][7] = 1.0f;
	if (fabs(r[3][0]) > fabs(r[2][0])) {
		temp = r[3];
		r[3] = r[2];
		r[2] = temp;
	}
	if (fabs(r[2][0]) > fabs(r[1][0])) {
		temp = r[2];
		r[2] = r[1];
		r[1] = temp;
	}
	if (fabs(r[1][0]) > fabs(r[0][0])) {
		temp = r[1];
		r[1] = r[0];
		r[0] = temp;
	}
	if (r[0][0]) {
		m_[1] = r[1][0] / r[0][0];
		m_[2] = r[2][0] / r[0][0];
		m_[3] = r[3][0] / r[0][0];
		s = r[0][1];
		r[1][1] -= m_[1] * s;
		r[2][1] -= m_[2] * s;
		r[3][1] -= m_[3] * s;
		s = r[0][2];
		r[1][2] -= m_[1] * s;
		r[2][2] -= m_[2] * s;
		r[3][2] -= m_[3] * s;
		s = r[0][3];
		r[1][3] -= m_[1] * s;
		r[2][3] -= m_[2] * s;
		r[3][3] -= m_[3] * s;
		s = r[0][4];
		if (s) {
			r[1][4] -= m_[1] * s;
			r[2][4] -= m_[2] * s;
			r[3][4] -= m_[3] * s;
		}
		s = r[0][5];
		if (s) {
			r[1][5] -= m_[1] * s;
			r[2][5] -= m_[2] * s;
			r[3][5] -= m_[3] * s;
		}
		s = r[0][6];
		if (s) {
			r[1][6] -= m_[1] * s;
			r[2][6] -= m_[2] * s;
			r[3][6] -= m_[3] * s;
		}
		s = r[0][7];
		if (s) {
			r[1][7] -= m_[1] * s;
			r[2][7] -= m_[2] * s;
			r[3][7] -= m_[3] * s;
		}
		if (fabs(r[3][1]) > fabs(r[2][1])) {
			temp = r[3];
			r[3] = r[2];
			r[2] = temp;
		}
		if (fabs(r[2][1]) > fabs(r[1][1])) {
			temp = r[2];
			r[2] = r[1];
			r[1] = temp;
		}
		if (r[1][1]) {
			m_[2] = r[2][1] / r[1][1];
			m_[3] = r[3][1] / r[1][1];
			r[2][2] -= m_[2] * r[1][2];
			r[3][2] -= m_[3] * r[1][2];
			r[2][3] -= m_[2] * r[1][3];
			r[3][3] -= m_[3] * r[1][3];
			s = r[1][4];
			if (s) {
				r[2][4] -= m_[2] * s;
				r[3][4] -= m_[3] * s;
			}
			s = r[1][5];
			if (s) {
				r[2][5] -= m_[2] * s;
				r[3][5] -= m_[3] * s;
			}
			s = r[1][6];
			if (s) {
				r[2][6] -= m_[2] * s;
				r[3][6] -= m_[3] * s;
			}
			s = r[1][7];
			if (s) {
				r[2][7] -= m_[2] * s;
				r[3][7] -= m_[3] * s;
			}
			if (fabs(r[3][2]) > fabs(r[2][2])) {
				temp = r[3];
				r[3] = r[2];
				r[2] = temp;
			}
			if (r[2][2]) {
				m_[3] = r[3][2] / r[2][2];
				r[3][3] -= m_[3] * r[2][3];
				r[3][4] -= m_[3] * r[2][4];
				r[3][5] -= m_[3] * r[2][5];
				r[3][6] -= m_[3] * r[2][6];
				r[3][7] -= m_[3] * r[2][7];
				if (r[3][3]) {
					s = 1.0f / r[3][3];
					r[3][4] *= s;
					r[3][5] *= s;
					r[3][6] *= s;
					r[3][7] *= s;
					m_[2] = r[2][3];
					s = 1.0f / r[2][2];
					r[2][4] = s * (r[2][4] - r[3][4] * m_[2]);
					r[2][5] = s * (r[2][5] - r[3][5] * m_[2]);
					r[2][6] = s * (r[2][6] - r[3][6] * m_[2]);
					r[2][7] = s * (r[2][7] - r[3][7] * m_[2]);
					m_[1] = r[1][3];
					r[1][4] -= r[3][4] * m_[1];
					r[1][5] -= r[3][5] * m_[1];
					r[1][6] -= r[3][6] * m_[1];
					r[1][7] -= r[3][7] * m_[1];
					m_[0] = r[0][3];
					r[0][4] -= r[3][4] * m_[0];
					r[0][5] -= r[3][5] * m_[0];
					r[0][6] -= r[3][6] * m_[0];
					r[0][7] -= r[3][7] * m_[0];
					m_[1] = r[1][2];
					s = 1.0f / r[1][1];
					r[1][4] = s * (r[1][4] - r[2][4] * m_[1]);
					r[1][5] = s * (r[1][5] - r[2][5] * m_[1]);
					r[1][6] = s * (r[1][6] - r[2][6] * m_[1]);
					r[1][7] = s * (r[1][7] - r[2][7] * m_[1]);
					m_[0] = r[0][2];
					r[0][4] -= r[2][4] * m_[0];
					r[0][5] -= r[2][5] * m_[0];
					r[0][6] -= r[2][6] * m_[0];
					r[0][7] -= r[2][7] * m_[0];
					m_[0] = r[0][1];
					s = 1.0f / r[0][0];
					r[0][4] = s * (r[0][4] - r[1][4] * m_[0]);
					r[0][5] = s * (r[0][5] - r[1][5] * m_[0]);
					r[0][6] = s * (r[0][6] - r[1][6] * m_[0]);
					r[0][7] = s * (r[0][7] - r[1][7] * m_[0]);
					_[0][0] = r[0][4];
					_[0][1] = r[0][5];
					_[0][2] = r[0][6];
					_[0][3] = r[0][7];
					_[1][0] = r[1][4];
					_[1][1] = r[1][5];
					_[1][2] = r[1][6];
					_[1][3] = r[1][7];
					_[2][0] = r[2][4];
					_[2][1] = r[2][5];
					_[2][2] = r[2][6];
					_[2][3] = r[2][7];
					_[3][0] = r[3][4];
					_[3][1] = r[3][5];
					_[3][2] = r[3][6];
					_[3][3] = r[3][7];
					return true;
				}
			}
		}
	}
	return false;
}

void Math::Matrix4x4_InvertSimple(matrix4x4 _, cmatrix4x4 m) {
	// we only support uniform scaling, so assume the first row is enough
	// (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which
	// makes the sqrt a waste of time)
	float scale = 1.0f / (m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
	// invert the rotation by transposing and multiplying by the squared
	// recipricol of the input matrix scale as described above
	_[0][0] = m[0][0] * scale;
	_[0][1] = m[1][0] * scale;
	_[0][2] = m[2][0] * scale;
	_[1][0] = m[0][1] * scale;
	_[1][1] = m[1][1] * scale;
	_[1][2] = m[2][1] * scale;
	_[2][0] = m[0][2] * scale;
	_[2][1] = m[1][2] * scale;
	_[2][2] = m[2][2] * scale;
	// invert the translate
	_[0][3] = -(m[0][3] * _[0][0] + m[1][3] * _[0][1] + m[2][3] * _[0][2]);
	_[1][3] = -(m[0][3] * _[1][0] + m[1][3] * _[1][1] + m[2][3] * _[1][2]);
	_[2][3] = -(m[0][3] * _[2][0] + m[1][3] * _[2][1] + m[2][3] * _[2][2]);
	// don't know if there's anything worth doing here
	_[3][0] = 0.0f;
	_[3][1] = 0.0f;
	_[3][2] = 0.0f;
	_[3][3] = 1.0f;
}

void Math::Matrix4x4_OriginFromMatrix(cmatrix4x4 m, float* _) {
	_[0] = m[0][3];
	_[1] = m[1][3];
	_[2] = m[2][3];
}

void Math::Matrix4x4_SetOrigin(matrix4x4 _, float x, float y, float z) {
	_[0][3] = x;
	_[1][3] = y;
	_[2][3] = z;
}

void Math::Matrix4x4_TransformPositivePlane(cmatrix4x4 m, const vec3_t normal, float d, vec3_t _, float* dist) {
	float scale = sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
	float iscale = 1.0f / scale;
	_[0] = (normal[0] * m[0][0] + normal[1] * m[0][1] + normal[2] * m[0][2]) * iscale;
	_[1] = (normal[0] * m[1][0] + normal[1] * m[1][1] + normal[2] * m[1][2]) * iscale;
	_[2] = (normal[0] * m[2][0] + normal[1] * m[2][1] + normal[2] * m[2][2]) * iscale;
	*dist = d * scale + (_[0] * m[0][3] + _[1] * m[1][3] + _[2] * m[2][3]);
}

void Math::Matrix4x4_TransformStandardPlane(cmatrix4x4 m, const vec3_t normal, float d, vec3_t _, float* dist) {
	float scale = sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
	float iscale = 1.0f / scale;
	_[0] = (normal[0] * m[0][0] + normal[1] * m[0][1] + normal[2] * m[0][2]) * iscale;
	_[1] = (normal[0] * m[1][0] + normal[1] * m[1][1] + normal[2] * m[1][2]) * iscale;
	_[2] = (normal[0] * m[2][0] + normal[1] * m[2][1] + normal[2] * m[2][2]) * iscale;
	*dist = d * scale - (_[0] * m[0][3] + _[1] * m[1][3] + _[2] * m[2][3]);
}

void Math::Matrix4x4_Transpose(matrix4x4 _, cmatrix4x4 m) {
	_[0][0] = m[0][0];
	_[0][1] = m[1][0];
	_[0][2] = m[2][0];
	_[0][3] = m[3][0];
	_[1][0] = m[0][1];
	_[1][1] = m[1][1];
	_[1][2] = m[2][1];
	_[1][3] = m[3][1];
	_[2][0] = m[0][2];
	_[2][1] = m[1][2];
	_[2][2] = m[2][2];
	_[2][3] = m[3][2];
	_[3][0] = m[0][3];
	_[3][1] = m[1][3];
	_[3][2] = m[2][3];
	_[3][3] = m[3][3];
}

void Math::Matrix4x4_VectorIRotate(cmatrix4x4 m, const float v[3], float _[3]) {
	_[0] = v[0] * m[0][0] + v[1] * m[1][0] + v[2] * m[2][0];
	_[1] = v[0] * m[0][1] + v[1] * m[1][1] + v[2] * m[2][1];
	_[2] = v[0] * m[0][2] + v[1] * m[1][2] + v[2] * m[2][2];
}

void Math::Matrix4x4_VectorITransform(cmatrix4x4 m, const float v[3], float _[3]) {
	vec3_t	dir;
	dir[0] = v[0] - m[0][3];
	dir[1] = v[1] - m[1][3];
	dir[2] = v[2] - m[2][3];
	_[0] = dir[0] * m[0][0] + dir[1] * m[1][0] + dir[2] * m[2][0];
	_[1] = dir[0] * m[0][1] + dir[1] * m[1][1] + dir[2] * m[2][1];
	_[2] = dir[0] * m[0][2] + dir[1] * m[1][2] + dir[2] * m[2][2];
}

void Math::Matrix4x4_VectorRotate(cmatrix4x4 m, const float v[3], float _[3]) {
	_[0] = v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2];
	_[1] = v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2];
	_[2] = v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2];
}

void Math::Matrix4x4_VectorTransform(cmatrix4x4 m, const float v[3], float _[3]) {
	_[0] = v[0] * m[0][0] + v[1] * m[0][1] + v[2] * m[0][2] + m[0][3];
	_[1] = v[0] * m[1][0] + v[1] * m[1][1] + v[2] * m[1][2] + m[1][3];
	_[2] = v[0] * m[2][0] + v[1] * m[2][1] + v[2] * m[2][2] + m[2][3];
}

int Math::NearestPow(int value, bool roundDown) {
	int	n = 1;
	if (value <= 0) return 1;
	while (n < value) n <<= 1;
	if (roundDown && n > value) n >>= 1;
	return n;
}

void Math::QuaternionSlerp(const vec4_t p, vec4_t q, float t, vec4_t _) {
	float omega, sclp, sclq;
	float cosom, sinom;
	float a = 0.0f;
	float b = 0.0f;
	int	i;
	// decide if one of the quaternions is backwards
	for (i = 0; i < 4; i++) {
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}
	if (a > b)
		for (i = 0; i < 4; i++)
			q[i] = -q[i];
	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];
	if ((1.0 + cosom) > 0.000001f) {
		if ((1.0f - cosom) > 0.000001f) {
			omega = acos(cosom);
#ifdef XASH_VECTORIZE_SINCOS
			SinFastVector3(omega, (1.0f - t) * omega, t * omega,
				&sinom, &sclp, &sclq);
			sclp /= sinom;
			sclq /= sinom;
#else
			sinom = sin(omega);
			sclp = sin((1.0f - t) * omega) / sinom;
			sclq = sin(t * omega) / sinom;
#endif
		}
		else {
			sclp = 1.0f - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++)
			_[i] = sclp * p[i] + sclq * q[i];
	}
	else {
		_[0] = -q[1];
		_[1] = q[0];
		_[2] = -q[3];
		_[3] = q[2];
		sclp = sin((1.0f - t) * 0.5f * M_PI);
		sclq = sin(t * 0.5f * M_PI);
		for (i = 0; i < 3; i++)
			_[i] = sclp * p[i] + sclq * _[i];
	}
}

// remap a ranged value from [a:b] to [c:d]
float Math::RangeRemapValue(float value, float a, float b, float c, float d) { return c + (d - c) * (value - a) / (b - a); }

float Math::Rsqrt(float value) {
	if (value == 0.0f)
		return 0.0f;
	float x = value * 0.5f;
	int i = *(int*)&value;
	i = 0x5f3759df - (i >> 1);
	float y = *(float*)&i;
	y = y * (1.5f - (x * y * y));
	return y;
}

void Math::SinCos(float radians, float* sin, float* cos) {
#if _MSC_VER == 1200
	_asm {
		fld	dword ptr[radians]
		fsincos
		mov edx, dword ptr[cos]
		mov eax, dword ptr[sin]
		fstp dword ptr[edx]
		fstp dword ptr[eax]
	}
#else
#if defined (__linux__) && !defined (__ANDROID__)
	sincosf(radians, sin, cos);
#else
	* sin = sinf(radians);
	*cos = cosf(radians);
#endif
#endif
}

#ifdef XASH_VECTORIZE_SINCOS
void Math::SinCosFastVector2(float r1, float r2,
	float* s0, float* s1,
	float* c0, float* c1) {
	v4sf rad_vector = { r1, r2, 0, 0 };
	v4sf sin_vector, cos_vector;
	sincos_ps(rad_vector, &sin_vector, &cos_vector);
	*s0 = s4f_x(sin_vector);
	*s1 = s4f_y(sin_vector);
	*c0 = s4f_x(cos_vector);
	*c1 = s4f_y(cos_vector);
}

void Math::SinCosFastVector3(float r1, float r2, float r3,
	float* s0, float* s1, float* s2,
	float* c0, float* c1, float* c2) {
	v4sf rad_vector = { r1, r2, r3, 0 };
	v4sf sin_vector, cos_vector;
	sincos_ps(rad_vector, &sin_vector, &cos_vector);
	*s0 = s4f_x(sin_vector);
	*s1 = s4f_y(sin_vector);
	*s2 = s4f_z(sin_vector);
	*c0 = s4f_x(cos_vector);
	*c1 = s4f_y(cos_vector);
	*c2 = s4f_z(cos_vector);
}

void Math::SinCosFastVector4(float r1, float r2, float r3, float r4,
	float* s0, float* s1, float* s2, float* s3,
	float* c0, float* c1, float* c2, float* c3) {
	v4sf rad_vector = { r1, r2, r3, r4 };
	v4sf sin_vector, cos_vector;
	sincos_ps(rad_vector, &sin_vector, &cos_vector);
	*s0 = s4f_x(sin_vector);
	*s1 = s4f_y(sin_vector);
	*s2 = s4f_z(sin_vector);
	*s3 = s4f_w(sin_vector);
	*c0 = s4f_x(cos_vector);
	*c1 = s4f_y(cos_vector);
	*c2 = s4f_z(cos_vector);
	*c3 = s4f_w(cos_vector);
}

void Math::SinFastVector3(float r1, float r2, float r3,
	float* s0, float* s1, float* s2) {
	v4sf rad_vector = { r1, r2, r3, 0 };
	v4sf sin_vector;
	sin_vector = sin_ps(rad_vector);
	*s0 = s4f_x(sin_vector);
	*s1 = s4f_y(sin_vector);
	*s2 = s4f_z(sin_vector);
}
#endif

float Math::Vector2NormLen(const vec3_t v, vec3_t _) {
	float length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt(length);
	if (length) {
		float ilength = 1.0f / length;
		_[0] = v[0] * ilength;
		_[1] = v[1] * ilength;
		_[2] = v[2] * ilength;
	}
	return length;
}

void Math::Vector3Angles(const float* forward, float* angles) {
	float tmp, yaw, pitch;
	if (!forward || !angles) {
		if (angles) Math_Vector3Zero(angles);
		return;
	}
	if (forward[1] == 0 && forward[0] == 0) {
		// fast case
		yaw = 0;
		pitch = forward[2] > 0 ? 90.0f : 270.0f;
	}
	else {
		yaw = atan2(forward[1], forward[0]) * 180 / M_PI;
		if (yaw < 0) yaw += 360;
		tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = atan2(forward[2], tmp) * 180 / M_PI;
		if (pitch < 0) pitch += 360;
	}
	Math_Vector3Set(angles, pitch, yaw, 0);
}

void Math::VectorsAngles(const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t _) {
	float yaw, roll;
	float pitch = -asin(forward[2]);
	float cpitch = cos(pitch);
	if (fabs(cpitch) > EQUAL_EPSILON) {
		cpitch = 1.0f / cpitch;
		pitch = Math_RAD2DEG(pitch);
		yaw = Math_RAD2DEG(atan2(forward[1] * cpitch, forward[0] * cpitch));
		roll = Math_RAD2DEG(atan2(-right[2] * cpitch, up[2] * cpitch));
	}
	else {
		pitch = forward[2] > 0 ? -90.0f : 90.0f;
		yaw = Math_RAD2DEG(atan2(right[0], -right[1]));
		roll = 180.0f;
	}
	_[M_PITCH] = pitch;
	_[M_YAW] = yaw;
	_[M_ROLL] = roll;
}

void Math::VectorsVectors(const vec3_t forward, vec3_t right, vec3_t up) {
	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];
	float d = Math_DotProduct(forward, right);
	Math_Vector3MA(right, -d, forward, right);
	Math_Vector3Norm(right);
	Math_CrossProduct(right, forward, up);
}