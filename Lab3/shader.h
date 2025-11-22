//shader.h
#pragma once
#include "geometry.h"
#include "tgaimage.h"

// Добавляем forward declaration
class Texture;

struct VertexInput {
  Vec3 position;
  Vec3 normal;
  Vec2 texcoord;
};

struct VertexOutput {
  Vec4 clip_position;
  Vec3 world_position;
  Vec3 normal;
  Vec2 texcoord;
  float reciprocal_w = 1.0f; 

  // Конструктор для полной инициализации
  VertexOutput() : clip_position(), world_position(), normal(), texcoord(), reciprocal_w(1.0f) {}
};

class IShader {
public:
  virtual ~IShader() = default;
  virtual VertexOutput vertex(const VertexInput& in) = 0;
  virtual TGAColor fragment(const VertexOutput& in) = 0;
};

class PhongShader : public IShader {
private:
  Mat4 model_;
  Mat4 view_;
  Mat4 projection_;
  Mat4 mvp_;

  Vec3 light_dir_;
  Vec3 light_color_;
  Vec3 view_pos_;

  Vec3 ambient_;
  Vec3 diffuse_;
  Vec3 specular_;
  float shininess_;

  Texture* texture_ = nullptr;  // Указатель на текстуру

public:
  PhongShader();

  void set_matrices(const Mat4& model, const Mat4& view, const Mat4& projection);
  void set_light(const Vec3& direction, const Vec3& color);
  void set_view_position(const Vec3& pos);
  void set_material(const Vec3& ambient, const Vec3& diffuse, const Vec3& specular, float shininess);
  void set_texture(Texture* texture); 

  VertexOutput vertex(const VertexInput& in) override;
  TGAColor fragment(const VertexOutput& in) override;
};