#pragma once

#include "AssistantHost/PromptConsole.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AssistantHost {
struct MidiNote {
  std::uint32_t startTick = 0;
  std::uint32_t durationTicks = 0;
  std::uint8_t channel = 0;
  std::uint8_t note = 0;
  std::uint8_t velocity = 0;
};

struct MidiTempoChange {
  std::uint32_t tick = 0;
  std::uint32_t microsecondsPerQuarterNote = 500'000;
};

struct MidiTrack {
  std::string name;
  std::vector<MidiNote> notes;
};

struct MidiFile {
  std::uint16_t format = 0;
  std::uint16_t ticksPerQuarterNote = 0;
  std::vector<MidiTempoChange> tempos;
  std::vector<MidiTrack> tracks;
};

[[nodiscard]] MidiFile ParseMidiFile(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string DescribeMidiForPrompt(const MidiFile &midi,
                                                std::string_view fileName);

class MidiAttachmentProvider final : public PromptAttachmentProvider {
public:
  [[nodiscard]] PromptAttachmentResult PasteClipboardFile() override;
};
} // namespace AssistantHost
