//texture.cpp
#include "texture.h"
#include <iostream>

Texture::Texture(const std::string& filename) : image_(0, 0, 0), width_(0), height_(0) {
  if (!image_.read_tga_file(filename.c_str())) {
    std::cerr << "Failed to load texture: " << filename << std::endl;
    return;
  }
  width_ = image_.get_width();
  height_ = image_.get_height();
}

TGAColor Texture::sample(float u, float v) const {
  if (width_ == 0 || height_ == 0) return TGAColor(255, 255, 255);

  // ÏÐÈÌÅÍßÅÌ ÌÀÑØÒÀÁÈÐÎÂÀÍÈÅ - ÓÌÅÍÜØÀÅÌ ÒÅÊÑÒÓÐÓ
  u = u * scale_;
  v = v * scale_;

  // Ïîâòîðÿåì òåêñòóðó (tile) åñëè êîîðäèíàòû âûõîäÿò çà [0,1]
  u = u - std::floor(u);
  v = v - std::floor(v);

  u = std::max(0.0f, std::min(1.0f, u));
  v = std::max(0.0f, std::min(1.0f, v));

  // Èñïîëüçóåì áèëèíåéíóþ ôèëüòðàöèþ
  return sample_bilinear(u, v);
}

TGAColor Texture::sample_bilinear(float u, float v) const {
  float x = u * (width_ - 1);
  float y = (1.0f - v) * (height_ - 1);

  int x0 = static_cast<int>(x);
  int y0 = static_cast<int>(y);
  int x1 = std::min(x0 + 1, width_ - 1);
  int y1 = std::min(y0 + 1, height_ - 1);

  float fx = x - x0;
  float fy = y - y0;

  TGAColor c00 = image_.get(x0, y0);
  TGAColor c10 = image_.get(x1, y0);
  TGAColor c01 = image_.get(x0, y1);
  TGAColor c11 = image_.get(x1, y1);

  TGAColor top(
    static_cast<unsigned char>(c00.r * (1 - fx) + c10.r * fx),
    static_cast<unsigned char>(c00.g * (1 - fx) + c10.g * fx),
    static_cast<unsigned char>(c00.b * (1 - fx) + c10.b * fx)
  );

  TGAColor bottom(
    static_cast<unsigned char>(c01.r * (1 - fx) + c11.r * fx),
    static_cast<unsigned char>(c01.g * (1 - fx) + c11.g * fx),
    static_cast<unsigned char>(c01.b * (1 - fx) + c11.b * fx)
  );

  return TGAColor(
    static_cast<unsigned char>(top.r * (1 - fy) + bottom.r * fy),
    static_cast<unsigned char>(top.g * (1 - fy) + bottom.g * fy),
    static_cast<unsigned char>(top.b * (1 - fy) + bottom.b * fy)
  );
}