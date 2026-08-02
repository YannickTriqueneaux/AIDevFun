#include "AssistantHost/AssistantProviderLoader.h"

#include <stdexcept>

namespace AssistantHost {

AssistantProviderLoader::AssistantProviderLoader(
    const std::filesystem::path &libraryPath,
    const std::filesystem::path &settingsPath,
    const std::filesystem::path &gameRoot)
    : library_(libraryPath) {
  const auto getApi =
      reinterpret_cast<Development::GetAssistantProviderApiFunction>(
          library_.GetFunction("GetAssistantProviderApi"));
  api_ = getApi();
  if (api_ == nullptr ||
      api_->apiVersion != Development::AssistantProviderApiVersion ||
      api_->create == nullptr || api_->destroy == nullptr)
    throw std::runtime_error("Assistant provider ABI is incompatible.");
  provider_ =
      api_->create(settingsPath.string().c_str(), gameRoot.string().c_str());
  if (provider_ == nullptr)
    throw std::runtime_error("Assistant provider creation failed.");
}

AssistantProviderLoader::~AssistantProviderLoader() {
  if (provider_ != nullptr)
    api_->destroy(provider_);
}

Development::AssistantProvider &AssistantProviderLoader::Get() const {
  return *provider_;
}

} // namespace AssistantHost
