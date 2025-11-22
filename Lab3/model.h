// model.h
#pragma once
#include <vector>
#include <string>
#include "geometry.h"

class Model {
private:
  std::vector<Vec3> vertices_;
  std::vector<Vec3> normals_;
  std::vector<Vec2> texcoords_;

  struct Face {
    int vertex_ids[3];
    int normal_ids[3];
    int texcoord_ids[3];
  };
  std::vector<Face> faces_;

public:
  Model(const std::string& filename);

  int nverts() const;
  int nnormals() const;
  int ntexcoords() const;
  int nfaces() const;

  Vec3 vert(int i) const;
  Vec3 normal(int i) const;
  Vec2 texcoord(int i) const;

  void get_face(int i, int* vertices, int* normals, int* texcoords) const;
};