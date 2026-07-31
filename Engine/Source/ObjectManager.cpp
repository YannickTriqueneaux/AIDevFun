#include "Engine/Gameplay/ObjectManager.h"
#include "Engine/Core/Memory.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine::Gameplay
{
    class ObjectManager::Impl
    {
    public:
        struct Slot
        {
            std::uint32_t version = 1;
            ObjectPtr object;
            bool reserved = false;
        };

        std::vector<Slot> slots{1};
        std::vector<std::uint32_t> freeIndices;
        std::unordered_map<ObjectID, std::string> names;
        std::size_t liveCount = 0;
    };

    ObjectManager::ObjectManager() : impl_(NEW_MEMORY(Impl).release()) {}
    ObjectManager::~ObjectManager() { DELETE_MEMORY(impl_); }
    ObjectManager::ObjectManager(ObjectManager&& other) noexcept : impl_(std::exchange(other.impl_, nullptr)) {}
    ObjectManager& ObjectManager::operator=(ObjectManager&& other) noexcept { if (this != &other) { DELETE_MEMORY(impl_); impl_ = std::exchange(other.impl_, nullptr); } return *this; }

    ObjectID ObjectManager::Reserve()
    {
        std::uint32_t index;
        if (impl_->freeIndices.empty())
        {
            index = static_cast<std::uint32_t>(impl_->slots.size());
            impl_->slots.emplace_back();
        }
        else
        {
            index = impl_->freeIndices.back();
            impl_->freeIndices.pop_back();
        }
        auto& slot = impl_->slots[index];
        slot.reserved = true;
        return {index, slot.version};
    }

    ObjectID ObjectManager::Add(ObjectPtr object)
    {
        const ObjectID id = Reserve();
        BindReserved(id, std::move(object));
        return id;
    }

    void ObjectManager::BindReserved(ObjectID id, ObjectPtr object)
    {
        if (!object || id.index >= impl_->slots.size()) throw std::invalid_argument("Invalid object binding.");
        auto& slot = impl_->slots[id.index];
        if (!slot.reserved || slot.version != id.version || slot.object) throw std::logic_error("ObjectID is not reserved.");
        object->id_ = id;
        slot.object = std::move(object);
        slot.reserved = false;
        ++impl_->liveCount;
    }

    void ObjectManager::Restore(ObjectID id, ObjectPtr object)
    {
        if (!id.IsValid() || !object) throw std::invalid_argument("Invalid restored object.");
        if (impl_->slots.size() <= id.index) impl_->slots.resize(static_cast<std::size_t>(id.index) + 1);
        auto& slot = impl_->slots[id.index];
        if (slot.object || slot.reserved) throw std::logic_error("Duplicate restored ObjectID.");
        slot.version = id.version;
        object->id_ = id;
        slot.object = std::move(object);
        ++impl_->liveCount;
    }

    void ObjectManager::RejectRestored(ObjectID id)
    {
        if (!id.IsValid()) return;
        if (impl_->slots.size() <= id.index) impl_->slots.resize(static_cast<std::size_t>(id.index) + 1);
        auto& slot = impl_->slots[id.index];
        if (slot.object || slot.reserved) throw std::logic_error("Cannot reject an occupied restored ObjectID.");
        slot.version = id.version + 1;
        if (slot.version == 0) ++slot.version;
        impl_->freeIndices.push_back(id.index);
    }

    void ObjectManager::Destroy(ObjectID id)
    {
        if (id.index >= impl_->slots.size()) return;
        auto& slot = impl_->slots[id.index];
        if (slot.version != id.version || (!slot.object && !slot.reserved)) return;
        const bool wasLive = slot.object != nullptr;
        DELETE_OBJECT(slot.object);
        slot.reserved = false;
        impl_->names.erase(id);
        if (++slot.version == 0) ++slot.version;
        impl_->freeIndices.push_back(id.index);
        if (wasLive) --impl_->liveCount;
    }

    Object* ObjectManager::Get(ObjectID id) { return const_cast<Object*>(std::as_const(*this).Get(id)); }
    const Object* ObjectManager::Get(ObjectID id) const
    {
        if (!id.IsValid() || id.index >= impl_->slots.size()) return nullptr;
        const auto& slot = impl_->slots[id.index];
        return slot.version == id.version ? slot.object.get() : nullptr;
    }

    void ObjectManager::Clear()
    {
        DELETE_MEMORY(impl_);
        impl_ = NEW_MEMORY(Impl).release();
    }

    void ObjectManager::SetDebugName(ObjectID id, std::string name)
    {
        if (!Get(id)) throw std::invalid_argument("Cannot name an unknown object.");
        impl_->names[id] = std::move(name);
    }

    const std::string& ObjectManager::GetDebugName(ObjectID id) const
    {
        static const std::string EmptyName;
        const auto found = impl_->names.find(id);
        return found == impl_->names.end() ? EmptyName : found->second;
    }

    std::size_t ObjectManager::LiveCount() const { return impl_->liveCount; }
}
