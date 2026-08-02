#include "Engine/Graphics/VectorShape.h"

#include "Engine/Core/Memory.h"
#include "Engine/Graphics/Renderer2D.h"

#include "raylib.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr std::size_t NoParent = static_cast<std::size_t>(-1);

struct Affine {
  float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, x = 0.0f, y = 0.0f;
};

Affine operator*(const Affine &left, const Affine &right) {
  return {left.a * right.a + left.c * right.b,
          left.b * right.a + left.d * right.b,
          left.a * right.c + left.c * right.d,
          left.b * right.c + left.d * right.d,
          left.a * right.x + left.c * right.y + left.x,
          left.b * right.x + left.d * right.y + left.y};
}

Engine::Vector2 Apply(const Affine &m, Engine::Vector2 p) {
  return {m.a * p.x + m.c * p.y + m.x, m.b * p.x + m.d * p.y + m.y};
}

Affine Translation(float x, float y) { return {1, 0, 0, 1, x, y}; }
Affine Scale(float x, float y) { return {x, 0, 0, y, 0, 0}; }
Affine Rotation(float degrees) {
  const float radians = degrees * Pi / 180.0f;
  const float cosine = std::cos(radians), sine = std::sin(radians);
  return {cosine, sine, -sine, cosine, 0, 0};
}

Affine ToAffine(const Engine::VectorShapeTransform &transform) {
  return Translation(transform.translation.x, transform.translation.y) *
         Rotation(transform.rotationDegrees) *
         Scale(transform.scale.x, transform.scale.y);
}

Matrix ToMatrix(const Affine &m) {
  Matrix result{};
  result.m0 = m.a;
  result.m1 = m.b;
  result.m4 = m.c;
  result.m5 = m.d;
  result.m10 = 1.0f;
  result.m12 = m.x;
  result.m13 = m.y;
  result.m15 = 1.0f;
  return result;
}

struct Vertex {
  Engine::Vector2 position;
  Engine::Color color;
};

struct Group {
  std::string id;
  std::size_t parent = NoParent;
  Affine transform;
  std::vector<Vertex> triangles;
  Mesh mesh{};
  bool uploaded = false;
};

struct Resource {
  std::uint32_t id = 0;
  Engine::Vector2 viewBoxOrigin{};
  Engine::Vector2 viewBoxSize{};
  std::vector<Group> groups;
  std::unordered_map<std::string, std::size_t> groupLookup;
  Material material{};
  bool materialLoaded = false;

  ~Resource() {
    for (Group &group : groups) {
      if (group.uploaded)
        UnloadMesh(group.mesh);
    }
    if (materialLoaded)
      UnloadMaterial(material);
  }
};

std::atomic<std::uint32_t> NextResourceID = 1;

using Attributes = std::unordered_map<std::string, std::string>;
struct Style {
  std::optional<Engine::Color> fill = Engine::Color{255, 255, 255, 255};
  std::optional<Engine::Color> stroke;
  float strokeWidth = 1.0f;
  float opacity = 1.0f;
};

void SkipSeparators(std::string_view text, std::size_t &cursor) {
  while (cursor < text.size() &&
         (std::isspace(static_cast<unsigned char>(text[cursor])) ||
          text[cursor] == ','))
    ++cursor;
}

bool Number(std::string_view text, std::size_t &cursor, float &value) {
  SkipSeparators(text, cursor);
  if (cursor >= text.size())
    return false;
  const char *begin = text.data() + cursor;
  char *end = nullptr;
  value = std::strtof(begin, &end);
  if (end == begin || !std::isfinite(value))
    return false;
  cursor += static_cast<std::size_t>(end - begin);
  return true;
}

float AttributeFloat(const Attributes &attributes, std::string_view name,
                     float fallback = 0.0f) {
  const auto found = attributes.find(std::string(name));
  if (found == attributes.end())
    return fallback;
  std::size_t cursor = 0;
  float value = fallback;
  return Number(found->second, cursor, value) ? value : fallback;
}

