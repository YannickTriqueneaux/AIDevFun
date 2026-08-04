#include "AssistantProviders/Codex/CodexFeatureDetection.h"

#include <cctype>

namespace AssistantProviders::Codex {

bool FeatureListContains(std::string_view featureList,
                         std::string_view featureName) {
  std::size_t lineStart = 0;
  while (lineStart < featureList.size()) {
    const std::size_t lineEnd = featureList.find('\n', lineStart);
    const std::string_view line = featureList.substr(
        lineStart, lineEnd == std::string_view::npos
                       ? std::string_view::npos
                       : lineEnd - lineStart);
    const std::size_t nameEnd = line.find_first_of(" \t\r");
    if (line.substr(0, nameEnd) == featureName)
      return true;
    if (lineEnd == std::string_view::npos)
      break;
    lineStart = lineEnd + 1;
  }
  return false;
}

} // namespace AssistantProviders::Codex
