#pragma once
#include <compare>
#include <cstdint>
#include <functional>
namespace Engine::Gameplay {
struct ObjectID { std::uint32_t index=0, version=0; [[nodiscard]] constexpr bool IsValid() const { return index != 0 && version != 0; } constexpr explicit operator bool() const { return IsValid(); } auto operator<=>(const ObjectID&) const = default; static constexpr ObjectID Invalid(){ return {}; } };
using TypeID=std::uint64_t;
consteval TypeID StableTypeID(const char* text) { TypeID hash=14695981039346656037ull; while(*text){ hash^=static_cast<unsigned char>(*text++); hash*=1099511628211ull; } return hash; }
}
template<> struct std::hash<Engine::Gameplay::ObjectID> { std::size_t operator()(Engine::Gameplay::ObjectID id) const noexcept { return (static_cast<std::size_t>(id.version)<<32u)^id.index; } };
