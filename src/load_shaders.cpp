#include <cstdio>
#include <cstdlib>
#include <math.h>
#include <glad/glad.h>
#include "defines.h"
#include "c_math.h"
#include "Scene.h"
#include "SelectRes.h"

#ifdef _WIN32
#include <Windows.h>
#define ERROR_MSG(x) MessageBox(nullptr, TEXT(x), TEXT("ERROR"), MB_OK);
#else
#define ERROR_MSG(x) std::cerr << x << std::endl;
#endif

static void calculate_flag_radius();
static void build_light_camera(vec3_t scene_center, float light_distance);
static void load_sparse_raymarch();
static void load_sparse_shadow_raymarch();
static void load_full_raymarch();
static void check_shader_compile(uint32_t shader, const char *name);
static char* load_text_file(const char *path);

static vec3_t cam_pos;
static float cam_yaw;
static float cam_pitch;

static vec3_t cam_forward;
static vec3_t cam_right;
static vec3_t cam_up;

static vec3_t light_pos;
static vec3_t light_forward;
static vec3_t light_right;
static vec3_t light_up;
static vec3_t lightdir_n;

static float focal_len;

static float iFracScale;
static float iFracAng1;
static float iFracAng2;
static vec3_t iFracShift;
static vec3_t iFracCol;

static vec3_t iMarblePos;
static float iMarbleRad;
static float iFlagScale;
static vec3_t iFlagPos;
static vec3_t flag_center;
static float flag_radius;

static float sea_level;

static uint32_t sparse_raymarch_prog;
static uint32_t sparse_shadow_raymarch_prog;
static uint32_t full_raymarch_prog;

static uint32_t depth_tex;
static uint32_t shadow_tex;

static uint32_t vao;
// static uint32_t render_tex;

static int num_rays_x;
static int num_rays_y;

static int window_w;
static int window_h;

extern uint32_t fbo;
extern float iTime;

void init_globals(const Resolution* resolution) {
  focal_len = 1.2;
  lightdir_n = normalize(LIGHT_DIRECTION);
  
  const vec3_t scene_center = vec3_t{0.0f, 0.0f, 0.0f};
  build_light_camera(scene_center, 12.0f);
  
  window_w = resolution->width;
  window_h = resolution->height;
  
  num_rays_x = (window_w + STRIDE - 1) / STRIDE;
  num_rays_y = (window_h + STRIDE - 1) / STRIDE;
}

static void calculate_flag_radius() {
  vec3_t cloth_center = vec3_add(iFlagPos, vec3_scale(vec3_t{1.5f, 4.0f, 0.0f}, iFlagScale));
  vec3_t pole_center = vec3_add(iFlagPos, vec3_t{0.0f, iFlagScale * 2.4f, 0.0f});

  vec3_t cloth_size = vec3_scale(vec3_t{1.5f, 0.8f, 0.08f}, iMarbleRad);

  float cloth_radius = length(cloth_size);
  float pole_radius = iMarbleRad * 2.4f + iMarbleRad * 0.18f;

  flag_center = vec3_scale(vec3_add(cloth_center, pole_center), 0.5f);

  float r0 = length(vec3_sub(cloth_center, flag_center)) + cloth_radius;
  float r1 = length(vec3_sub(pole_center,  flag_center)) + pole_radius;

  flag_radius = fmaxf(r0, r1);
}

static void build_light_camera(vec3_t scene_center, float light_distance) {
  vec3_t forward = {-lightdir_n.x, -lightdir_n.y, -lightdir_n.z};

  vec3_t tmp = fabsf(forward.y) < 0.99f
    ? vec3_t{0.0f,1.0f,0.0f}
    : vec3_t{1.0f,0.0f,0.0f};

  vec3_t right = normalize(cross(tmp, forward));
  vec3_t up = cross(forward, right);

  light_forward = forward;
  light_right = right;
  light_up = up;

  light_pos.x = scene_center.x - forward.x * light_distance;
  light_pos.y = scene_center.y - forward.y * light_distance;
  light_pos.z = scene_center.z - forward.z * light_distance;
}

