#include "AssistantHost/MidiAttachmentProvider.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#endif

namespace {
class Reader {
public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  [[nodiscard]] std::size_t Remaining() const {
    return bytes_.size() - offset_;
  }
  [[nodiscard]] std::size_t Offset() const { return offset_; }

  std::uint8_t Byte() {
    Require(1);
    return bytes_[offset_++];
  }

  std::uint16_t Big16() {
    const auto first = Byte();
    return static_cast<std::uint16_t>((first << 8U) | Byte());
  }

  std::uint32_t Big32() {
    const std::uint32_t first = Big16();
    return (first << 16U) | Big16();
  }

  std::uint32_t Variable() {
    std::uint32_t value = 0;
    for (int count = 0; count < 4; ++count) {
      const std::uint8_t byte = Byte();
      value = (value << 7U) | (byte & 0x7fU);
      if ((byte & 0x80U) == 0)
        return value;
    }
    throw std::runtime_error("Invalid MIDI variable-length value.");
  }

  std::span<const std::uint8_t> Bytes(std::size_t size) {
    Require(size);
    const auto result = bytes_.subspan(offset_, size);
    offset_ += size;
    return result;
  }

  void Skip(std::size_t size) { (void)Bytes(size); }

private:
  void Require(std::size_t size) const {
    if (size > Remaining())
      throw std::runtime_error("Truncated MIDI file.");
  }

  std::span<const std::uint8_t> bytes_;
  std::size_t offset_ = 0;
};

bool IsChunk(std::span<const std::uint8_t> id, std::string_view expected) {
  return id.size() == expected.size() &&
         std::equal(id.begin(), id.end(), expected.begin());
}

std::string NoteName(std::uint8_t note) {
  static constexpr std::array Names{"C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B"};
  return std::string(Names[note % 12]) +
         std::to_string(static_cast<int>(note) / 12 - 1);
}

std::optional<std::filesystem::path> ClipboardMidiPath() {
#if defined(_WIN32)
  if (!OpenClipboard(nullptr))
    return std::nullopt;
  struct ClipboardCloser {
    ~ClipboardCloser() { CloseClipboard(); }
  } closer;

  const HANDLE handle = GetClipboardData(CF_HDROP);
  if (handle == nullptr)
    return std::nullopt;
  const auto drop = static_cast<HDROP>(handle);
  const UINT count = DragQueryFileW(drop, 0xffffffffU, nullptr, 0);
  for (UINT index = 0; index < count; ++index) {
    const UINT length = DragQueryFileW(drop, index, nullptr, 0);
    std::wstring value(length + 1, L'\0');
    if (DragQueryFileW(drop, index, value.data(), length + 1) == 0)
      continue;
    value.resize(length);
    std::filesystem::path path(value);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                     return static_cast<char>(std::tolower(character));
                   });
    if (extension == ".mid" || extension == ".midi")
      return path;
  }
#endif
  return std::nullopt;
}
} // namespace

