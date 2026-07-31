#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace Engine::Detail
{
    struct PoolAllocationInfo
    {
        std::uint32_t index = 0;
        std::uint32_t version = 0;
    };

    class SparseMemoryPool
    {
    public:
        SparseMemoryPool(std::size_t size, std::size_t alignment)
            : size_(size), alignment_(alignment) {}

        ~SparseMemoryPool()
        {
            for (auto& slot : slots_) ::operator delete(slot.rawMemory);
        }

        void* Allocate(bool& recycled)
        {
            std::uint32_t index;
            if (freeIndices_.empty())
            {
                index = static_cast<std::uint32_t>(slots_.size());
                slots_.emplace_back();
                CreateBlock(index);
                recycled = false;
            }
            else
            {
                index = freeIndices_.back();
                freeIndices_.pop_back();
                recycled = true;
            }
            Slot& slot = slots_[index];
            slot.occupied = true;
            HeaderAt(slot.payload)->version = slot.version;
            return slot.payload;
        }

        void Release(void* payload) noexcept
        {
            const Header* header = HeaderAt(payload);
            if (header->magic != HeaderMagic || header->pool != this || header->index >= slots_.size()) std::terminate();
            Slot& slot = slots_[header->index];
            if (!slot.occupied || slot.payload != payload || slot.version != header->version) std::terminate();
            slot.occupied = false;
            if (++slot.version == 0) ++slot.version;
            freeIndices_.push_back(header->index);
        }

        [[nodiscard]] PoolAllocationInfo Info(const void* payload) const
        {
            const Header* header = HeaderAt(payload);
            if (header->magic != HeaderMagic || header->pool != this) throw std::invalid_argument("Memory does not belong to this pool.");
            return {header->index, header->version};
        }

        [[nodiscard]] std::size_t AvailableCount() const { return freeIndices_.size(); }
        [[nodiscard]] std::size_t Size() const { return size_; }
        [[nodiscard]] std::size_t Alignment() const { return alignment_; }
        static SparseMemoryPool* PoolOf(const void* payload)
        {
            const Header* header = HeaderAt(payload);
            return header->magic == HeaderMagic ? header->pool : nullptr;
        }

    private:
        static constexpr std::uint64_t HeaderMagic = 0x504f4f4c4d455441ull;
        struct alignas(std::max_align_t) Header
        {
            std::uint64_t magic = HeaderMagic;
            SparseMemoryPool* pool = nullptr;
            std::uint32_t index = 0;
            std::uint32_t version = 1;
        };
        struct Slot
        {
            void* rawMemory = nullptr;
            void* payload = nullptr;
            std::uint32_t version = 1;
            bool occupied = false;
        };

        static Header* HeaderAt(void* payload) { return reinterpret_cast<Header*>(static_cast<std::byte*>(payload) - sizeof(Header)); }
        static const Header* HeaderAt(const void* payload) { return reinterpret_cast<const Header*>(static_cast<const std::byte*>(payload) - sizeof(Header)); }

        void CreateBlock(std::uint32_t index)
        {
            const std::size_t effectiveAlignment = std::max(alignment_, alignof(Header));
            const std::size_t allocationSize = sizeof(Header) + effectiveAlignment - 1 + size_;
            void* raw = ::operator new(allocationSize);
            void* candidate = static_cast<std::byte*>(raw) + sizeof(Header);
            std::size_t space = allocationSize - sizeof(Header);
            void* payload = std::align(effectiveAlignment, size_, candidate, space);
            if (payload == nullptr) { ::operator delete(raw); throw std::bad_alloc(); }
            Slot& slot = slots_[index];
            slot.rawMemory = raw;
            slot.payload = payload;
            new (HeaderAt(payload)) Header{HeaderMagic, this, index, slot.version};
        }

        std::size_t size_;
        std::size_t alignment_;
        std::vector<Slot> slots_;
        std::vector<std::uint32_t> freeIndices_;
    };
}