std::optional<Engine::Color> ParseColor(std::string_view text) {
  if (text == "none")
    return std::nullopt;
  static const std::unordered_map<std::string_view, Engine::Color> names{
      {"black", {0, 0, 0, 255}},      {"white", {255, 255, 255, 255}},
      {"red", {255, 0, 0, 255}},      {"green", {0, 128, 0, 255}},
      {"blue", {0, 0, 255, 255}},     {"yellow", {255, 255, 0, 255}},
      {"cyan", {0, 255, 255, 255}},   {"magenta", {255, 0, 255, 255}},
      {"gray", {128, 128, 128, 255}}, {"grey", {128, 128, 128, 255}},
      {"transparent", {0, 0, 0, 0}}};
  if (auto found = names.find(text); found != names.end())
    return found->second;
  if (text.empty() || text.front() != '#')
    return std::nullopt;
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  Engine::Color color{};
  if (text.size() == 4) {
    const int r = nibble(text[1]), g = nibble(text[2]), b = nibble(text[3]);
    if (r < 0 || g < 0 || b < 0)
      return std::nullopt;
    color = {static_cast<unsigned char>(r * 17),
             static_cast<unsigned char>(g * 17),
             static_cast<unsigned char>(b * 17), 255};
  } else if (text.size() == 7 || text.size() == 9) {
    unsigned value = 0;
    const auto result =
        std::from_chars(text.data() + 1, text.data() + text.size(), value, 16);
    if (result.ec != std::errc{})
      return std::nullopt;
    if (text.size() == 7)
      color = {static_cast<unsigned char>(value >> 16),
               static_cast<unsigned char>(value >> 8),
               static_cast<unsigned char>(value), 255};
    else
      color = {static_cast<unsigned char>(value >> 24),
               static_cast<unsigned char>(value >> 16),
               static_cast<unsigned char>(value >> 8),
               static_cast<unsigned char>(value)};
  } else {
    return std::nullopt;
  }
  return color;
}

Affine ParseTransform(std::string_view text) {
  Affine result;
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    SkipSeparators(text, cursor);
    const std::size_t nameStart = cursor;
    while (cursor < text.size() &&
           std::isalpha(static_cast<unsigned char>(text[cursor])))
      ++cursor;
    const std::string_view name = text.substr(nameStart, cursor - nameStart);
    while (cursor < text.size() &&
           std::isspace(static_cast<unsigned char>(text[cursor])))
      ++cursor;
    if (cursor >= text.size() || text[cursor++] != '(')
      break;
    const std::size_t close = text.find(')', cursor);
    if (close == std::string_view::npos)
      break;
    const std::string_view values = text.substr(cursor, close - cursor);
    std::size_t valueCursor = 0;
    float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    Affine next;
    if (name == "translate" && Number(values, valueCursor, a)) {
      if (!Number(values, valueCursor, b))
        b = 0;
      next = Translation(a, b);
    } else if (name == "scale" && Number(values, valueCursor, a)) {
      if (!Number(values, valueCursor, b))
        b = a;
      next = Scale(a, b);
    } else if (name == "rotate" && Number(values, valueCursor, a)) {
      if (Number(values, valueCursor, b) && Number(values, valueCursor, c))
        next = Translation(b, c) * Rotation(a) * Translation(-b, -c);
      else
        next = Rotation(a);
    } else if (name == "matrix" && Number(values, valueCursor, a) &&
               Number(values, valueCursor, b) &&
               Number(values, valueCursor, c) &&
               Number(values, valueCursor, d) &&
               Number(values, valueCursor, e) &&
               Number(values, valueCursor, f)) {
      next = {a, b, c, d, e, f};
    }
    result = result * next;
    cursor = close + 1;
  }
  return result;
}

bool ParseAttributes(std::string_view text, Attributes &attributes) {
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    while (cursor < text.size() &&
           std::isspace(static_cast<unsigned char>(text[cursor])))
      ++cursor;
    if (cursor >= text.size() || text[cursor] == '/')
      break;
    const std::size_t nameStart = cursor;
    while (cursor < text.size() &&
           (std::isalnum(static_cast<unsigned char>(text[cursor])) ||
            text[cursor] == '-' || text[cursor] == ':'))
      ++cursor;
    if (nameStart == cursor)
      return false;
    const std::string name(text.substr(nameStart, cursor - nameStart));
    while (cursor < text.size() &&
           std::isspace(static_cast<unsigned char>(text[cursor])))
      ++cursor;
    if (cursor >= text.size() || text[cursor++] != '=')
      return false;
    while (cursor < text.size() &&
           std::isspace(static_cast<unsigned char>(text[cursor])))
      ++cursor;
    if (cursor >= text.size() || (text[cursor] != '\'' && text[cursor] != '"'))
      return false;
    const char quote = text[cursor++];
    const std::size_t end = text.find(quote, cursor);
    if (end == std::string_view::npos)
      return false;
    attributes.emplace(name, std::string(text.substr(cursor, end - cursor)));
    cursor = end + 1;
  }
  return true;
}

