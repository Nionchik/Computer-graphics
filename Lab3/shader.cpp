//shader.cpp
#include "shader.h"
#include "texture.h"
#include <algorithm>

PhongShader::PhongShader()
  : light_dir_(Vec3(0.5f, 1.0f, 0.5f).normalize()),
  light_color_(Vec3(1.0f, 1.0f, 1.0f)),
  view_pos_(Vec3(0.0f, 0.0f, 3.0f)),
  ambient_(Vec3(0.2f, 0.2f, 0.2f)),
  diffuse_(Vec3(0.7f, 0.7f, 0.7f)),
  specular_(Vec3(0.5f, 0.5f, 0.5f)),
  shininess_(32.0f),
  texture_(nullptr) {
}

void PhongShader::set_texture(Texture* texture) {
  texture_ = texture;
}

void PhongShader::set_matrices(const Mat4& model, const Mat4& view, const Mat4& projection) {
  model_ = model;
  view_ = view;
  projection_ = projection;
  mvp_ = projection_ * view_ * model_;
}

void PhongShader::set_light(const Vec3& direction, const Vec3& color) {
  light_dir_ = direction.normalize();
  light_color_ = color;
}

void PhongShader::set_view_position(const Vec3& pos) {
  view_pos_ = pos;
}

void PhongShader::set_material(const Vec3& ambient, const Vec3& diffuse, const Vec3& specular, float shininess) {
  ambient_ = ambient;
  diffuse_ = diffuse;
  specular_ = specular;
  shininess_ = shininess;
}

VertexOutput PhongShader::vertex(const VertexInput& in) {
  VertexOutput out;

  Vec4 world_pos = model_ * Vec4(in.position, 1.0f);
  out.clip_position = mvp_ * Vec4(in.position, 1.0f);
  out.world_position = world_pos.xyz();
  out.normal = in.normal;
  out.texcoord = in.texcoord;
  out.reciprocal_w = 1.0f / out.clip_position.w;

  return out;
}

TGAColor PhongShader::fragment(const VertexOutput& in) {
  Vec3 normal = in.normal.normalize();
  Vec3 light_dir = (-light_dir_).normalize();
  Vec3 view_dir = (view_pos_ - in.world_position).normalize();
  Vec3 reflect_dir = (normal * (2.0f * normal.dot(light_dir)) - light_dir).normalize();

  Vec3 ambient = ambient_;
  float diff = std::max(normal.dot(light_dir), 0.0f);
  Vec3 diffuse = diffuse_ * diff;
  float spec = pow(std::max(view_dir.dot(reflect_dir), 0.0f), shininess_);
  Vec3 specular = specular_ * spec;

  // Базовый цвет из текстуры
  TGAColor base_color(255, 255, 255);
  if (texture_ != nullptr) {
    base_color = texture_->sample(in.texcoord.x, in.texcoord.y);
  }

  // Преобразуем цвет текстуры в [0,1] диапазон
  Vec3 texture_color = Vec3(base_color.r / 255.0f, base_color.g / 255.0f, base_color.b / 255.0f);

  // УМНОЖАЕМ НА ОСВЕЩЕНИЕ (а не наоборот)
  Vec3 result = texture_color * (ambient + diffuse) + specular;

  result.x = std::max(0.0f, std::min(result.x, 1.0f));
  result.y = std::max(0.0f, std::min(result.y, 1.0f));
  result.z = std::max(0.0f, std::min(result.z, 1.0f));

  return TGAColor(
    static_cast<unsigned char>(result.x * 255),
    static_cast<unsigned char>(result.y * 255),
    static_cast<unsigned char>(result.z * 255)
  );
}