// SPDX-License-Identifier: GPL-3.0-only
// Compile/link generated shaders on the actual Android GLES driver. This does not claim gameplay rendering.
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

static GLuint Compile(const std::filesystem::path& file, GLenum stage) {
  if (std::filesystem::file_size(file) > 4 * 1024 * 1024) return 0;
  std::ifstream in(file, std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(in)), {});
  const char* source = text.c_str();
  GLuint shader = glCreateShader(stage);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char message[4096]{};
    glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
    std::fprintf(stderr, "%s: %s\n", file.filename().string().c_str(), message);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!eglInitialize(display, nullptr, nullptr)) return 3;
  const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
  EGLConfig config{};
  EGLint count = 0;
  if (!eglChooseConfig(display, attributes, &config, 1, &count) || !count) return 4;
  const EGLint pixels[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  EGLSurface surface = eglCreatePbufferSurface(display, config, pixels);
  const EGLint version[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
  EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
  if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) return 5;
  std::printf("Driver: %s; %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
  GLint units = 0, outputs = 0, ubo = 0, texture = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units);
  glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &outputs);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &ubo);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture);
  std::printf("Limits: combined units %d, vertex output components %d, UBO bytes %d, texture size %d\n", units, outputs, ubo, texture);
  unsigned shaders = 0, pairs = 0, failures = 0;
  for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
    const auto extension = entry.path().extension();
    if (extension != ".vert" && extension != ".frag") continue;
    GLuint shader = Compile(entry.path(), extension == ".vert" ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    if (shader) { ++shaders; glDeleteShader(shader); } else ++failures;
  }
  for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(argv[1]) / "pares")) {
    if (entry.path().extension() != ".frag") continue;
    auto vertex = entry.path(); vertex.replace_extension(".vert");
    GLuint vs = Compile(vertex, GL_VERTEX_SHADER), ps = Compile(entry.path(), GL_FRAGMENT_SHADER);
    if (!vs || !ps) {
      if (vs) glDeleteShader(vs);
      if (ps) glDeleteShader(ps);
      ++failures; continue;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs); glAttachShader(program, ps);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
      char message[4096]{};
      glGetProgramInfoLog(program, sizeof(message), nullptr, message);
      std::fprintf(stderr, "Pair %s: %s\n", entry.path().stem().string().c_str(), message);
      ++failures;
    } else ++pairs;
    glDeleteProgram(program); glDeleteShader(vs); glDeleteShader(ps);
  }
  const GLenum error = glGetError();
  std::printf("GLES driver validation: %u shaders compiled, %u pairs linked, %u failures, GL error %u\n", shaders, pairs, failures, error);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display, context); eglDestroySurface(display, surface); eglTerminate(display);
  return !shaders || !pairs || failures || error ? 1 : 0;
}