Style ResolveStyle(const Style &parent, const Attributes &attributes) {
  Style style = parent;
  if (auto found = attributes.find("fill"); found != attributes.end())
    style.fill = ParseColor(found->second);
  if (auto found = attributes.find("stroke"); found != attributes.end())
    style.stroke = ParseColor(found->second);
  style.strokeWidth =
      AttributeFloat(attributes, "stroke-width", style.strokeWidth);
  style.opacity *=
      std::clamp(AttributeFloat(attributes, "opacity", 1.0f), 0.0f, 1.0f);
  return style;
}

float Cross(Engine::Vector2 a, Engine::Vector2 b, Engine::Vector2 c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

void AddTriangle(Group &group, Engine::Vector2 a, Engine::Vector2 b,
                 Engine::Vector2 c, Engine::Color color,
                 const Affine &transform) {
  group.triangles.push_back({Apply(transform, a), color});
  group.triangles.push_back({Apply(transform, b), color});
  group.triangles.push_back({Apply(transform, c), color});
}

void FillPolygon(Group &group, std::vector<Engine::Vector2> points,
                 Engine::Color color, const Affine &transform) {
  if (points.size() > 2 && points.front().x == points.back().x &&
      points.front().y == points.back().y)
    points.pop_back();
  if (points.size() < 3)
    return;
  float area = 0;
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto &a = points[i], &b = points[(i + 1) % points.size()];
    area += a.x * b.y - b.x * a.y;
  }
  std::vector<std::size_t> indices(points.size());
  for (std::size_t i = 0; i < indices.size(); ++i)
    indices[i] = i;
  const float orientation = area >= 0 ? 1.0f : -1.0f;
  auto inside = [](Engine::Vector2 p, Engine::Vector2 a, Engine::Vector2 b,
                   Engine::Vector2 c) {
    const float c1 = Cross(a, b, p), c2 = Cross(b, c, p), c3 = Cross(c, a, p);
    return (c1 >= 0 && c2 >= 0 && c3 >= 0) || (c1 <= 0 && c2 <= 0 && c3 <= 0);
  };
  for (std::size_t guard = 0;
       indices.size() > 2 && guard < points.size() * points.size(); ++guard) {
    bool clipped = false;
    for (std::size_t i = 0; i < indices.size(); ++i) {
      const std::size_t previous =
          indices[(i + indices.size() - 1) % indices.size()];
      const std::size_t current = indices[i],
                        next = indices[(i + 1) % indices.size()];
      if (Cross(points[previous], points[current], points[next]) *
              orientation <=
          0)
        continue;
      bool contains = false;
      for (std::size_t candidate : indices)
        if (candidate != previous && candidate != current &&
            candidate != next &&
            inside(points[candidate], points[previous], points[current],
                   points[next])) {
          contains = true;
          break;
        }
      if (contains)
        continue;
      AddTriangle(group, points[previous], points[current], points[next], color,
                  transform);
      indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
      clipped = true;
      break;
    }
    if (!clipped)
      break;
  }
}

void StrokePolyline(Group &group, const std::vector<Engine::Vector2> &points,
                    bool closed, float width, Engine::Color color,
                    const Affine &transform) {
  const std::size_t segments =
      closed ? points.size() : points.size() - (points.empty() ? 0 : 1);
  for (std::size_t i = 0; i < segments; ++i) {
    const auto a = points[i], b = points[(i + 1) % points.size()];
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0)
      continue;
    const Engine::Vector2 normal{-dy * width / (2 * length),
                                 dx * width / (2 * length)};
    const Engine::Vector2 p0{a.x + normal.x, a.y + normal.y},
        p1{b.x + normal.x, b.y + normal.y};
    const Engine::Vector2 p2{b.x - normal.x, b.y - normal.y},
        p3{a.x - normal.x, a.y - normal.y};
    AddTriangle(group, p0, p1, p2, color, transform);
    AddTriangle(group, p0, p2, p3, color, transform);
  }
}