namespace AssistantHost {
MidiFile ParseMidiFile(std::span<const std::uint8_t> bytes) {
  Reader reader(bytes);
  if (!IsChunk(reader.Bytes(4), "MThd"))
    throw std::runtime_error("The file is not a Standard MIDI File.");
  const std::uint32_t headerSize = reader.Big32();
  if (headerSize < 6)
    throw std::runtime_error("Invalid MIDI header.");

  MidiFile midi;
  midi.format = reader.Big16();
  const std::uint16_t trackCount = reader.Big16();
  const std::uint16_t division = reader.Big16();
  if ((division & 0x8000U) != 0)
    throw std::runtime_error("SMPTE-timed MIDI files are not supported.");
  if (division == 0)
    throw std::runtime_error("MIDI ticks per quarter note cannot be zero.");
  midi.ticksPerQuarterNote = division;
  reader.Skip(headerSize - 6);

  midi.tracks.reserve(trackCount);
  for (std::uint16_t trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
    if (!IsChunk(reader.Bytes(4), "MTrk"))
      throw std::runtime_error("Missing MIDI track chunk.");
    const auto trackBytes = reader.Bytes(reader.Big32());
    Reader trackReader(trackBytes);
    MidiTrack track;
    std::array<
        std::array<std::vector<std::pair<std::uint32_t, std::uint8_t>>, 128>,
        16>
        activeNotes;
    std::uint32_t tick = 0;
    std::uint8_t runningStatus = 0;

    while (trackReader.Remaining() > 0) {
      tick += trackReader.Variable();
      std::uint8_t status = trackReader.Byte();
      bool consumedFirstDataByte = false;
      std::uint8_t firstDataByte = 0;
      if (status < 0x80U) {
        if (runningStatus == 0)
          throw std::runtime_error("Invalid MIDI running status.");
        firstDataByte = status;
        status = runningStatus;
        consumedFirstDataByte = true;
      } else if (status < 0xf0U) {
        runningStatus = status;
      }

      if (status == 0xffU) {
        runningStatus = 0;
        const std::uint8_t type = trackReader.Byte();
        const auto data = trackReader.Bytes(trackReader.Variable());
        if (type == 0x03U)
          track.name.assign(data.begin(), data.end());
        else if (type == 0x51U && data.size() == 3) {
          midi.tempos.push_back(
              {tick, static_cast<std::uint32_t>((data[0] << 16U) |
                                                (data[1] << 8U) | data[2])});
        }
        if (type == 0x2fU)
          break;
        continue;
      }
      if (status == 0xf0U || status == 0xf7U) {
        runningStatus = 0;
        trackReader.Skip(trackReader.Variable());
        continue;
      }
      if (status >= 0xf0U)
        throw std::runtime_error("Unsupported MIDI system event.");

      const std::uint8_t event = status & 0xf0U;
      const std::uint8_t channel = status & 0x0fU;
      const auto dataByte = [&]() {
        if (consumedFirstDataByte) {
          consumedFirstDataByte = false;
          return firstDataByte;
        }
        return trackReader.Byte();
      };
      const std::uint8_t first = dataByte();
      const bool oneDataByte = event == 0xc0U || event == 0xd0U;
      const std::uint8_t second = oneDataByte ? 0 : trackReader.Byte();

      if (event == 0x90U && second != 0) {
        activeNotes[channel][first].push_back({tick, second});
      } else if (event == 0x80U || (event == 0x90U && second == 0)) {
        auto &notes = activeNotes[channel][first];
        if (!notes.empty()) {
          const auto [start, velocity] = notes.front();
          notes.erase(notes.begin());
          track.notes.push_back(
              {start, std::max(1U, tick - start), channel, first, velocity});
        }
      }
    }

    std::sort(track.notes.begin(), track.notes.end(),
              [](const MidiNote &left, const MidiNote &right) {
                return std::tie(left.startTick, left.channel, left.note) <
                       std::tie(right.startTick, right.channel, right.note);
              });
    midi.tracks.push_back(std::move(track));
  }

  if (midi.tempos.empty())
    midi.tempos.push_back({});
  std::sort(midi.tempos.begin(), midi.tempos.end(),
            [](const auto &left, const auto &right) {
              return left.tick < right.tick;
            });
  return midi;
}

std::string DescribeMidiForPrompt(const MidiFile &midi,
                                  std::string_view fileName) {
  constexpr std::size_t MaximumDescribedNotes = 4'096;
  std::ostringstream output;
  output << "\n\n--- MIDI reference: " << fileName << " ---\n"
         << "Format: " << midi.format << "; PPQ: " << midi.ticksPerQuarterNote
         << "; tracks: " << midi.tracks.size() << "\nTempo map (tick:BPM): ";
  for (std::size_t index = 0; index < midi.tempos.size(); ++index) {
    if (index != 0)
      output << ", ";
    const auto &tempo = midi.tempos[index];
    const double bpm = 60'000'000.0 / tempo.microsecondsPerQuarterNote;
    output << tempo.tick << ':' << std::fixed << std::setprecision(2) << bpm;
  }
  output << "\nNotes use startBeat,durationBeat,note,velocity,channel.\n";

  std::size_t describedNotes = 0;
  for (std::size_t trackIndex = 0; trackIndex < midi.tracks.size();
       ++trackIndex) {
    const MidiTrack &track = midi.tracks[trackIndex];
    output << "Track " << trackIndex << " \""
           << (track.name.empty() ? "Unnamed" : track.name) << "\" ("
           << track.notes.size() << " notes):\n";
    for (const MidiNote &note : track.notes) {
      if (describedNotes++ >= MaximumDescribedNotes) {
        output << "[remaining notes omitted]\n";
        return output.str();
      }
      const double start =
          static_cast<double>(note.startTick) / midi.ticksPerQuarterNote;
      const double duration =
          static_cast<double>(note.durationTicks) / midi.ticksPerQuarterNote;
      output << std::fixed << std::setprecision(3) << start << ',' << duration
             << ',' << NoteName(note.note) << '(' << static_cast<int>(note.note)
             << ")," << static_cast<int>(note.velocity) << ','
             << static_cast<int>(note.channel + 1) << '\n';
    }
  }
  return output.str();
}

PromptAttachmentResult MidiAttachmentProvider::PasteClipboardFile() {
  constexpr std::uintmax_t MaximumFileSize = 2 * 1024 * 1024;
  const auto path = ClipboardMidiPath();
  if (!path)
    return {};

  PromptAttachmentResult result{.handled = true};
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(*path, error);
  if (error || size == 0 || size > MaximumFileSize) {
    result.message =
        "The MIDI file is empty, inaccessible, or larger than 2 MB.";
    return result;
  }

  std::ifstream stream(*path, std::ios::binary);
  const std::vector<char> rawBytes{std::istreambuf_iterator<char>(stream),
                                   std::istreambuf_iterator<char>()};
  const std::vector<std::uint8_t> bytes(rawBytes.begin(), rawBytes.end());
  try {
    const MidiFile midi = ParseMidiFile(bytes);
    const std::string fileName = path->filename().string();
    result.attachment = PromptTextAttachment{
        "MIDI - " + fileName, DescribeMidiForPrompt(midi, fileName)};
    std::size_t noteCount = 0;
    for (const MidiTrack &track : midi.tracks)
      noteCount += track.notes.size();
    result.message = "Attached MIDI " + fileName + " (" +
                     std::to_string(midi.tracks.size()) + " tracks, " +
                     std::to_string(noteCount) + " notes).";
  } catch (const std::exception &exception) {
    result.message = std::string("Could not attach MIDI: ") + exception.what();
  }
  return result;
}
} // namespace AssistantHost
