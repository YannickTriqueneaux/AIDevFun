#include "Engine/AI/OpenAIPricing.h"

#include <algorithm>

namespace Engine {
OpenAICostEstimate EstimateOpenAICost(std::string_view model,
                                      const OpenAIPricingSettings &pricing,
                                      const OpenAITokenUsage &usage) {
  const std::string snapshotPrefix = pricing.model + "-202";
  if (!pricing.IsConfigured() ||
      (model != pricing.model && !model.starts_with(snapshotPrefix))) {
    return {};
  }

  const std::uint64_t cachedTokens =
      std::min(usage.cachedInputTokens, usage.inputTokens);
  const std::uint64_t uncachedTokens = usage.inputTokens - cachedTokens;
  const bool longContext = pricing.longContextThreshold > 0 &&
                           usage.inputTokens > pricing.longContextThreshold;
  const double inputMultiplier =
      longContext ? pricing.longContextInputMultiplier : 1.0;
  const double outputMultiplier =
      longContext ? pricing.longContextOutputMultiplier : 1.0;

  const double inputCost =
      (static_cast<double>(uncachedTokens) * pricing.inputUsdPerMillion +
       static_cast<double>(cachedTokens) * pricing.cachedInputUsdPerMillion) /
      1'000'000.0 * inputMultiplier;
  const double outputCost = static_cast<double>(usage.outputTokens) *
                            pricing.outputUsdPerMillion / 1'000'000.0 *
                            outputMultiplier;

  return {.available = true, .usd = inputCost + outputCost};
}
} // namespace Engine