bool PathPoints(std::string_view data, std::vector<Engine::Vector2> &points,
                bool &closed) {
  std::size_t cursor = 0;
  char command = 0;
  Engine::Vector2 current{}, start{};
  while (cursor < data.size()) {
    SkipSeparators(data, cursor);
    if (cursor >= data.size())
      break;
    if (std::isalpha(static_cast<unsigned char>(data[cursor])))
      command = data[cursor++];
    if (command == 'Z' || command == 'z') {
      closed = true;
      current = start;
      command = 0;
      continue;
    }
    const bool relative = std::islower(static_cast<unsigned char>(command));
    const char upper =
        static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
    float x = 0, y = 0;
    if (upper == 'M' || upper == 'L') {
      if (!Number(data, cursor, x) || !Number(data, cursor, y))
        return false;
      if (relative) {
        x += current.x;
        y += current.y;
      }
      current = {x, y};
      if (upper == 'M') {
        start = current;
        command = relative ? 'l' : 'L';
      }
      points.push_back(current);
    } else if (upper == 'H') {
      if (!Number(data, cursor, x))
        return false;
      current.x = relative ? current.x + x : x;
      points.push_back(current);
    } else if (upper == 'V') {
      if (!Number(data, cursor, y))
        return false;
      current.y = relative ? current.y + y : y;
      points.push_back(current);
    } else {
      return false;
    }
  }
  return !points.empty();
}

std::vector<Engine::Vector2> PointsAttribute(std::string_view text) {
  std::vector<Engine::Vector2> points;
  std::size_t cursor = 0;
  float x = 0, y = 0;
  while (Number(text, cursor, x) && Number(text, cursor, y))
    points.push_back({x, y});
  return points;
}