void update_uniforms(Scene& scene) {
  cam_right = vec3_t{scene.cam_mat(0, 0), scene.cam_mat(1, 0), scene.cam_mat(2, 0)};
  cam_up = vec3_t{-scene.cam_mat(0, 1), -scene.cam_mat(1, 1), -scene.cam_mat(2, 1)};
  cam_forward = vec3_t{-scene.cam_mat(0, 2), -scene.cam_mat(1, 2), -scene.cam_mat(2, 2)};
  cam_pos = vec3_t{scene.cam_mat(0, 3), scene.cam_mat(1, 3), scene.cam_mat(2, 3)};
  
  iMarblePos = scene.free_camera ?
    vec3_t{999.0f, 999.0f, 999.0f} :
    vec3_t{scene.marble_pos.x(), scene.marble_pos.y(), scene.marble_pos.z()};
  iMarbleRad = scene.marble_rad;
  iFlagScale = scene.level_copy.planet ? -scene.marble_rad : scene.marble_rad;
  iFlagPos = scene.free_camera ?
    vec3_t{-999.0f, -999.0f, -999.0f} :
    vec3_t{scene.flag_pos.x(), scene.flag_pos.y(), scene.flag_pos.z()};
  
  sea_level = scene.level_copy.water_y;
  
  iFracScale = scene.frac_params_smooth[0];
  iFracAng1 = scene.frac_params_smooth[1];
  iFracAng2 = scene.frac_params_smooth[2];
  iFracShift = vec3_t{scene.frac_params_smooth[3], scene.frac_params_smooth[4], scene.frac_params_smooth[5]};
  iFracCol = vec3_t{scene.frac_params_smooth[6], scene.frac_params_smooth[7], scene.frac_params_smooth[8]};
  
  calculate_flag_radius();
}

void create_objects(uint32_t render_tex) {
  // create the depth texture
  glGenTextures(1, &depth_tex);
  glBindTexture(GL_TEXTURE_2D, depth_tex);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, num_rays_x, num_rays_y);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  // create the shadow depth texture
  glGenTextures(1, &shadow_tex);
  glBindTexture(GL_TEXTURE_2D, shadow_tex);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  
  // vertex array object
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  
  // load the shaders
  load_sparse_raymarch();
  #if SHADOWS_ENABLED
    load_sparse_shadow_raymarch();
  #endif
  load_full_raymarch();
  
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  
  // create fixed-resolution render target
  // glGenTextures(1, &render_tex);
  // glBindTexture(GL_TEXTURE_2D, render_tex);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, window_w, window_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_tex, 0);
  
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void load_sparse_raymarch() {
  const char *cs_str = "assets/sparse_raymarch.comp";
  
  char *cs_src = load_text_file(cs_str);
  
  uint32_t cs = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(cs, 1, (const GLchar **)&cs_src, NULL);
  glCompileShader(cs);
  check_shader_compile(cs, cs_str);
  
  sparse_raymarch_prog = glCreateProgram();
  glAttachShader(sparse_raymarch_prog, cs);
  glLinkProgram(sparse_raymarch_prog);
  
  glDeleteShader(cs);
  free(cs_src);
}

static void load_sparse_shadow_raymarch() {
  const char *cs_str = "assets/sparse_shadow_raymarch.comp";
  
  char *cs_src = load_text_file(cs_str);
  
  uint32_t cs = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(cs, 1, (const GLchar **)&cs_src, NULL);
  glCompileShader(cs);
  check_shader_compile(cs, cs_str);
  
  sparse_shadow_raymarch_prog = glCreateProgram();
  glAttachShader(sparse_shadow_raymarch_prog, cs);
  glLinkProgram(sparse_shadow_raymarch_prog);
  
  glDeleteShader(cs);
  free(cs_src);
}

