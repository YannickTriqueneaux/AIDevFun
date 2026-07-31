#include "Engine/Graphics/Renderer3D.h"

#include "Engine/Graphics/ShaderProgram.h"
#include "raylib.h"

#include <cassert>

namespace {
::Color ToRaylibColor(Engine::Color color) {
  return {color.red, color.green, color.blue, color.alpha};
}

::Vector3 ToRaylibVector(Engine::Vector3 vector) {
  return {vector.x, vector.y, vector.z};
}

::Camera3D ToRaylibCamera(const Engine::Camera3D &camera) {
  return {ToRaylibVector(camera.position), ToRaylibVector(camera.target),
          ToRaylibVector(camera.up), camera.fieldOfView,
          camera.projection == Engine::CameraProjection::Perspective
              ? CAMERA_PERSPECTIVE
              : CAMERA_ORTHOGRAPHIC};
}
} // namespace

namespace Engine {
void Renderer3D::Begin(const Camera3D &camera) {
  assert(!active_);
  BeginMode3D(ToRaylibCamera(camera));
  active_ = true;
}

void Renderer3D::End() {
  assert(active_);
  assert(!shaderActive_);
  EndMode3D();
  active_ = false;
}

void Renderer3D::BeginShader(const ShaderProgram &shader) {
  assert(active_);
  assert(!shaderActive_);
  assert(shader.IsValid());
  shader.BeginUse();
  shaderActive_ = true;
}

void Renderer3D::EndShader() {
  assert(active_);
  assert(shaderActive_);
  EndShaderMode();
  shaderActive_ = false;
}

void Renderer3D::DrawGrid(int slices, float spacing) {
  assert(active_);
  ::DrawGrid(slices, spacing);
}

void Renderer3D::DrawLine(Vector3 start, Vector3 end, Color color) {
  assert(active_);
  DrawLine3D(ToRaylibVector(start), ToRaylibVector(end), ToRaylibColor(color));
}

void Renderer3D::DrawCube(Vector3 position, Vector3 size, Color color) {
  assert(active_);
  DrawCubeV(ToRaylibVector(position), ToRaylibVector(size),
            ToRaylibColor(color));
}

void Renderer3D::DrawCubeOutline(Vector3 position, Vector3 size, Color color) {
  assert(active_);
  DrawCubeWiresV(ToRaylibVector(position), ToRaylibVector(size),
                 ToRaylibColor(color));
}

void Renderer3D::DrawSphere(Vector3 center, float radius, Color color) {
  assert(active_);
  ::DrawSphere(ToRaylibVector(center), radius, ToRaylibColor(color));
}
} // namespace Engine
