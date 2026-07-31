#include "Engine/Core/Memory.h"
#include "SparseMemoryPool.h"

#include <mutex>
#include <unordered_map>

namespace Engine::Memory
{
    namespace
    {
        struct BucketKey { std::size_t size; std::size_t alignment; auto operator<=>(const BucketKey&) const = default; };
        struct BucketHash { std::size_t operator()(const BucketKey& key) const noexcept { return key.size ^ (key.alignment + 0x9e3779b9u + (key.size << 6u) + (key.size >> 2u)); } };
        struct Bucket { BucketKey key; Detail::SparseMemoryPool pool; BucketStats stats; Bucket(BucketKey value):key(value),pool(value.size,value.alignment){} };
        std::mutex bucketMutex;
        std::unordered_map<BucketKey, std::unique_ptr<Bucket>, BucketHash> buckets;
    }

    void* Allocate(std::size_t size, std::size_t alignment)
    {
        std::scoped_lock lock(bucketMutex);
        const BucketKey key{size, alignment};
        auto& bucket = buckets[key];
        if (!bucket) bucket = std::make_unique<Bucket>(key);
        bool recycled = false;
        void* result = bucket->pool.Allocate(recycled);
        ++bucket->stats.liveAllocations; ++bucket->stats.totalAllocations;
        if (recycled) ++bucket->stats.recycledAllocations;
        bucket->stats.availableBlocks = bucket->pool.AvailableCount();
        return result;
    }

    void Release(void* memory, std::size_t size, std::size_t alignment) noexcept
    {
        if (!memory) return;
        std::scoped_lock lock(bucketMutex);
        auto& bucket = buckets.at({size, alignment});
        bucket->pool.Release(memory);
        --bucket->stats.liveAllocations;
        bucket->stats.availableBlocks = bucket->pool.AvailableCount();
    }

    void Release(void* memory) noexcept
    {
        if (!memory) return;
        std::scoped_lock lock(bucketMutex);
        Detail::SparseMemoryPool* pool = Detail::SparseMemoryPool::PoolOf(memory);
        if (!pool) std::terminate();
        auto& bucket = buckets.at({pool->Size(), pool->Alignment()});
        if (&bucket->pool != pool) std::terminate();
        bucket->pool.Release(memory);
        --bucket->stats.liveAllocations;
        bucket->stats.availableBlocks = bucket->pool.AvailableCount();
    }

    BucketStats GetBucketStats(std::size_t size, std::size_t alignment)
    {
        std::scoped_lock lock(bucketMutex);
        const auto found = buckets.find({size, alignment});
        return found == buckets.end() ? BucketStats{} : found->second->stats;
    }

    AllocationInfo GetAllocationInfo(const void* memory, std::size_t size, std::size_t alignment)
    {
        std::scoped_lock lock(bucketMutex);
        const auto info = buckets.at({size, alignment})->pool.Info(memory);
        return {info.index, info.version};
    }
}
