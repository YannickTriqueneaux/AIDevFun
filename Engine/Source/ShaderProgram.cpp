#include "Engine/Graphics/ShaderProgram.h"

#include "raylib.h"

#include <string>
struct Engine::ShaderProgram::Implementation {
  ::Shader shader{};
};

namespace {
int GetLocation(::Shader shader, std::string_view name) {
  const std::string terminatedName(name);
  return GetShaderLocation(shader, terminatedName.c_str());
}

const char *SourceOrNull(const std::string &source) {
  return source.empty() ? nullptr : source.c_str();
}
} // namespace

namespace Engine {
ShaderProgram::ShaderProgram() : implementation_(new Implementation()) {}

ShaderProgram::~ShaderProgram() {
  Unload();
  delete implementation_;
}

ShaderProgram::ShaderProgram(ShaderProgram &&other) noexcept
    : implementation_(other.implementation_) {
  other.implementation_ = nullptr;
}

ShaderProgram &ShaderProgram::operator=(ShaderProgram &&other) noexcept {
  if (this != &other) {
    Unload();
    delete implementation_;
    implementation_ = other.implementation_;
    other.implementation_ = nullptr;
  }
  return *this;
}

bool ShaderProgram::LoadFromMemory(std::string_view vertexSource,
                                   std::string_view fragmentSource) {
  Unload();
  if (!implementation_) {
    implementation_ = new Implementation();
  }

  const std::string vertex(vertexSource);
  const std::string fragment(fragmentSource);
  implementation_->shader =
      LoadShaderFromMemory(SourceOrNull(vertex), SourceOrNull(fragment));
  return IsValid();
}

void ShaderProgram::Unload() {
  if (IsValid()) {
    UnloadShader(implementation_->shader);
    implementation_->shader = {};
  }
}

bool ShaderProgram::IsValid() const {
  return implementation_ && ::IsShaderValid(implementation_->shader);
}

void ShaderProgram::BeginUse() const {
  if (IsValid()) {
    BeginShaderMode(implementation_->shader);
  }
}

void ShaderProgram::SetFloat(std::string_view name, float value) const {
  if (IsValid()) {
    SetShaderValue(implementation_->shader,
                   GetLocation(implementation_->shader, name), &value,
                   SHADER_UNIFORM_FLOAT);
  }
}

void ShaderProgram::SetInt(std::string_view name, int value) const {
  if (IsValid()) {
    SetShaderValue(implementation_->shader,
                   GetLocation(implementation_->shader, name), &value,
                   SHADER_UNIFORM_INT);
  }
}

void ShaderProgram::SetVector2(std::string_view name, Vector2 value) const {
  if (IsValid()) {
    const float values[]{value.x, value.y};
    SetShaderValue(implementation_->shader,
                   GetLocation(implementation_->shader, name), values,
                   SHADER_UNIFORM_VEC2);
  }
}

void ShaderProgram::SetVector3(std::string_view name, Vector3 value) const {
  if (IsValid()) {
    const float values[]{value.x, value.y, value.z};
    SetShaderValue(implementation_->shader,
                   GetLocation(implementation_->shader, name), values,
                   SHADER_UNIFORM_VEC3);
  }
}

void ShaderProgram::SetColor(std::string_view name, Color value) const {
  if (IsValid()) {
    constexpr float ByteToUnit = 1.0f / 255.0f;
    const float values[]{value.red * ByteToUnit, value.green * ByteToUnit,
                         value.blue * ByteToUnit, value.alpha * ByteToUnit};
    SetShaderValue(implementation_->shader,
                   GetLocation(implementation_->shader, name), values,
                   SHADER_UNIFORM_VEC4);
  }
}
} // namespace Engine
