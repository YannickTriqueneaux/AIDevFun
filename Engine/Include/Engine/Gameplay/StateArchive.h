#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace Engine::Gameplay {
class StateWriter {
public:
 template<class T> requires std::is_trivially_copyable_v<T> void Value(const T& value){ const auto* p=reinterpret_cast<const std::byte*>(&value); data_.insert(data_.end(),p,p+sizeof(T)); }
 void String(const std::string& value){ Value(static_cast<std::uint32_t>(value.size())); Bytes(std::as_bytes(std::span(value))); }
 void Bytes(std::span<const std::byte> value){ data_.insert(data_.end(),value.begin(),value.end()); }
 [[nodiscard]] const std::vector<std::byte>& Data() const { return data_; }
 [[nodiscard]] std::vector<std::byte> Take(){ return std::move(data_); }
private: std::vector<std::byte> data_;
};
class StateReader {
public:
 explicit StateReader(std::span<const std::byte> data):data_(data){}
 template<class T> requires std::is_trivially_copyable_v<T> T Value(){ if(remaining()<sizeof(T)) throw std::runtime_error("Truncated gameplay state."); T v{}; std::memcpy(&v,data_.data()+offset_,sizeof(T)); offset_+=sizeof(T); return v; }
 std::string String(){ const auto size=Value<std::uint32_t>(); const auto b=Bytes(size); return {reinterpret_cast<const char*>(b.data()),b.size()}; }
 std::span<const std::byte> Bytes(std::size_t size){ if(size>remaining()) throw std::runtime_error("Truncated gameplay state payload."); auto r=data_.subspan(offset_,size); offset_+=size; return r; }
 [[nodiscard]] std::size_t remaining() const { return data_.size()-offset_; }
 void RequireEnd() const { if(remaining()!=0) throw std::runtime_error("Gameplay state has trailing data."); }
private: std::span<const std::byte> data_; std::size_t offset_=0;
};
}
