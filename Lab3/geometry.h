//geometry.h
#pragma once
#include <cmath>
#include <algorithm>

class Vec2 {
public:
  float x, y;
  Vec2() : x(0), y(0) {}
  Vec2(float x, float y) : x(x), y(y) {}

  Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
  Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
  Vec2 operator*(float f) const { return Vec2(x * f, y * f); }
  Vec2 operator/(float f) const { return Vec2(x / f, y / f); }
};

class Vec3 {
public:
  float x, y, z;
  Vec3() : x(0), y(0), z(0) {}
  Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

  Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
  Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
  Vec3 operator*(float f) const { return Vec3(x * f, y * f, z * f); }
  Vec3 operator/(float f) const { return Vec3(x / f, y / f, z / f); }
  Vec3 operator-() const { return Vec3(-x, -y, -z); }
  Vec3 operator*(const Vec3& v) const { return Vec3(x * v.x, y * v.y, z * v.z); }

  float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
  Vec3 cross(const Vec3& v) const {
    return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
  }

  float length() const { return sqrtf(x * x + y * y + z * z); }
  Vec3 normalize() const {
    float len = length();
    if (len > 0) return *this / len;
    return *this;
  }
};

class Vec4 {
public:
  float x, y, z, w;
  Vec4() : x(0), y(0), z(0), w(0) {}
  Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
  Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

  Vec3 xyz() const { return Vec3(x, y, z); }
};

class Mat4 {
public:
  float m[4][4];

  Mat4() {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < 4; j++)
        m[i][j] = (i == j) ? 1.0f : 0.0f;
  }

  static Mat4 identity() { return Mat4(); }

  static Mat4 translation(const Vec3& t) {
    Mat4 result;
    result.m[0][3] = t.x;
    result.m[1][3] = t.y;
    result.m[2][3] = t.z;
    return result;
  }

  static Mat4 scale(const Vec3& s) {
    Mat4 result;
    result.m[0][0] = s.x;
    result.m[1][1] = s.y;
    result.m[2][2] = s.z;
    return result;
  }

  static Mat4 perspective(float fov, float aspect, float near, float far) {
    Mat4 result;
    float tanHalfFov = tanf(fov * 0.5f);
    result.m[0][0] = 1.0f / (aspect * tanHalfFov);
    result.m[1][1] = 1.0f / tanHalfFov;
    result.m[2][2] = -(far + near) / (far - near);
    result.m[2][3] = -2.0f * far * near / (far - near);
    result.m[3][2] = -1.0f;
    result.m[3][3] = 0.0f;
    return result;
  }

  static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalize();
    Vec3 s = f.cross(up.normalize()).normalize();
    Vec3 u = s.cross(f);

    Mat4 result;
    result.m[0][0] = s.x;
    result.m[0][1] = s.y;
    result.m[0][2] = s.z;
    result.m[0][3] = -s.dot(eye);

    result.m[1][0] = u.x;
    result.m[1][1] = u.y;
    result.m[1][2] = u.z;
    result.m[1][3] = -u.dot(eye);

    result.m[2][0] = -f.x;
    result.m[2][1] = -f.y;
    result.m[2][2] = -f.z;
    result.m[2][3] = f.dot(eye);

    return result;
  }

  Mat4 operator*(const Mat4& other) const {
    Mat4 result;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        result.m[i][j] = 0;
        for (int k = 0; k < 4; k++) {
          result.m[i][j] += m[i][k] * other.m[k][j];
        }
      }
    }
    return result;
  }

  Vec4 operator*(const Vec4& v) const {
    return Vec4(
      m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
      m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
      m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
      m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w
    );
  }
};

inline float radians(float degrees) {
  return degrees * 3.14159265359f / 180.0f;
}

inline float clamp(float x, float min_val, float max_val) {
  return std::max(min_val, std::min(x, max_val));
}

inline Vec3 normalize(const Vec3& v) {
  return v.normalize();
}