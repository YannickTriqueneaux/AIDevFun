#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>

namespace Engine::Detail {
struct PoolAllocationInfo {
  std::uint32_t index = 0;
  std::uint32_t version = 0;
};

class SparseMemoryPool {
public:
  SparseMemoryPool(std::size_t size, std::size_t alignment)
      : size_(size), alignment_(alignment),
        effectiveAlignment_(std::max(alignment_, alignof(Header))),
        payloadOffset_(AlignUp(sizeof(Header), effectiveAlignment_)),
        stride_(AlignUp(payloadOffset_ + size_, effectiveAlignment_)) {}

  ~SparseMemoryPool() {
    for (const Page &page : pages_)
      ::operator delete(page.memory, std::align_val_t(effectiveAlignment_));
  }

  void *Allocate(bool &recycled) {
    if (freeIndices_.empty())
      CreatePage();
    const std::uint32_t index = freeIndices_.back();
    freeIndices_.pop_back();
    Slot &slot = slots_[index];
    recycled = slot.everAllocated;
    slot.everAllocated = true;
    slot.occupied = true;
    HeaderAt(slot.payload)->version = slot.version;
    return slot.payload;
  }

  void Release(void *payload) noexcept {
    const Header *header = HeaderAt(payload);
    if (header->magic != HeaderMagic || header->pool != this ||
        header->index >= slots_.size())
      std::terminate();
    Slot &slot = slots_[header->index];
    if (!slot.occupied || slot.payload != payload ||
        slot.version != header->version)
      std::terminate();
    slot.occupied = false;
    if (++slot.version == 0)
      ++slot.version;
    freeIndices_.push_back(header->index);
  }

  [[nodiscard]] PoolAllocationInfo Info(const void *payload) const {
    const Header *header = HeaderAt(payload);
    if (header->magic != HeaderMagic || header->pool != this)
      throw std::invalid_argument("Memory does not belong to this pool.");
    return {header->index, header->version};
  }

  [[nodiscard]] std::size_t AvailableCount() const {
    return freeIndices_.size();
  }
  [[nodiscard]] std::size_t Size() const { return size_; }
  [[nodiscard]] std::size_t Alignment() const { return alignment_; }
  using VisitFunction = void (*)(void *payload, void *context);
  void ForEachOccupied(VisitFunction visit, void *context) {
    for (Slot &slot : slots_) {
      if (slot.occupied)
        visit(slot.payload, context);
    }
  }
  static SparseMemoryPool *PoolOf(const void *payload) {
    const Header *header = HeaderAt(payload);
    return header->magic == HeaderMagic ? header->pool : nullptr;
  }

private:
  static constexpr std::uint64_t HeaderMagic = 0x504f4f4c4d455441ull;
  struct alignas(std::max_align_t) Header {
    std::uint64_t magic = HeaderMagic;
    SparseMemoryPool *pool = nullptr;
    std::uint32_t index = 0;
    std::uint32_t version = 1;
  };
  struct Slot {
    void *payload = nullptr;
    std::uint32_t version = 1;
    bool occupied = false;
    bool everAllocated = false;
  };
  struct Page {
    void *memory = nullptr;
  };

  static constexpr std::uint32_t SlotsPerPage = 64;

  static std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
  }

  static Header *HeaderAt(void *payload) {
    return reinterpret_cast<Header *>(static_cast<std::byte *>(payload) -
                                      sizeof(Header));
  }
  static const Header *HeaderAt(const void *payload) {
    return reinterpret_cast<const Header *>(
        static_cast<const std::byte *>(payload) - sizeof(Header));
  }

  void CreatePage() {
    const std::uint32_t firstIndex = static_cast<std::uint32_t>(slots_.size());
    slots_.reserve(slots_.size() + SlotsPerPage);
    pages_.reserve(pages_.size() + 1);
    void *memory = ::operator new(stride_ * SlotsPerPage,
                                  std::align_val_t(effectiveAlignment_));
    pages_.push_back({memory});
    slots_.resize(slots_.size() + SlotsPerPage);
    for (std::uint32_t offset = 0; offset < SlotsPerPage; ++offset) {
      const std::uint32_t index = firstIndex + offset;
      Slot &slot = slots_[index];
      slot.payload =
          static_cast<std::byte *>(memory) + offset * stride_ + payloadOffset_;
      new (HeaderAt(slot.payload))
          Header{HeaderMagic, this, index, slot.version};
    }
    for (std::uint32_t offset = SlotsPerPage; offset > 0; --offset)
      freeIndices_.push_back(firstIndex + offset - 1);
  }

  std::size_t size_;
  std::size_t alignment_;
  std::size_t effectiveAlignment_;
  std::size_t payloadOffset_;
  std::size_t stride_;
  std::vector<Page> pages_;
  std::vector<Slot> slots_;
  std::vector<std::uint32_t> freeIndices_;
};
} // namespace Engine::Detail
