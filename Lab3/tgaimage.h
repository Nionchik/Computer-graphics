//tgaimage.h
#pragma once
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>

#pragma pack(push,1)
struct TGAHeader {
  char idlength;
  char colormaptype;
  char datatypecode;
  short colormaporigin;
  short colormaplength;
  char colormapdepth;
  short x_origin;
  short y_origin;
  short width;
  short height;
  char bitsperpixel;
  char imagedescriptor;
};
#pragma pack(pop)

struct TGAColor {
  unsigned char b, g, r, a;

  TGAColor() : b(0), g(0), r(0), a(255) {}
  TGAColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
    : b(b), g(g), r(r), a(a) {
  }

  TGAColor operator*(float intensity) const {
    return TGAColor(
      static_cast<unsigned char>(r * intensity),
      static_cast<unsigned char>(g * intensity),
      static_cast<unsigned char>(b * intensity),
      a
    );
  }
};

class TGAImage {
private:
  std::vector<unsigned char> data;
  int width, height;
  int bytespp;

public:
  TGAImage(int w, int h, int bpp = 4);
  bool set(int x, int y, TGAColor color);
  TGAColor get(int x, int y) const;
  bool read_tga_file(const char* filename);
  bool write_tga_file(const char* filename);
  void flip_vertically();
  int get_width() const { return width; }
  int get_height() const { return height; }
  
};