//tgaimage.cpp
#include "tgaimage.h"

TGAImage::TGAImage(int w, int h, int bpp) : width(w), height(h), bytespp(bpp) {
  data.resize(w * h * bytespp, 0);
}

bool TGAImage::read_tga_file(const char* filename) {
  std::ifstream in(filename, std::ios::binary);
  if (!in) return false;

  TGAHeader header;
  in.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!in) return false;

  width = header.width;
  height = header.height;
  bytespp = header.bitsperpixel / 8;

  if (width <= 0 || height <= 0 || (bytespp != 3 && bytespp != 4)) {
    return false;
  }

  size_t nbytes = width * height * bytespp;
  data.resize(nbytes);

  if (header.datatypecode == 2 || header.datatypecode == 3) {
    in.read(reinterpret_cast<char*>(data.data()), nbytes);
    if (!in) return false;
  }
  else {
    return false; // Поддержка только несжатых TGA
  }

  if (!(header.imagedescriptor & 0x20)) {
    flip_vertically();
  }

  return true;
}

bool TGAImage::set(int x, int y, TGAColor color) {
  if (x < 0 || y < 0 || x >= width || y >= height) return false;
  memcpy(&data[(x + y * width) * bytespp], &color, bytespp);
  return true;
}

TGAColor TGAImage::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= width || y >= height) return TGAColor();
  TGAColor color;
  memcpy(&color, &data[(x + y * width) * bytespp], bytespp);
  return color;
}

bool TGAImage::write_tga_file(const char* filename) {
  TGAHeader header;
  memset(&header, 0, sizeof(header));
  header.datatypecode = 2;
  header.width = width;
  header.height = height;
  header.bitsperpixel = bytespp * 8;
  header.imagedescriptor = 0x20;

  std::ofstream out(filename, std::ios::binary);
  if (!out) return false;

  out.write((char*)&header, sizeof(header));
  out.write((char*)data.data(), data.size());
  out.close();
  return true;
}

void TGAImage::flip_vertically() {
  for (int y = 0; y < height / 2; y++) {
    for (int x = 0; x < width; x++) {
      TGAColor c1 = get(x, y);
      TGAColor c2 = get(x, height - 1 - y);
      set(x, y, c2);
      set(x, height - 1 - y, c1);
    }
  }
}