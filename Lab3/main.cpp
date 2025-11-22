//main.cpp
#include <iostream>
#include "geometry.h"
#include "model.h"
#include "camera.h"
#include "shader.h"
#include "graphics.h"
#include "texture.h" 

int main() {
  const int width = 800;
  const int height = 800;

  try {
    Model model("head.obj");
    if (model.nfaces() == 0) {
      std::cerr << "Failed to load model!" << std::endl;
      return 1;
    }

    // Загружаем текстуру
    Texture texture("head_diffuse.tga");

    Camera camera(
      Vec3(0, -0.3f, 8.0f),
      Vec3(0, -0.3f, 0),
      Vec3(0, 1, 0),
      45.0f,
      (float)width / height,
      0.1f,
      100.0f
    );

    PhongShader shader;
    shader.set_matrices(
      Mat4::identity(),
      camera.view_matrix(),
      camera.projection_matrix()
    );
    shader.set_light(Vec3(0, 0.2f, -1).normalize(), Vec3(1.2f, 1.2f, 1.2f));
    shader.set_view_position(camera.position);
    shader.set_texture(&texture);  // Передаем указатель на текстуру
    shader.set_material(
      Vec3(0.3f, 0.25f, 0.2f),
      Vec3(0.9f, 0.7f, 0.6f),
      Vec3(0.1f, 0.08f, 0.06f),
      32.0f
    );

    GraphicsRenderer renderer(width, height);
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