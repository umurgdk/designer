#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <GLFW/glfw3.h>

#include <cairo/cairo.h>

#define WIDTH  640
#define HEIGHT 480

bool should_render = true;
bool running = true;

int fb_width = WIDTH;
int fb_height = HEIGHT;

GLuint scr_vbo, scr_vao, scr_ebo;
GLuint scr_tex;
GLuint scr_vert, scr_frag, scr_prog;
static const float QUAD_VERTICES[] = {
  // Position         Tex Coords
  1.0f, 1.0f, 0.0f,   1.0f, 1.0f,   // Top right      
  1.0f, -1.0f, 0.0f,   1.0f, 0.0f,   // Bottom right
  -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,   // Bottom left
  -1.0f, 1.0f, 0.0f,   0.0f, 1.0f,   // Top left
};
static const unsigned int QUAD_INDICES[] = {
  0, 1, 3,
  1, 2, 3,
};

static const char *shader_vert =
  "#version 330 core\n"
  "layout (location = 0) in vec3 aPos;"
  "layout (location = 1) in vec2 aTexCoord;"
  "out vec2 texCoord;"
  "void main()"
  "{"
  "    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);"
  "    texCoord = aTexCoord;"
  "}\n\0";

static const char *shader_frag = 
  "#version 330\n"
  "out vec4 vCol;"
  "in vec2 texCoord;"
  "uniform sampler2D tex;"
  "void main() {"
  "    vCol = texture(tex, texCoord);"
  "}\n\0";

cairo_surface_t *scr_surf;
cairo_t *ctx;

void error_callback(int err, const char *description);
void key_callback(GLFWwindow *win, int key, int scancode, int action, int mods);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

cairo_t*  create_cairo(int width, int height, cairo_surface_t **surf);

void p_gl_error()
{
  GLenum res = glGetError();
  if ( res == GL_INVALID_ENUM ) fprintf(stderr, "GL_INVALID_ENUM\n");
  if ( res == GL_INVALID_VALUE ) fprintf(stderr, "GL_INVALID_VALUE\n");
  if ( res == GL_INVALID_OPERATION ) fprintf(stderr, "GL_INVALID_OPERATION\n");
  if ( res == GL_OUT_OF_MEMORY ) fprintf(stderr, "GL_OUT_OF_MEMORY\n");
}