class SvgParser {
public:
  bool Parse(std::string_view source, Resource &resource, std::string &error) {
    source_ = source;
    resource_ = &resource;
    error_ = &error;
    resource.groups.push_back({"root"});
    resource.groupLookup.emplace("root", 0);
    groupStack_.push_back(0);
    styleStack_.push_back({});
    std::size_t cursor = 0;
    while (true) {
      const std::size_t open = source.find('<', cursor);
      if (open == std::string_view::npos)
        break;
      if (source.substr(open, 4) == "<!--") {
        const std::size_t close = source.find("-->", open + 4);
        if (close == std::string_view::npos)
          return Fail("Unterminated SVG comment.");
        cursor = close + 3;
        continue;
      }
      const std::size_t close = source.find('>', open + 1);
      if (close == std::string_view::npos)
        return Fail("Unterminated SVG tag.");
      std::string_view body = source.substr(open + 1, close - open - 1);
      cursor = close + 1;
      if (body.empty() || body.front() == '?' || body.front() == '!')
        continue;
      bool closing = body.front() == '/';
      if (closing)
        body.remove_prefix(1);
      bool selfClosing = !body.empty() && body.back() == '/';
      if (selfClosing)
        body.remove_suffix(1);
      std::size_t nameEnd = 0;
      while (nameEnd < body.size() &&
             !std::isspace(static_cast<unsigned char>(body[nameEnd])))
        ++nameEnd;
      const std::string name(body.substr(0, nameEnd));
      if (closing) {
        if ((name == "g" || name == "svg") && groupStack_.size() > 1) {
          groupStack_.pop_back();
          styleStack_.pop_back();
        }
        continue;
      }
      Attributes attributes;
      if (!ParseAttributes(body.substr(nameEnd), attributes))
        return Fail("Invalid SVG attributes.");
      if (!Start(name, attributes, selfClosing))
        return false;
    }
    if (!sawSvg_)
      return Fail("The document has no svg root.");
    return resource.viewBoxSize.x > 0 && resource.viewBoxSize.y > 0;
  }

private:
  bool Start(const std::string &name, const Attributes &attributes,
             bool selfClosing) {
    if (name == "script" || name == "style" || name == "image" ||
        name == "use" || name == "foreignObject")
      return Fail("Unsupported or unsafe SVG element: " + name);
    if (name == "svg") {
      sawSvg_ = true;
      if (auto found = attributes.find("viewBox"); found != attributes.end()) {
        std::size_t c = 0;
        if (!Number(found->second, c, resource_->viewBoxOrigin.x) ||
            !Number(found->second, c, resource_->viewBoxOrigin.y) ||
            !Number(found->second, c, resource_->viewBoxSize.x) ||
            !Number(found->second, c, resource_->viewBoxSize.y))
          return Fail("Invalid viewBox.");
      } else {
        resource_->viewBoxSize = {AttributeFloat(attributes, "width"),
                                  AttributeFloat(attributes, "height")};
      }
      if (!selfClosing) {
        groupStack_.push_back(0);
        styleStack_.push_back(ResolveStyle(styleStack_.back(), attributes));
      }
      return true;
    }
    if (name == "g") {
      const std::string id =
          attributes.contains("id")
              ? attributes.at("id")
              : "group_" + std::to_string(resource_->groups.size());
      if (resource_->groupLookup.contains(id))
        return Fail("Duplicate SVG group id: " + id);
      Group group{id, groupStack_.back()};
      if (auto found = attributes.find("transform"); found != attributes.end())
        group.transform = ParseTransform(found->second);
      const std::size_t index = resource_->groups.size();
      resource_->groups.push_back(std::move(group));
      resource_->groupLookup.emplace(id, index);
      if (!selfClosing) {
        groupStack_.push_back(index);
        styleStack_.push_back(ResolveStyle(styleStack_.back(), attributes));
      }
      return true;
    }
    const bool shape = name == "rect" || name == "circle" ||
                       name == "ellipse" || name == "line" ||
                       name == "polyline" || name == "polygon" ||
                       name == "path";
    if (!shape)
      return Fail("Unsupported SVG element: " + name);
    const Style style = ResolveStyle(styleStack_.back(), attributes);
    const auto withOpacity = [&style](std::optional<Engine::Color> color) {
      if (color)
        color->alpha = static_cast<unsigned char>(color->alpha * style.opacity);
      return color;
    };
    const auto fill = withOpacity(style.fill);
    const auto stroke = withOpacity(style.stroke);
    Affine transform;
    if (auto found = attributes.find("transform"); found != attributes.end())
      transform = ParseTransform(found->second);
    std::vector<Engine::Vector2> points;
    bool closed = false;
    if (name == "rect") {
      const float x = AttributeFloat(attributes, "x"),
                  y = AttributeFloat(attributes, "y");
      const float w = AttributeFloat(attributes, "width"),
                  h = AttributeFloat(attributes, "height");
      points = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
      closed = true;
    } else if (name == "circle" || name == "ellipse") {
      const float cx = AttributeFloat(attributes, "cx"),
                  cy = AttributeFloat(attributes, "cy");
      const float rx = name == "circle" ? AttributeFloat(attributes, "r")
                                        : AttributeFloat(attributes, "rx");
      const float ry = name == "circle" ? rx : AttributeFloat(attributes, "ry");
      for (int i = 0; i < 32; ++i) {
        const float angle = 2 * Pi * i / 32;
        points.push_back(
            {cx + std::cos(angle) * rx, cy + std::sin(angle) * ry});
      }
      closed = true;
    } else if (name == "line") {
      points = {
          {AttributeFloat(attributes, "x1"), AttributeFloat(attributes, "y1")},
          {AttributeFloat(attributes, "x2"), AttributeFloat(attributes, "y2")}};
    } else if (name == "polyline" || name == "polygon") {
      if (!attributes.contains("points"))
        return Fail("Shape has no points.");
      points = PointsAttribute(attributes.at("points"));
      closed = name == "polygon";
    } else {
      if (!attributes.contains("d") ||
          !PathPoints(attributes.at("d"), points, closed))
        return Fail("Unsupported or invalid path data. Use M, L, H, V, and Z "
                    "commands.");
    }
    Group &group = resource_->groups[groupStack_.back()];
    if (closed && fill)
      FillPolygon(group, points, *fill, transform);
    if (stroke && points.size() >= 2)
      StrokePolyline(group, points, closed, style.strokeWidth, *stroke,
                     transform);
    return true;
  }

