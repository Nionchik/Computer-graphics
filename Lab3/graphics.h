#pragma once
#include "tgaimage.h"
#include "geometry.h"
#include "model.h"
#include "shader.h"
#include <vector>

class GraphicsRenderer {
private:
  TGAImage image_;
  std::vector<float> depth_buffer_;

  Vec3 barycentric(const Vec3& A, const Vec3& B, const Vec3& C, const Vec3& P);
  void triangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, IShader& shader);

public:
  GraphicsRenderer(int width, int height);
  void render(Model& model, IShader& shader);
  bool write_tga(const char* filename);
};