#pragma once

#if defined(ENGINE_PROFILE_ENABLED)
#include <tracy/Tracy.hpp>

#define ENGINE_PROFILE_DETAIL_JOIN_IMPL(left, right) left##right
#define ENGINE_PROFILE_DETAIL_JOIN(left, right)                                \
  ENGINE_PROFILE_DETAIL_JOIN_IMPL(left, right)
#define ENGINE_PROFILE_SCOPE(name) ZoneScopedN(name)
#define ENGINE_PROFILE_DYNAMIC_SCOPE(name)                                     \
  ZoneTransientN(ENGINE_PROFILE_DETAIL_JOIN(engineProfileZone, __LINE__),      \
                 name, true)
#define ENGINE_PROFILE_FRAME() FrameMark
#else
#define ENGINE_PROFILE_SCOPE(name) ((void)0)
#define ENGINE_PROFILE_DYNAMIC_SCOPE(name) ((void)0)
#define ENGINE_PROFILE_FRAME() ((void)0)
#endif