  bool Fail(std::string message) {
    *error_ = std::move(message);
    return false;
  }
  std::string_view source_;
  Resource *resource_ = nullptr;
  std::string *error_ = nullptr;
  bool sawSvg_ = false;
  std::vector<std::size_t> groupStack_;
  std::vector<Style> styleStack_;
};

struct Keyframe {
  float time = 0;
  Engine::VectorShapeTransform transform;
};
Engine::VectorShapeTransform Interpolate(const Keyframe &a, const Keyframe &b,
                                         float time) {
  const float t =
      b.time <= a.time
          ? 0
          : std::clamp((time - a.time) / (b.time - a.time), 0.0f, 1.0f);
  Engine::VectorShapeTransform result;
  result.translation = {
      a.transform.translation.x +
          (b.transform.translation.x - a.transform.translation.x) * t,
      a.transform.translation.y +
          (b.transform.translation.y - a.transform.translation.y) * t};
  result.scale = {
      a.transform.scale.x + (b.transform.scale.x - a.transform.scale.x) * t,
      a.transform.scale.y + (b.transform.scale.y - a.transform.scale.y) * t};
  float delta = std::fmod(b.transform.rotationDegrees -
                              a.transform.rotationDegrees + 540.0f,
                          360.0f) -
                180.0f;
  result.rotationDegrees = a.transform.rotationDegrees + delta * t;
  result.opacity =
      a.transform.opacity + (b.transform.opacity - a.transform.opacity) * t;
  return result;
}
} // namespace

