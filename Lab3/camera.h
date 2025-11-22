//camera.h
#pragma once
#include "geometry.h"

class Camera {
public:
  Vec3 position;
  Vec3 target;
  Vec3 up;
  float fov;
  float aspect;
  float near_plane;
  float far_plane;

  Camera(Vec3 position = Vec3(0, 0, 0),
    Vec3 target = Vec3(0, 0, 0),
    Vec3 up = Vec3(0, 1, 0),
    float fov_degrees = 45.0f,
    float aspect_ratio = 1.0f,
    float near_plane = 0.1f,
    float far_plane = 100.0f)
    : position(position), target(target), up(up.normalize()),
    fov(radians(fov_degrees)), aspect(aspect_ratio),
    near_plane(near_plane), far_plane(far_plane) {
  }

  Mat4 view_matrix() const {
    return Mat4::lookAt(position, target, up);
  }

  Mat4 projection_matrix() const {
    return Mat4::perspective(fov, aspect, near_plane, far_plane);
  }
};