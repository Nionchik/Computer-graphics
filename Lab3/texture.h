//texture.h
#pragma once
#include "tgaimage.h"
#include "geometry.h"
#include <string>

class Texture {
private:
  TGAImage image_;
  int width_, height_;
  float scale_ = 0.1f;

public:
  Texture(const std::string& filename);
  TGAColor sample(float u, float v) const;
  TGAColor sample_bilinear(float u, float v) const;
  TGAColor sample(const Vec2& uv) const { return sample(uv.x, uv.y); }
  int get_width() const { return width_; }
  int get_height() const { return height_; }


};