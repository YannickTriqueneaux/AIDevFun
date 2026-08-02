#include "AssistantHost/MidiAttachmentProvider.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void TestMidiParsing() {
  const std::vector<std::uint8_t> bytes{
      'M',  'T', 'h',  'd',  0,    0,    0,    6, 0,    0,    0,    1, 1,
      0xe0, 'M', 'T',  'r',  'k',  0,    0,    0, 0x1c, 0,    0xff, 3, 4,
      'L',  'e', 'a',  'd',  0,    0xff, 0x51, 3, 0x07, 0xa1, 0x20, 0, 0x90,
      60,   100, 0x83, 0x60, 0x80, 60,   0,    0, 0xff, 0x2f, 0};

  const AssistantHost::MidiFile midi = AssistantHost::ParseMidiFile(bytes);
  Require(midi.format == 0, "MIDI format was not parsed.");
  Require(midi.ticksPerQuarterNote == 480, "MIDI PPQ was not parsed.");
  Require(midi.tracks.size() == 1 && midi.tracks[0].name == "Lead",
          "MIDI track metadata was not parsed.");
  Require(midi.tracks[0].notes.size() == 1,
          "MIDI note events were not paired.");
  const auto &note = midi.tracks[0].notes[0];
  Require(note.note == 60 && note.startTick == 0 && note.durationTicks == 480 &&
              note.velocity == 100,
          "MIDI note properties were not parsed.");

  const std::string description =
      AssistantHost::DescribeMidiForPrompt(midi, "reference.mid");
  Require(description.find("120.00") != std::string::npos,
          "MIDI tempo was not described.");
  Require(description.find("C4(60)") != std::string::npos,
          "MIDI pitch was not described.");
}
} // namespace

int main() {
  try {
    TestMidiParsing();
    std::cout << "AssistantHost tests passed.\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "AssistantHost test failure: " << exception.what() << '\n';
    return 1;
  }
}
