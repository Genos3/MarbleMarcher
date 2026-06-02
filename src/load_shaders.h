#pragma once
#include <glad/glad.h>
#include "SelectRes.h"

void init_globals(const Resolution* resolution);
void update_uniforms(Scene& scene);
void create_objects(uint32_t render_tex);
void setup_static_uniforms();
void dispatch_sparse_raymarch();
void dispatch_sparse_shadow_raymarch();
void dispatch_full_raymarch();
void GLAPIENTRY MessageCallback(GLenum source, GLenum type, uint32_t id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);