int main(void) {
  if (!glfwInit()) {
    fprintf(stderr, "failed to init glfw\n");
    exit(1);
  }

  glfwSetErrorCallback(error_callback);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE_
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *win = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
  if (win == NULL) {
    fprintf(stderr, "failed to create window\n");
    glfwTerminate();
    exit(1);
  }

  glfwMakeContextCurrent(win);
  glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);

  // get version info
  const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
  const GLubyte* version = glGetString(GL_VERSION); // version as a string
  printf("Renderer: %s\n", renderer);
  printf("OpenGL version supported %s\n", version);

  int success = 0;
  char info_log[512];

  // Create simple shader
  scr_vert = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(scr_vert, 1, &shader_vert, NULL);
  glCompileShader(scr_vert);
  glGetShaderiv(scr_vert, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(scr_vert, 512, NULL, info_log);
    fprintf(stderr, "FATAL main(): couldn't compile vert shader: %s\n", info_log);
    glfwTerminate();
    exit(1);
  }

  scr_frag = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(scr_frag, 1, &shader_frag, NULL);
  glCompileShader(scr_frag);
  glGetShaderiv(scr_frag, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(scr_frag, 512, NULL, info_log);
    fprintf(stderr, "FATAL main(): couldn't compile frag shader: %s\n", info_log);
    glDeleteShader(scr_vert);
    glfwTerminate();
    exit(1);
  }

  scr_prog = glCreateProgram();
  glAttachShader(scr_prog, scr_vert);
  glAttachShader(scr_prog, scr_frag);
  glLinkProgram(scr_prog);
  glGetProgramiv(scr_prog, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(scr_prog, 512, NULL, info_log);
    fprintf(stderr, "FATAL main(): couldn't link shader: %s\n", info_log);
    glDeleteShader(scr_vert);
    glDeleteShader(scr_frag);
    glfwTerminate();
    exit(1);
  }

  printf("shader program linked\n");

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // We don't need shaders anymore program is enough
  glDeleteShader(scr_vert);
  glDeleteShader(scr_frag);

  // Create a quad for screen surface
  glGenBuffers(1, &scr_vbo);
  glGenBuffers(1, &scr_ebo);
  glGenVertexArrays(1, &scr_vao);

  glBindVertexArray(scr_vao);

  glBindBuffer(GL_ARRAY_BUFFER, scr_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scr_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_INDICES), QUAD_INDICES, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(0 * sizeof(float)));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);


  // Create screen cairo context
  ctx = create_cairo(WIDTH, HEIGHT, &scr_surf);
  if (ctx == NULL) {
    fprintf(stderr, "FATAL main(): couldn't create cairo\n");
    glfwTerminate();
    exit(1);
  }

  cairo_set_source_rgba(ctx, 1.0, 1.0, 1.0, 0.0);
  cairo_rectangle(ctx, 0, 0, 800, 600);
  cairo_fill(ctx);

  unsigned char *scr_tex_data = cairo_image_surface_get_data(scr_surf);

  // Create an opengl texture from cairo buffer
  glGenTextures(1, &scr_tex);
  glBindTexture(GL_TEXTURE_2D, scr_tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, scr_tex_data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glClearColor(1.0f, 0.4f, 1.0f, 1.0f);
  while (running && !glfwWindowShouldClose(win)) {
    // Render code
    if (should_render) {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      cairo_set_source_rgba(ctx, 1.0, 1.0, 1.0, 0.0);
      cairo_rectangle(ctx, 0, 0, fb_width, fb_height);
      cairo_fill(ctx);

      cairo_set_line_width(ctx, 9);
      cairo_translate(ctx, fb_width/2.0, fb_height/2.0);
      cairo_arc(ctx, 0, 0, 50, 0, 2 * M_PI);

      cairo_set_source_rgb(ctx, 1.0, 1.0, 1.0);
      cairo_stroke_preserve(ctx);

      cairo_set_source_rgb(ctx, 0.2, 1.0, 0.7);
      cairo_fill(ctx);

      scr_tex_data = cairo_image_surface_get_data(scr_surf);

      glBindTexture(GL_TEXTURE_2D, scr_tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_width, fb_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, scr_tex_data);

      glUseProgram(scr_prog);
      glBindTexture(GL_TEXTURE_2D, scr_tex);
      glBindVertexArray(scr_vao);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);

      glfwSwapBuffers(win);

      should_render = false;
      printf("rendered\n");
    }

    glfwWaitEvents();
  }

  glDeleteVertexArrays(1, &scr_vao);
  glDeleteBuffers(1, &scr_vbo);
  glDeleteBuffers(1, &scr_ebo);
  glfwTerminate();
  return 0;
}

void error_callback(int err, const char *description) {
  fprintf(stderr, "ERR: %s\n", description);
}

void key_callback(GLFWwindow *win, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE)
    running = false;
  printf("key event: %c\n", scancode);
}

cairo_t*  create_cairo(int width, int height, cairo_surface_t **surf) {
  *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status(*surf) != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "FATAL create_cairo(): couldn't create cairo surface\n");
    return NULL;
  }

  cairo_t *ctx = cairo_create(*surf);
  if (cairo_status(ctx) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(*surf);
    *surf = NULL;
    fprintf(stderr, "FATAL create_cairo(): couldn't create cairo context\n");
    return NULL;
  }

  return ctx;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  fb_width = width;
  fb_height = height;

  glViewport(0, 0, width, height);
  /*printf("framebuffer resized to: %dx%d\n", width, height);*/

  cairo_surface_destroy(scr_surf);
  cairo_destroy(ctx);
  ctx = create_cairo(fb_width, fb_height, &scr_surf);
  should_render = true;
}
