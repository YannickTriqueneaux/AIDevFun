#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Gameplay/ObjectID.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace Engine::Gameplay {
class Object;
using ObjectPoolDomain = std::uint64_t;
using ObjectPoolVisitFunction = void (*)(void *object, void *context);

ENGINE_API ObjectPoolDomain CreateObjectPoolDomain();
ENGINE_API void DestroyObjectPoolDomain(ObjectPoolDomain domain) noexcept;
ENGINE_API void SetActiveObjectPoolDomain(ObjectPoolDomain domain);
ENGINE_API ObjectPoolDomain GetActiveObjectPoolDomain();

class ENGINE_API ObjectPoolDomainScope {
public:
  ObjectPoolDomainScope();
  ~ObjectPoolDomainScope();
  ObjectPoolDomainScope(const ObjectPoolDomainScope &) = delete;
  ObjectPoolDomainScope &operator=(const ObjectPoolDomainScope &) = delete;
  [[nodiscard]] ObjectPoolDomain Get() const { return domain_; }

private:
  ObjectPoolDomain domain_ = 0;
};

ENGINE_API void *AllocateObjectMemory(TypeID storageType, std::size_t size,
                                      std::size_t alignment);
ENGINE_API void ReleaseObjectMemory(TypeID storageType, void *memory,
                                    std::size_t size,
                                    std::size_t alignment) noexcept;
ENGINE_API void VisitObjectsInActivePool(TypeID storageType, std::size_t size,
                                         std::size_t alignment,
                                         ObjectPoolVisitFunction visit,
                                         void *context);

struct ObjectPoolStats {
  std::size_t liveObjects = 0;
  std::size_t totalAllocations = 0;
  std::size_t recycledAllocations = 0;
  std::size_t availableBlocks = 0;
};

ENGINE_API ObjectPoolStats GetObjectPoolStats(TypeID storageType);
struct ObjectAllocationInfo {
  std::uint32_t index = 0;
  std::uint32_t version = 0;
};
ENGINE_API ObjectAllocationInfo GetObjectAllocationInfo(TypeID storageType,
                                                        const Object *object);

template <class T> consteval TypeID ObjectStorageTypeID() {
#if defined(_MSC_VER)
  return StableTypeID(__FUNCSIG__);
#else
  return StableTypeID(__PRETTY_FUNCTION__);
#endif
}

struct ObjectDeleter {
  using DestroyFunction = void (*)(Object *) noexcept;
  TypeID storageType = 0;
  std::size_t size = 0;
  std::size_t alignment = 0;
  DestroyFunction destroy = nullptr;
  ENGINE_API void operator()(Object *object) const noexcept;
};

using ObjectPtr = std::unique_ptr<Object, ObjectDeleter>;

template <class T, class... Arguments>
ObjectPtr NewObject(Arguments &&...arguments) {
  static_assert(std::is_base_of_v<Object, T>);
  constexpr TypeID storageType = ObjectStorageTypeID<T>();
  void *memory = AllocateObjectMemory(storageType, sizeof(T), alignof(T));
  try {
    T *object = new (memory) T(std::forward<Arguments>(arguments)...);
    return ObjectPtr(object, ObjectDeleter{storageType, sizeof(T), alignof(T),
                                           [](Object *value) noexcept {
                                             static_cast<T *>(value)->~T();
                                           }});
  } catch (...) {
    ReleaseObjectMemory(storageType, memory, sizeof(T), alignof(T));
    throw;
  }
}
} // namespace Engine::Gameplay

#define NEW_OBJECT(Type, ...) ::Engine::Gameplay::NewObject<Type>(__VA_ARGS__)

#define DELETE_OBJECT(ObjectOwner) (ObjectOwner).reset()