static void load_full_raymarch() {
  const char *vs_str = "assets/fsq.vert";
  #if SEA_ENABLED
    const char *fs_str = "assets/full_raymarch_sea.frag";
  #elif SHADOWS_ENABLED
    const char *fs_str = "assets/full_raymarch_no_sea.frag";
  #endif
  
  char *vs_src = load_text_file(vs_str);
  char *fs_src = load_text_file(fs_str);
  
  uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
  uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
  
  glShaderSource(vs, 1, (const GLchar **)&vs_src, NULL);
  glShaderSource(fs, 1, (const GLchar **)&fs_src, NULL);
  
  glCompileShader(vs);
  glCompileShader(fs);
  check_shader_compile(vs, vs_str);
  check_shader_compile(fs, fs_str);
  
  full_raymarch_prog = glCreateProgram();
  glAttachShader(full_raymarch_prog, vs);
  glAttachShader(full_raymarch_prog, fs);
  glLinkProgram(full_raymarch_prog);
  
  glDeleteShader(vs);
  glDeleteShader(fs);
  
  free(vs_src);
  free(fs_src);
}

void setup_static_uniforms() {
  glUseProgram(sparse_raymarch_prog);
  
  glUniform2i(glGetUniformLocation(sparse_raymarch_prog, "uResolution"), num_rays_x, num_rays_y);
  glUniform1i(glGetUniformLocation(sparse_raymarch_prog, "uMaxSteps"), MAX_STEPS);
  
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "depth_near"), DEPTH_NEAR);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "depth_far"), DEPTH_FAR);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "focal_len"), focal_len);
  
  glUseProgram(sparse_shadow_raymarch_prog);
  
  glUniform2i(glGetUniformLocation(sparse_shadow_raymarch_prog, "uResolution"), SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
  glUniform1i(glGetUniformLocation(sparse_shadow_raymarch_prog, "uMaxSteps"), MAX_STEPS);
  
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "light_pos"), light_pos.x, light_pos.y, light_pos.z);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "light_forward"), light_forward.x, light_forward.y, light_forward.z);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "light_right"), light_right.x, light_right.y, light_right.z);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "light_up"), light_up.x, light_up.y, light_up.z);
  
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "depth_near"), DEPTH_NEAR);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "depth_far"), DEPTH_FAR);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "focal_len"), focal_len);
  
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "ortho_scale"), ORTHO_SCALE);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "shadow_sharpness"), SHADOW_SHARPNESS);
  
  glUseProgram(full_raymarch_prog);
  
  glUniform2i(glGetUniformLocation(full_raymarch_prog, "uResolution"), window_w, window_h);
  glUniform1i(glGetUniformLocation(full_raymarch_prog, "uMaxSteps"), MAX_STEPS);
  glUniform1i(glGetUniformLocation(full_raymarch_prog, "uStride"), STRIDE);
  
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "depth_near"), DEPTH_NEAR);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "depth_far"), DEPTH_FAR);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "focal_len"), focal_len);
  
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "lightdir_n"), lightdir_n.x, lightdir_n.y, lightdir_n.z);
  
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "light_pos"), light_pos.x, light_pos.y, light_pos.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "light_forward"), light_forward.x, light_forward.y, light_forward.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "light_right"), light_right.x, light_right.y, light_right.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "light_up"), light_up.x, light_up.y, light_up.z);
  
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "ortho_scale"), ORTHO_SCALE);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "shadow_sharpness"), SHADOW_SHARPNESS);
}

