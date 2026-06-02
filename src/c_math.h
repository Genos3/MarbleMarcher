#define DEG2RAD(x) ((x) * 0.01745329251f)

typedef struct {
  float x, y, z;
} vec3_t;

// math helpers

static inline vec3_t vec3_add(vec3_t a, vec3_t b) {
  return vec3_t{a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline vec3_t vec3_sub(vec3_t a, vec3_t b) {
  return vec3_t{a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline vec3_t vec3_mul(vec3_t a, vec3_t b) {
  return vec3_t{a.x * b.x, a.y * b.y, a.z * b.z};
}

static inline vec3_t vec3_div(vec3_t a, vec3_t b) {
  return vec3_t{a.x / b.x, a.y / b.y, a.z / b.z};
}

static inline vec3_t vec3_scale(vec3_t v, float s) {
  return vec3_t{v.x * s, v.y * s, v.z * s};
}

static inline vec3_t vec3_div_scalar(vec3_t a, float b) {
  float inv = 1.0f / b;
  return vec3_t{a.x * inv, a.y * inv, a.z * inv};
}

static inline float dot(vec3_t a, vec3_t b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float length(vec3_t v) {
  return sqrtf(dot(v, v));
}

static inline vec3_t normalize(vec3_t v) {
  float inv_len = 1.0f / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  return {v.x * inv_len, v.y * inv_len, v.z * inv_len};
}

static inline vec3_t cross(vec3_t a, vec3_t b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}