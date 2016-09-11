#include "gfx/exceptions.h"
#include "gfx/game_window.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

// gfx::Camera* camera;
// GLFWwindow* window;
// GLuint program;
// glm::mat4 perspective_projection;

gfx::GameWindow::GameWindow(int width, int height, std::string vertex_path,
    std::string fragment_path, gfx::Camera* camera, float fov, gfx::Color color) : camera{camera},
    window{nullptr}, field_of_view{fov}, program{0} {
  gfx::GameWindow::InitializeGameWindow(width, height, color);
  program = gfx::GameWindow::LinkProgram(vertex_path, fragment_path);
  if (program == 0) {
    throw gfx::GameWindowCannotBeInitializedException();
  }
}

gfx::GameWindow::GameWindow(int width, int height, std::string vertex_path,
    std::string fragment_path, gfx::Camera* camera) : camera{camera}, window{nullptr},
    field_of_view{45.0f}, program{0} {
  gfx::GameWindow::InitializeGameWindow(width, height, gfx::Color(0.0f, 0.0f, 0.0f));
  program = gfx::GameWindow::LinkProgram(vertex_path, fragment_path);
  if (program == 0) {
    throw gfx::GameWindowCannotBeInitializedException();
  }
}

bool gfx::GameWindow::IsRunning() {
  return !glfwWindowShouldClose(window);
}

void gfx::GameWindow::InitializeGameWindow(int width, int height, gfx::Color color) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // This is needed to work on OS X.
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  // Create the window and make it the current context.
  window = glfwCreateWindow(width, height, "OpenGL", nullptr, nullptr);
  if (window == nullptr) {
    std::cout << "Failed to create OpenGL context." << std::endl;
    glfwTerminate();
    throw gfx::GameWindowCannotBeInitializedException();
  }
  glfwMakeContextCurrent(window);
  gladLoadGL();

  glEnable(GL_DEPTH_TEST);
  gfx::GameWindow::SetBufferClearColor(color);
  gfx::GameWindow::UpdateDimensions(width, height);
}

GLuint gfx::GameWindow::CompileShader(std::string path, GLenum shader_type) {
  std::ifstream ifs(path);
  std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
  if (content.size() == 0) {
    return 0;
  }

  GLuint shader = glCreateShader(shader_type);
  const char *c_content = content.c_str();
  glShaderSource(shader, 1, &c_content, NULL);
  glCompileShader(shader);

  // Handle errors.
  GLint success;
  GLchar info_log[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

  if(!success) {
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    std::cout << "Shader compilation of \'" << path << "\' failed:\n" << info_log << std::endl;
    return 0;
  }
  return shader;
}

GLuint gfx::GameWindow::LinkProgram(std::string vertex_path, std::string fragment_path) {
  GLuint vertex_shader = CompileShader(vertex_path, GL_VERTEX_SHADER);
  GLuint frag_shader = CompileShader(fragment_path, GL_FRAGMENT_SHADER);
  if (vertex_shader == 0 || frag_shader == 0) {
    return 0;
  }
  GLuint linked_program = glCreateProgram();
  glAttachShader(linked_program, vertex_shader);
  glAttachShader(linked_program, frag_shader);
  glLinkProgram(linked_program);
  glDeleteShader(vertex_shader);
  glDeleteShader(frag_shader);

  // Handle errors.
  GLint success;
  GLchar info_log[512];
  glGetProgramiv(linked_program, GL_LINK_STATUS, &success);
  if(!success) {
    glGetProgramInfoLog(linked_program, 512, NULL, info_log);
    std::cout << "Failed to link shader program:\n" << info_log << std::endl;
    return 0;
  }

  if (linked_program == 0) {
    std::cout << "Failed to create shader program." << std::endl;
    glfwTerminate();
    return 0;
  }
  return linked_program;
}

void gfx::GameWindow::SetBufferClearColor(gfx::Color color) {
  glClearColor(color.r, color.g, color.b, color.a);
}

// Updates the dimensions of the window and recalculates the perspective projection.
void gfx::GameWindow::UpdateDimensions(int width, int height) {
  glfwSetWindowSize(window, width, height);
  int real_width, real_height;
  glfwGetFramebufferSize(window, &real_width, &real_height);
  glViewport(0, 0, real_width, real_height);
  gfx::GameWindow::UpdatePerspectiveProjection(real_width, real_height);
}

// Updates the field of view and recalculates the perspective projection.
void gfx::GameWindow::UpdateFieldOfView(float fov) {
  field_of_view = fov;
  int real_width, real_height;
  glfwGetFramebufferSize(window, &real_width, &real_height);
  gfx::GameWindow::UpdatePerspectiveProjection(real_width, real_height);
}

// Polls the GLFW window for events and invokes the proper callbacks.
void gfx::GameWindow::PollForEvents() {
  glfwPollEvents();
}

// This must be called every frame before drawing any ModelInstances to the screen. This sets
// the state of the OpenGL context so that we can begin drawing the next frame. After this is
// called, the caller shouldn't change the game state and should only call RenderModel until
// the render is completed with FinishRender.
void gfx::GameWindow::PrepareRender() {
  glUseProgram(program);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GLint view_location = glGetUniformLocation(program, "view");
  glUniformMatrix4fv(view_location, 1, GL_FALSE, glm::value_ptr(camera->GetViewTransform()));
  GLint projection_location = glGetUniformLocation(program, "projection");
  glUniformMatrix4fv(projection_location, 1, GL_FALSE, glm::value_ptr(perspective_projection));
}

// Draws a given ModelInstance. Note that this must be called in between a PrepareRender and a
// a FinishRender.
void gfx::GameWindow::RenderModel(gfx::ModelInstance* model_instance) {
  model_instance->Draw(program);
}

// Complets the rendering started by PrepareRender. This swaps the buffer so the rendered image
// can actually be seen.
void gfx::GameWindow::FinishRender() {
  glfwSwapBuffers(window);
}

// Updates the perspective projection with the width, height, and field of view.
void gfx::GameWindow::UpdatePerspectiveProjection(int width, int height) {
  perspective_projection = glm::perspective(glm::radians(field_of_view),
      (GLfloat)width / (GLfloat)height, 0.1f, 100.0f);
}