namespace Engine {
struct VectorShape::Implementation {
  std::shared_ptr<Resource> resource;
  std::string error;
};
struct VectorShapePose::Implementation {
  std::shared_ptr<Resource> resource;
  std::vector<VectorShapeTransform> transforms;
};
struct VectorShapeAnimation::Implementation {
  float duration = 0;
  std::shared_ptr<Resource> resource;
  std::vector<std::vector<Keyframe>> tracks;
};

VectorShape::VectorShape()
    : implementation_(NEW_MEMORY(Implementation).release()) {}
VectorShape::~VectorShape() { DELETE_MEMORY(implementation_); }
VectorShape::VectorShape(VectorShape &&other) noexcept
    : implementation_(std::exchange(other.implementation_, nullptr)) {}
VectorShape &VectorShape::operator=(VectorShape &&other) noexcept {
  if (this != &other) {
    DELETE_MEMORY(implementation_);
    implementation_ = std::exchange(other.implementation_, nullptr);
  }
  return *this;
}
bool VectorShape::LoadFromSvg(std::string_view source) {
  if (!implementation_)
    implementation_ = NEW_MEMORY(Implementation).release();
  implementation_->error.clear();
  auto resource = std::make_shared<Resource>();
  resource->id = NextResourceID.fetch_add(1, std::memory_order_relaxed);
  if (resource->id == 0)
    resource->id = NextResourceID.fetch_add(1, std::memory_order_relaxed);
  SvgParser parser;
  if (!parser.Parse(source, *resource, implementation_->error)) {
    implementation_->resource.reset();
    return false;
  }
  implementation_->resource = std::move(resource);
  return true;
}
bool VectorShape::Upload() {
  if (!IsValid()) {
    if (implementation_)
      implementation_->error = "Load a valid SVG before Upload().";
    return false;
  }
  if (!implementation_->resource->materialLoaded) {
    implementation_->resource->material = LoadMaterialDefault();
    implementation_->resource->materialLoaded = true;
  }
  for (Group &group : implementation_->resource->groups) {
    if (group.uploaded || group.triangles.empty())
      continue;
    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(group.triangles.size());
    mesh.triangleCount = mesh.vertexCount / 3;
    mesh.vertices = static_cast<float *>(
        MemAlloc(static_cast<unsigned>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.colors = static_cast<unsigned char *>(
        MemAlloc(static_cast<unsigned>(mesh.vertexCount * 4)));
    for (int i = 0; i < mesh.vertexCount; ++i) {
      mesh.vertices[i * 3] = group.triangles[i].position.x;
      mesh.vertices[i * 3 + 1] = group.triangles[i].position.y;
      mesh.vertices[i * 3 + 2] = 0;
      mesh.colors[i * 4] = group.triangles[i].color.red;
      mesh.colors[i * 4 + 1] = group.triangles[i].color.green;
      mesh.colors[i * 4 + 2] = group.triangles[i].color.blue;
      mesh.colors[i * 4 + 3] = group.triangles[i].color.alpha;
    }
    UploadMesh(&mesh, false);
    group.mesh = mesh;
    group.uploaded = true;
  }
  return true;
}
void VectorShape::Unload() {
  if (implementation_)
    implementation_->resource.reset();
}
bool VectorShape::IsValid() const {
  return implementation_ && implementation_->resource;
}
bool VectorShape::IsUploaded() const {
  if (!IsValid())
    return false;
  return std::all_of(
      implementation_->resource->groups.begin(),
      implementation_->resource->groups.end(),
      [](const Group &g) { return g.triangles.empty() || g.uploaded; });
}
Vector2 VectorShape::GetViewBoxSize() const {
  return IsValid() ? implementation_->resource->viewBoxSize : Vector2{};
}
std::size_t VectorShape::GetGroupCount() const {
  return IsValid() ? implementation_->resource->groups.size() : 0;
}
std::size_t VectorShape::GetTriangleCount() const {
  if (!IsValid())
    return 0;
  std::size_t count = 0;
  for (const Group &g : implementation_->resource->groups)
    count += g.triangles.size() / 3;
  return count;
}
bool VectorShape::HasGroup(std::string_view id) const {
  return ResolveGroup(id).IsValid();
}
VectorShapeGroupID VectorShape::ResolveGroup(std::string_view name) const {
  if (!IsValid())
    return {};
  const auto found =
      implementation_->resource->groupLookup.find(std::string(name));
  if (found == implementation_->resource->groupLookup.end())
    return {};
  return {static_cast<std::uint32_t>(found->second),
          implementation_->resource->id};
}
const std::string &VectorShape::GetLastError() const {
  static const std::string empty;
  return implementation_ ? implementation_->error : empty;
}
void VectorShape::Draw(const VectorShapeDrawParameters &parameters) const {
  if (!IsUploaded())
    return;
  const Resource &resource = *implementation_->resource;
  const Vector2 size{
      parameters.size.x == 0 ? resource.viewBoxSize.x : parameters.size.x,
      parameters.size.y == 0 ? resource.viewBoxSize.y : parameters.size.y};
  Affine root =
      Translation(parameters.position.x, parameters.position.y) *
      Rotation(parameters.rotationDegrees) *
      Scale(size.x / resource.viewBoxSize.x, size.y / resource.viewBoxSize.y) *
      Translation(-resource.viewBoxOrigin.x - resource.viewBoxSize.x / 2,
                  -resource.viewBoxOrigin.y - resource.viewBoxSize.y / 2);
  std::vector<Affine> worlds(resource.groups.size());
  std::vector<float> opacities(resource.groups.size(), 1.0f);
  for (std::size_t i = 0; i < resource.groups.size(); ++i) {
    const Group &group = resource.groups[i];
    Affine local = group.transform;
    float opacity = 1;
    if (parameters.pose && parameters.pose->implementation_ &&
        parameters.pose->implementation_->resource.get() == &resource) {
      local = local * ToAffine(parameters.pose->implementation_->transforms[i]);
      opacity = std::clamp(
          parameters.pose->implementation_->transforms[i].opacity, 0.0f, 1.0f);
    }
    if (group.parent == NoParent)
      worlds[i] = root * local;
    else {
      worlds[i] = worlds[group.parent] * local;
      opacity *= opacities[group.parent];
    }
    opacities[i] = opacity;
    if (!group.uploaded)
      continue;
    Material &material = implementation_->resource->material;
    material.maps[MATERIAL_MAP_DIFFUSE].color = {
        parameters.tint.red, parameters.tint.green, parameters.tint.blue,
        static_cast<unsigned char>(parameters.tint.alpha * opacity)};
    DrawMesh(group.mesh, material, ToMatrix(worlds[i]));
  }
}

VectorShapePose::VectorShapePose(const VectorShape &shape)
    : implementation_(NEW_MEMORY(Implementation).release()) {
  if (shape.IsValid()) {
    implementation_->resource = shape.implementation_->resource;
    implementation_->transforms.resize(shape.GetGroupCount());
  }
}
VectorShapePose::~VectorShapePose() { DELETE_MEMORY(implementation_); }
VectorShapePose::VectorShapePose(VectorShapePose &&other) noexcept
    : implementation_(std::exchange(other.implementation_, nullptr)) {}
VectorShapePose &VectorShapePose::operator=(VectorShapePose &&other) noexcept {
  if (this != &other) {
    DELETE_MEMORY(implementation_);
    implementation_ = std::exchange(other.implementation_, nullptr);
  }
  return *this;
}
void VectorShapePose::Reset() {
  if (implementation_)
    std::fill(implementation_->transforms.begin(),
              implementation_->transforms.end(), VectorShapeTransform{});
}
bool VectorShapePose::SetGroupTransform(VectorShapeGroupID id,
                                        VectorShapeTransform transform) {
  if (!implementation_ || !implementation_->resource ||
      id.resource != implementation_->resource->id ||
      id.index >= implementation_->transforms.size())
    return false;
  implementation_->transforms[id.index] = transform;
  return true;
}
bool VectorShapePose::GetGroupTransform(VectorShapeGroupID id,
                                        VectorShapeTransform &transform) const {
  if (!implementation_ || !implementation_->resource ||
      id.resource != implementation_->resource->id ||
      id.index >= implementation_->transforms.size())
    return false;
  transform = implementation_->transforms[id.index];
  return true;
}

VectorShapeAnimation::VectorShapeAnimation(const VectorShape &shape)
    : implementation_(NEW_MEMORY(Implementation).release()) {
  if (shape.IsValid()) {
    implementation_->resource = shape.implementation_->resource;
    implementation_->tracks.resize(shape.GetGroupCount());
  }
}
VectorShapeAnimation::~VectorShapeAnimation() {
  DELETE_MEMORY(implementation_);
}
VectorShapeAnimation::VectorShapeAnimation(
    VectorShapeAnimation &&other) noexcept
    : implementation_(std::exchange(other.implementation_, nullptr)) {}
VectorShapeAnimation &
VectorShapeAnimation::operator=(VectorShapeAnimation &&other) noexcept {
  if (this != &other) {
    DELETE_MEMORY(implementation_);
    implementation_ = std::exchange(other.implementation_, nullptr);
  }
  return *this;
}
void VectorShapeAnimation::SetDuration(float seconds) {
  implementation_->duration = std::max(0.0f, seconds);
}
float VectorShapeAnimation::GetDuration() const {
  return implementation_ ? implementation_->duration : 0;
}
bool VectorShapeAnimation::AddKeyframe(VectorShapeGroupID groupId,
                                       float timeSeconds,
                                       VectorShapeTransform transform) {
  if (!implementation_ || !implementation_->resource ||
      groupId.resource != implementation_->resource->id ||
      groupId.index >= implementation_->tracks.size())
    return false;
  auto &track = implementation_->tracks[groupId.index];
  track.push_back({std::max(0.0f, timeSeconds), transform});
  std::sort(
      track.begin(), track.end(),
      [](const Keyframe &a, const Keyframe &b) { return a.time < b.time; });
  implementation_->duration =
      std::max(implementation_->duration, std::max(0.0f, timeSeconds));
  return true;
}
void VectorShapeAnimation::Clear() {
  for (auto &track : implementation_->tracks)
    track.clear();
  implementation_->duration = 0;
}
bool VectorShapeAnimation::Sample(float timeSeconds, bool loop,
                                  VectorShapePose &pose) const {
  if (!implementation_ || !implementation_->resource || !pose.implementation_ ||
      pose.implementation_->resource != implementation_->resource)
    return false;
  pose.Reset();
  float time = std::max(0.0f, timeSeconds);
  if (implementation_->duration > 0)
    time = loop ? std::fmod(time, implementation_->duration)
                : std::min(time, implementation_->duration);
  for (std::size_t groupIndex = 0; groupIndex < implementation_->tracks.size();
       ++groupIndex) {
    const auto &track = implementation_->tracks[groupIndex];
    if (track.empty())
      continue;
    VectorShapeTransform value = track.front().transform;
    if (time >= track.back().time)
      value = track.back().transform;
    else
      for (std::size_t i = 1; i < track.size(); ++i)
        if (time <= track[i].time) {
          value = Interpolate(track[i - 1], track[i], time);
          break;
        }
    pose.implementation_->transforms[groupIndex] = value;
  }
  return true;
}
} // namespace Engine
