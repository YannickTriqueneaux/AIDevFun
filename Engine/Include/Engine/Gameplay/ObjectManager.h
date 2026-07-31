#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Gameplay/Object.h"
#include "Engine/Gameplay/ObjectMemory.h"

#include <string>

namespace Engine::Gameplay
{
    class ENGINE_API ObjectManager
    {
    public:
        ObjectManager();
        ~ObjectManager();
        ObjectManager(const ObjectManager&) = delete;
        ObjectManager& operator=(const ObjectManager&) = delete;
        ObjectManager(ObjectManager&&) noexcept;
        ObjectManager& operator=(ObjectManager&&) noexcept;

        ObjectID Reserve();
        ObjectID Add(ObjectPtr object);
        void BindReserved(ObjectID id, ObjectPtr object);
        void Restore(ObjectID id, ObjectPtr object);
        void RejectRestored(ObjectID id);
        void Destroy(ObjectID id);
        void Clear();

        [[nodiscard]] Object* Get(ObjectID id);
        [[nodiscard]] const Object* Get(ObjectID id) const;
        template<class T> [[nodiscard]] T* GetAs(ObjectID id) { return dynamic_cast<T*>(Get(id)); }
        template<class T> [[nodiscard]] const T* GetAs(ObjectID id) const { return dynamic_cast<const T*>(Get(id)); }

        void SetDebugName(ObjectID id, std::string name);
        [[nodiscard]] const std::string& GetDebugName(ObjectID id) const;
        [[nodiscard]] std::size_t LiveCount() const;

    private:
        class Impl;
        Impl* impl_ = nullptr;
    };

    template<class T> T* ObjectRef<T>::Resolve(ObjectManager& manager) const { return manager.GetAs<T>(id_); }
    template<class T> const T* ObjectRef<T>::Resolve(const ObjectManager& manager) const { return manager.GetAs<T>(id_); }
}
