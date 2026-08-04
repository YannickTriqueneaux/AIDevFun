#pragma once

#include <string_view>

namespace AssistantProviders::Codex {

[[nodiscard]] bool FeatureListContains(std::string_view featureList,
                                       std::string_view featureName);

} // namespace AssistantProviders::Codex
