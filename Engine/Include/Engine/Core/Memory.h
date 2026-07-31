#pragma once

#include "Engine/Core/Export.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace Engine::Memory {
struct BucketStats {
  std::size_t liveAllocations = 0;
  std::size_t totalAllocations = 0;
  std::size_t recycledAllocations = 0;
  std::size_t availableBlocks = 0;
};
struct AllocationInfo {
  std::uint32_t index = 0;
  std::uint32_t version = 0;
};

ENGINE_API void *Allocate(std::size_t size, std::size_t alignment);
ENGINE_API void Release(void *memory, std::size_t size,
                        std::size_t alignment) noexcept;
ENGINE_API void Release(void *memory) noexcept;
ENGINE_API BucketStats GetBucketStats(std::size_t size, std::size_t alignment);
ENGINE_API AllocationInfo GetAllocationInfo(const void *memory,
                                            std::size_t size,
                                            std::size_t alignment);

template <class T> struct Deleter {
  void operator()(T *value) const noexcept {
    if (value == nullptr)
      return;
    value->~T();
    Release(value, sizeof(T), alignof(T));
  }
};

template <class T> using Ptr = std::unique_ptr<T, Deleter<T>>;

template <class T> class Allocator {
public:
  using value_type = T;
  Allocator() noexcept = default;
  template <class U> Allocator(const Allocator<U> &) noexcept {}
  [[nodiscard]] T *allocate(std::size_t count) {
    return static_cast<T *>(Allocate(sizeof(T) * count, alignof(T)));
  }
  void deallocate(T *memory, std::size_t count) noexcept {
    Release(memory, sizeof(T) * count, alignof(T));
  }
  template <class U> bool operator==(const Allocator<U> &) const noexcept {
    return true;
  }
};

template <class T, class... Arguments> Ptr<T> New(Arguments &&...arguments) {
  void *memory = Allocate(sizeof(T), alignof(T));
  try {
    return Ptr<T>(new (memory) T(std::forward<Arguments>(arguments)...));
  } catch (...) {
    Release(memory, sizeof(T), alignof(T));
    throw;
  }
}

template <class T> void Delete(Ptr<T> &owner) noexcept { owner.reset(); }
template <class T> void Delete(T *&value) noexcept {
  if (!value)
    return;
  value->~T();
  Release(value, sizeof(T), alignof(T));
  value = nullptr;
}
} // namespace Engine::Memory

#define NEW_MEMORY(Type, ...) ::Engine::Memory::New<Type>(__VA_ARGS__)

#define DELETE_MEMORY(MemoryOwner) ::Engine::Memory::Delete(MemoryOwner)
