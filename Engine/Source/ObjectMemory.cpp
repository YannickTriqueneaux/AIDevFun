#include "Engine/Gameplay/ObjectMemory.h"
#include "Engine/Gameplay/Object.h"
#include "SparseMemoryPool.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace Engine::Gameplay {
namespace {
struct PoolKey {
  ObjectPoolDomain domain;
  TypeID storageType;
  std::size_t size;
  std::size_t alignment;
  auto operator<=>(const PoolKey &) const = default;
};
struct PoolKeyHash {
  std::size_t operator()(const PoolKey &key) const noexcept {
    std::size_t hash = std::hash<ObjectPoolDomain>{}(key.domain);
    hash ^= key.storageType + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
    hash ^= key.size + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
    hash ^= key.alignment + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
    return hash;
  }
};
struct Pool {
  explicit Pool(const PoolKey &key) : memory(key.size, key.alignment) {}
  Detail::SparseMemoryPool memory;
  ObjectPoolStats stats;
};
std::mutex poolMutex;
std::unordered_map<PoolKey, std::unique_ptr<Pool>, PoolKeyHash> pools;
std::atomic<ObjectPoolDomain> nextDomain{1};
ObjectPoolDomain activeDomain = 0;
} // namespace

ObjectPoolDomain CreateObjectPoolDomain() { return nextDomain.fetch_add(1); }

void DestroyObjectPoolDomain(ObjectPoolDomain domain) noexcept {
  std::scoped_lock lock(poolMutex);
  for (auto entry = pools.begin(); entry != pools.end();) {
    if (entry->first.domain != domain) {
      ++entry;
      continue;
    }
    if (entry->second->stats.liveObjects != 0)
      std::terminate();
    entry = pools.erase(entry);
  }
}

ObjectPoolDomainScope::ObjectPoolDomainScope()
    : domain_(CreateObjectPoolDomain()) {}

ObjectPoolDomainScope::~ObjectPoolDomainScope() {
  DestroyObjectPoolDomain(domain_);
}

void SetActiveObjectPoolDomain(ObjectPoolDomain domain) {
  std::scoped_lock lock(poolMutex);
  activeDomain = domain;
}

ObjectPoolDomain GetActiveObjectPoolDomain() {
  std::scoped_lock lock(poolMutex);
  return activeDomain;
}

void *AllocateObjectMemory(TypeID storageType, std::size_t size,
                           std::size_t alignment) {
  std::scoped_lock lock(poolMutex);
  const PoolKey key{activeDomain, storageType, size, alignment};
  auto &pool = pools[key];
  if (!pool)
    pool = std::make_unique<Pool>(key);
  bool recycled = false;
  void *result = pool->memory.Allocate(recycled);
  ++pool->stats.liveObjects;
  ++pool->stats.totalAllocations;
  if (recycled)
    ++pool->stats.recycledAllocations;
  pool->stats.availableBlocks = pool->memory.AvailableCount();
  return result;
}

void ReleaseObjectMemory(TypeID storageType, void *memory, std::size_t size,
                         std::size_t alignment) noexcept {
  if (!memory)
    return;
  std::scoped_lock lock(poolMutex);
  Detail::SparseMemoryPool *owningPool =
      Detail::SparseMemoryPool::PoolOf(memory);
  auto found = pools.end();
  for (auto candidate = pools.begin(); candidate != pools.end(); ++candidate) {
    if (&candidate->second->memory == owningPool) {
      found = candidate;
      break;
    }
  }
  if (found == pools.end() || found->first.storageType != storageType ||
      found->first.size != size || found->first.alignment != alignment)
    std::terminate();
  auto &pool = *found->second;
  pool.memory.Release(memory);
  --pool.stats.liveObjects;
  pool.stats.availableBlocks = pool.memory.AvailableCount();
}

void VisitObjectsInActivePool(TypeID storageType, std::size_t size,
                              std::size_t alignment,
                              ObjectPoolVisitFunction visit, void *context) {
  Pool *pool = nullptr;
  {
    std::scoped_lock lock(poolMutex);
    const auto found = pools.find({activeDomain, storageType, size, alignment});
    if (found != pools.end())
      pool = found->second.get();
  }
  if (pool)
    pool->memory.ForEachOccupied(visit, context);
}

ObjectPoolStats GetObjectPoolStats(TypeID storageType) {
  std::scoped_lock lock(poolMutex);
  ObjectPoolStats combined;
  for (const auto &[key, pool] : pools) {
    if (key.storageType != storageType)
      continue;
    combined.liveObjects += pool->stats.liveObjects;
    combined.totalAllocations += pool->stats.totalAllocations;
    combined.recycledAllocations += pool->stats.recycledAllocations;
    combined.availableBlocks += pool->stats.availableBlocks;
  }
  return combined;
}

ObjectAllocationInfo GetObjectAllocationInfo(TypeID storageType,
                                             const Object *object) {
  std::scoped_lock lock(poolMutex);
  Detail::SparseMemoryPool *owningPool =
      Detail::SparseMemoryPool::PoolOf(object);
  for (const auto &[key, pool] : pools) {
    if (key.storageType == storageType && &pool->memory == owningPool) {
      const auto info = pool->memory.Info(object);
      return {info.index, info.version};
    }
  }
  throw std::invalid_argument(
      "Object does not belong to the requested type pool.");
}

void ObjectDeleter::operator()(Object *object) const noexcept {
  if (!object)
    return;
  destroy(object);
  ReleaseObjectMemory(storageType, object, size, alignment);
}
} // namespace Engine::Gameplay