void dispatch_sparse_raymarch() {
  glUseProgram(sparse_raymarch_prog);
  
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "cam_pos"), cam_pos.x, cam_pos.y, cam_pos.z);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "cam_forward"), cam_forward.x, cam_forward.y, cam_forward.z);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "cam_right"), cam_right.x, cam_right.y, cam_right.z);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "cam_up"), cam_up.x, cam_up.y, cam_up.z);
  
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "iFracScale"), iFracScale);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "iFracAng1"), iFracAng1);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "iFracAng2"), iFracAng2);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "iFracShift"), iFracShift.x, iFracShift.y, iFracShift.z);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "iFracCol"), iFracCol.x, iFracCol.y, iFracCol.z);
  
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "iMarblePos"), iMarblePos.x, iMarblePos.y, iMarblePos.z);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "iMarbleRad"), iMarbleRad);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "iFlagScale"), iFlagScale);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "iFlagPos"), iFlagPos.x, iFlagPos.y, iFlagPos.z);
  glUniform3f(glGetUniformLocation(sparse_raymarch_prog, "flag_center"), flag_center.x, flag_center.y, flag_center.z);
  glUniform1f(glGetUniformLocation(sparse_raymarch_prog, "flag_radius"), flag_radius);
  
  glBindImageTexture(0, depth_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
  glDispatchCompute((num_rays_x + 15) / 16, (num_rays_y + 15) / 16, 1);
  
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void dispatch_sparse_shadow_raymarch() {
  glUseProgram(sparse_shadow_raymarch_prog);
  
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFracScale"), iFracScale);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFracAng1"), iFracAng1);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFracAng2"), iFracAng2);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFracShift"), iFracShift.x, iFracShift.y, iFracShift.z);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFracCol"), iFracCol.x, iFracCol.y, iFracCol.z);
  
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iMarblePos"), iMarblePos.x, iMarblePos.y, iMarblePos.z);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iMarbleRad"), iMarbleRad);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFlagScale"), iFlagScale);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "iFlagPos"), iFlagPos.x, iFlagPos.y, iFlagPos.z);
  glUniform3f(glGetUniformLocation(sparse_shadow_raymarch_prog, "flag_center"), flag_center.x, flag_center.y, flag_center.z);
  glUniform1f(glGetUniformLocation(sparse_shadow_raymarch_prog, "flag_radius"), flag_radius);
  
  glBindImageTexture(0, shadow_tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
  glDispatchCompute((SHADOW_MAP_SIZE + 15) / 16, (SHADOW_MAP_SIZE + 15) / 16, 1);
  
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

void dispatch_full_raymarch() {
  glUseProgram(full_raymarch_prog);
  
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "cam_pos"), cam_pos.x, cam_pos.y, cam_pos.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "cam_forward"), cam_forward.x, cam_forward.y, cam_forward.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "cam_right"), cam_right.x, cam_right.y, cam_right.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "cam_up"), cam_up.x, cam_up.y, cam_up.z);
  
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "iFracScale"), iFracScale);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "iFracAng1"), iFracAng1);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "iFracAng2"), iFracAng2);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "iFracShift"), iFracShift.x, iFracShift.y, iFracShift.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "iFracCol"), iFracCol.x, iFracCol.y, iFracCol.z);
  
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "iMarblePos"), iMarblePos.x, iMarblePos.y, iMarblePos.z);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "iMarbleRad"), iMarbleRad);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "iFlagScale"), iFlagScale);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "iFlagPos"), iFlagPos.x, iFlagPos.y, iFlagPos.z);
  glUniform3f(glGetUniformLocation(full_raymarch_prog, "flag_center"), flag_center.x, flag_center.y, flag_center.z);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "flag_radius"), flag_radius);
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "sea_level"), sea_level);
  
  glUniform1f(glGetUniformLocation(full_raymarch_prog, "iTime"), iTime);
  
  glBindTextureUnit(0, depth_tex);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void check_shader_compile(uint32_t shader, const char *name) {
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    char *log = (char*)malloc(len);
    glGetShaderInfoLog(shader, len, NULL, log);
    // printf("failed to compile shader: %s\n%s\n", name, log);
    char msg[4096];
    snprintf(msg, sizeof(msg),
      "failed to compile shader: %s\n\n%s",
      name, log
    );
    ERROR_MSG(msg);
    free(log);
    exit(1);
  }
}

static char* load_text_file(const char *path) {
  FILE *file = fopen(path, "rb");
  
  if (!file) {
    // printf("can't load the shaders\n");
    char msg[4096];
    snprintf(msg, sizeof(msg),
      "can't load the shader: %s",
      path
    );
    exit(1);
  }
  
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  char *buffer = (char*)malloc(size + 1);
  if (!buffer) {
    fclose(file);
    // printf("can't allocate memory\n");
    ERROR_MSG("can't allocate memory");
    exit(1);
  }
  
  fread(buffer, 1, size, file);
  buffer[size] = 0;
  
  fclose(file);
  return buffer;
}

void GLAPIENTRY MessageCallback(
  GLenum source,
  GLenum type,
  uint32_t id,
  GLenum severity,
  GLsizei length,
  const GLchar* message,
  const void* userParam) {
  fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
         (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
          type, severity, message);
}