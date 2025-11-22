//model.cpp
#include "model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <limits>
#include <cmath>

Model::Model(const std::string& filename) {
  std::ifstream in(filename);
  if (!in.is_open()) {
    std::cerr << "Cannot open file: " << filename << std::endl;
    return;
  }

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string type;
    iss >> type;

    if (type == "v") {
      float x, y, z;
      iss >> x >> y >> z;
      vertices_.push_back(Vec3(x, y, z));
    }
    else if (type == "vn") {
      float x, y, z;
      iss >> x >> y >> z;
      normals_.push_back(Vec3(x, y, z).normalize());
    }
    else if (type == "vt") {
      float u, v;
      iss >> u >> v;
      texcoords_.push_back(Vec2(u, 1.0f - v));
    }
    else if (type == "f") {
      Face face;
      for (int i = 0; i < 3; i++) {
        std::string token;
        iss >> token;

        size_t pos1 = token.find('/');
        size_t pos2 = token.find('/', pos1 + 1);

        face.vertex_ids[i] = std::stoi(token.substr(0, pos1)) - 1;

        if (pos1 != std::string::npos && pos2 != pos1 + 1) {
          face.texcoord_ids[i] = std::stoi(token.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
        }
        else {
          face.texcoord_ids[i] = -1;
        }

        if (pos2 != std::string::npos) {
          face.normal_ids[i] = std::stoi(token.substr(pos2 + 1)) - 1;
        }
        else if (pos1 != std::string::npos) {
          face.normal_ids[i] = std::stoi(token.substr(pos1 + 2)) - 1;
        }
        else {
          face.normal_ids[i] = -1;
        }
      }
      faces_.push_back(face);
    }
  }

  // ГЕНЕРИРУЕМ UV КООРДИНАТЫ ЕСЛИ ИХ НЕТ
  if (texcoords_.empty()) {

    for (size_t i = 0; i < vertices_.size(); i++) {
      const Vec3& vertex = vertices_[i];

      // Сферические координаты для UV
      Vec3 normalized = vertex.normalize();
      float u = 0.5f + std::atan2(normalized.z, normalized.x) / (2.0f * 3.14159265f);
      float v = 0.5f - std::asin(normalized.y) / 3.14159265f;

      u = std::max(0.0f, std::min(1.0f, u));
      v = std::max(0.0f, std::min(1.0f, v));

      texcoords_.push_back(Vec2(u, v));
    }

    // Обновляем грани чтобы использовали сгенерированные UV
    for (auto& face : faces_) {
      for (int j = 0; j < 3; j++) {
        face.texcoord_ids[j] = face.vertex_ids[j];
      }
    }

    std::cout << "Generated " << texcoords_.size() << " UV coordinates" << std::endl;
  }

  std::cout << "Loaded model: " << vertices_.size() << " vertices, "
    << faces_.size() << " faces, " << normals_.size() << " normals, "
    << texcoords_.size() << " texture coordinates" << std::endl;
}

void Model::get_face(int i, int* vertices, int* normals, int* texcoords) const {
  const Face& face = faces_[i];
  for (int j = 0; j < 3; j++) {
    vertices[j] = face.vertex_ids[j];
    normals[j] = face.normal_ids[j];
    texcoords[j] = face.texcoord_ids[j];

    // Гарантируем что индексы в пределах массива
    if (texcoords[j] < 0 || texcoords[j] >= (int)texcoords_.size()) {
      texcoords[j] = 0;
    }
    if (normals[j] < 0 || normals[j] >= (int)normals_.size()) {
      normals[j] = 0;
    }
  }
}

Vec3 Model::vert(int i) const {
  if (i >= 0 && i < (int)vertices_.size()) {
    return vertices_[i];
  }
  return Vec3(0, 0, 0);
}

Vec3 Model::normal(int i) const {
  if (i >= 0 && i < (int)normals_.size()) {
    return normals_[i];
  }
  return Vec3(0, 1, 0);
}

Vec2 Model::texcoord(int i) const {
  if (i >= 0 && i < (int)texcoords_.size()) {
    return texcoords_[i];
  }
  return Vec2(0, 0);
}

int Model::nverts() const { return (int)vertices_.size(); }
int Model::nnormals() const { return (int)normals_.size(); }
int Model::ntexcoords() const { return (int)texcoords_.size(); }
int Model::nfaces() const { return (int)faces_.size(); }