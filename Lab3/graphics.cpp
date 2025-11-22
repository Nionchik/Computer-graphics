//graphics.cpp
#include "graphics.h"
#include <algorithm>
#include <limits>
#include <iostream>

GraphicsRenderer::GraphicsRenderer(int width, int height)
  : image_(width, height, 4),
  depth_buffer_(width* height, std::numeric_limits<float>::max()) {
}

Vec3 GraphicsRenderer::barycentric(const Vec3& A, const Vec3& B, const Vec3& C, const Vec3& P) {
  Vec3 s[2];
  s[0] = Vec3(C.x - A.x, B.x - A.x, A.x - P.x);
  s[1] = Vec3(C.y - A.y, B.y - A.y, A.y - P.y);
  Vec3 u = s[0].cross(s[1]);

  if (std::abs(u.z) > 1e-2) {
    return Vec3(1.0f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
  }
  return Vec3(-1, 1, 1);
}

void GraphicsRenderer::triangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, IShader& shader) {
  int width = image_.get_width();
  int height = image_.get_height();

  Vec3 pts[3] = {
      Vec3((v0.clip_position.x / v0.clip_position.w + 1.0f) * width * 0.5f,
           (v0.clip_position.y / v0.clip_position.w + 1.0f) * height * 0.5f,
           v0.clip_position.z / v0.clip_position.w),
      Vec3((v1.clip_position.x / v1.clip_position.w + 1.0f) * width * 0.5f,
           (v1.clip_position.y / v1.clip_position.w + 1.0f) * height * 0.5f,
           v1.clip_position.z / v1.clip_position.w),
      Vec3((v2.clip_position.x / v2.clip_position.w + 1.0f) * width * 0.5f,
           (v2.clip_position.y / v2.clip_position.w + 1.0f) * height * 0.5f,
           v2.clip_position.z / v2.clip_position.w)
  };

  int minX = std::max(0, (int)std::floor(std::min({ pts[0].x, pts[1].x, pts[2].x })));
  int maxX = std::min(width - 1, (int)std::ceil(std::max({ pts[0].x, pts[1].x, pts[2].x })));
  int minY = std::max(0, (int)std::floor(std::min({ pts[0].y, pts[1].y, pts[2].y })));
  int maxY = std::min(height - 1, (int)std::ceil(std::max({ pts[0].y, pts[1].y, pts[2].y })));

  for (int y = minY; y <= maxY; y++) {
    for (int x = minX; x <= maxX; x++) {
      Vec3 P(x + 0.5f, y + 0.5f, 0);
      Vec3 bc = barycentric(pts[0], pts[1], pts[2], P);

      if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

      float depth = pts[0].z * bc.x + pts[1].z * bc.y + pts[2].z * bc.z;
      int idx = x + y * width;

      if (depth < depth_buffer_[idx]) {
        VertexOutput interpolated;

        interpolated.world_position.x = v0.world_position.x * bc.x + v1.world_position.x * bc.y + v2.world_position.x * bc.z;
        interpolated.world_position.y = v0.world_position.y * bc.x + v1.world_position.y * bc.y + v2.world_position.y * bc.z;
        interpolated.world_position.z = v0.world_position.z * bc.x + v1.world_position.z * bc.y + v2.world_position.z * bc.z;

        Vec3 normal;
        normal.x = v0.normal.x * bc.x + v1.normal.x * bc.y + v2.normal.x * bc.z;
        normal.y = v0.normal.y * bc.x + v1.normal.y * bc.y + v2.normal.y * bc.z;
        normal.z = v0.normal.z * bc.x + v1.normal.z * bc.y + v2.normal.z * bc.z;
        interpolated.normal = normal.normalize();

        interpolated.texcoord.x = v0.texcoord.x * bc.x + v1.texcoord.x * bc.y + v2.texcoord.x * bc.z;
        interpolated.texcoord.y = v0.texcoord.y * bc.x + v1.texcoord.y * bc.y + v2.texcoord.y * bc.z;

        TGAColor color = shader.fragment(interpolated);
        image_.set(x, y, color);
        depth_buffer_[idx] = depth;
      }
    }
  }
}

void GraphicsRenderer::render(Model& model, IShader& shader) {
  for (int i = 0; i < image_.get_width() * image_.get_height(); i++) {
    depth_buffer_[i] = std::numeric_limits<float>::max();
  }

  for (int y = 0; y < image_.get_height(); y++) {
    for (int x = 0; x < image_.get_width(); x++) {
      image_.set(x, y, TGAColor(0, 0, 0));
    }
  }

  for (int i = 0; i < model.nfaces(); i++) {
    int verts[3], norms[3], texcoords[3];
    model.get_face(i, verts, norms, texcoords);

    VertexInput input[3];
    VertexOutput output[3];

    for (int j = 0; j < 3; j++) {
      input[j].position = model.vert(verts[j]);
      input[j].normal = (norms[j] >= 0) ? model.normal(norms[j]) : Vec3(0, 1, 0);
      input[j].texcoord = (texcoords[j] >= 0) ? model.texcoord(texcoords[j]) : Vec2(0, 0);

            output[j] = shader.vertex(input[j]);
    }

    triangle(output[0], output[1], output[2], shader);
  }
}

bool GraphicsRenderer::write_tga(const char* filename) {
  image_.flip_vertically();
  return image_.write_tga_file(filename);
}