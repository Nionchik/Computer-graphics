// main.cpp
#include <iostream>
#include "geometry.h"
#include "model.h"
#include "camera.h"
#include "shader.h"
#include "graphics.h"
#include "texture.h" 
#include <cmath>

int main() {
  const int width = 800;
  const int height = 800;

  try {
    Model model("sponza.obj");
    if (model.nfaces() == 0) {
      std::cerr << "Failed to load model!" << std::endl;
      return 1;
    }

    // Вычисляем bounding box модели
    Vec3 min_vert(1e9, 1e9, 1e9);
    Vec3 max_vert(-1e9, -1e9, -1e9);

    for (int i = 0; i < model.nverts(); i++) {
      Vec3 v = model.vert(i);
      min_vert.x = std::min(min_vert.x, v.x);
      min_vert.y = std::min(min_vert.y, v.y);
      min_vert.z = std::min(min_vert.z, v.z);
      max_vert.x = std::max(max_vert.x, v.x);
      max_vert.y = std::max(max_vert.y, v.y);
      max_vert.z = std::max(max_vert.z, v.z);
    }

    Vec3 center = (min_vert + max_vert) * 0.5f;
    Vec3 size = max_vert - min_vert;


    // Масштабируем модель, если она слишком большая
    float max_dim = std::max(size.x, std::max(size.y, size.z));
    float scale_factor = 1.0f;

    if (max_dim > 100.0f) {
      scale_factor = 10.0f / max_dim;
      std::cout << "Model is large. Applying scale factor: " << scale_factor << std::endl;
    }
    else if (max_dim < 1.0f) {
      scale_factor = 1.0f / max_dim;
      std::cout << "Model is small. Applying scale factor: " << scale_factor << std::endl;
    }

    // Загружаем текстуру
    Texture texture("head_diffuse.tga");
    if (texture.get_width() == 0 || texture.get_height() == 0) {
      std::cerr << "Warning: Failed to load texture or texture is empty!" << std::endl;
    }
    else {
      std::cout << "Texture loaded: " << texture.get_width() << "x" << texture.get_height() << std::endl;
    }


    Vec3 scaled_center = center * scale_factor;

    float distance = 0.0f;

    if (max_dim > 0) {

      distance = max_dim * 2.0f * scale_factor;
    }
    else {
      distance = 10.0f;
    }


    float angle_rad = 45.0f * 3.14159265f / 180.0f; 
    float sin45 = std::sin(angle_rad);
    float cos45 = std::cos(angle_rad);

    Vec3 camera_pos = scaled_center + Vec3(
      distance * cos45,     
      distance * sin45,      
      distance * cos45       
    );


    Vec3 light_direction = (scaled_center - camera_pos).normalize();



    Camera camera(
      camera_pos,                          
      scaled_center,                     
      Vec3(0, 1, 0),                     
      60.0f,                              
      (float)width / height,
      distance * 0.1f,                    
      distance * 10.0f                     
    );

    PhongShader shader;

 
    Mat4 model_matrix = Mat4::scale(Vec3(scale_factor, scale_factor, scale_factor)) *
      Mat4::translation(-center);

    shader.set_matrices(
      model_matrix,
      camera.view_matrix(),
      camera.projection_matrix()
    );

    // НАСТРОЙКА ОСВЕЩЕНИЯ
    shader.set_light(light_direction, Vec3(1.0f, 1.0f, 1.0f));
    shader.set_view_position(camera.position);

    if (texture.get_width() > 0 && texture.get_height() > 0) {
      shader.set_texture(&texture);
    }

    // Настройка материала
    shader.set_material(
      Vec3(0.3f, 0.3f, 0.3f),    // Ambient
      Vec3(0.8f, 0.8f, 0.8f),    // Diffuse
      Vec3(0.5f, 0.5f, 0.5f),    // Specular
      32.0f                      // Shininess
    );

    GraphicsRenderer renderer(width, height);

    std::cout << "\n=== Rendering ===" << std::endl;
    renderer.render(model, shader);

    if (renderer.write_tga("output.tga")) {
      std::cout << "Rendered image saved to output.tga" << std::endl;
    }
    else {
      std::cerr << "Failed to save image!" << std::endl;
      return 1;
    }

  }
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}