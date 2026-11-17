#pragma once
// PresetIO: Access Virus A/B/C single/multi import & export.
// Pure C++20, no JUCE, no Access ROM/DSP code. The byte layout comes from
// reference/virusC_single_256byte_map.csv via the generated VirusCLayout.h.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vc
{
using Bytes = std::vector<uint8_t>;

constexpr int kSingleSize = 256;
constexpr int kSingleDumpSize = 267;   // F0 00 20 33 01 dev 10 bank prog <256> cs F7
constexpr uint8_t kVirusCVersion = 7;

/** One warning produced while importing. */
struct ImportWarning
{
    enum class Kind { Checksum, Clamped, TiDump, Truncated, Unsupported, Info };
    Kind kind = Kind::Info;
    int patchIndex = -1;      // index in ImportResult::singles, -1 = file level
    int offset = -1;          // byte offset for Clamped
    int rawValue = -1;
    std::string message;
};

/** A Virus single: the raw 256 bytes exactly as imported (out-of-range bytes kept). */
struct SinglePatch
{
    std::array<uint8_t, kSingleSize> raw {};
    int bank = 0;           // 0 = edit buffer
    int program = 0;
    Bytes extra;            // anything beyond 256 bytes (TI dumps), kept opaque
    bool checksumOk = true;

    std::string name() const;              // bytes 240-249, sanitised, trailing spaces trimmed
    void setName (const std::string&);
    int category1() const { return raw[251]; }
    int category2() const { return raw[252]; }
    int version() const   { return raw[0]; }

    /** Value clamped to the documented min/max (what the engine uses). */
    int value (int offset) const;
    /** True if raw byte is outside the documented range. */
    bool isOutOfRange (int offset) const;
};

struct MultiPatch
{
    Bytes raw;              // data bytes between header and checksum (kept as-is)
    int bank = 0, program = 0;
    std::string name() const;   // multi name: bytes 4-13 on A/B/C
};

struct ParamChange
{
    int page = 0x70;        // 0x70 = page A, 0x71 = page B
    int part = 0;
    int index = 0;
    int value = 0;
    int offset() const { return (page == 0x71 ? 128 : 0) + index; }
};

struct ImportResult
{
    std::vector<SinglePatch> singles;
    std::vector<MultiPatch> multis;
    std::vector<ParamChange> paramChanges;
    std::vector<ImportWarning> warnings;
    bool isOsUpdate = false;     // Access OS-update .mid (firmware): rejected
    std::string error;           // non-empty = nothing usable
    int sysexCount = 0;
    int virusSysexCount = 0;
};

// --- low level -------------------------------------------------------------------------
/** Extracts complete SysEx messages (F0 ... F7, inclusive) from a Standard MIDI File.
    Handles type 0 and 1, running status, meta events and F7 continuation packets.
    Returns false if the data is not an SMF. Never throws, never reads out of bounds. */
bool extractSysexFromSmf (const uint8_t* data, size_t size, std::vector<Bytes>& out);
/** Splits raw .syx data on F0 ... F7. */
void extractSysexFromRaw (const uint8_t* data, size_t size, std::vector<Bytes>& out);

/** Virus checksum: sum(msg[5..last]) & 0x7F. */
uint8_t virusChecksum (const uint8_t* msg, int first, int last);

// --- high level ------------------------------------------------------------------------
/** Parses a list of SysEx messages. */
ImportResult parseSysexMessages (const std::vector<Bytes>& messages);
/** Parses file contents (.mid or .syx, detected by content). */
ImportResult importData (const uint8_t* data, size_t size);
ImportResult importFile (const std::string& path);

/** Builds a 267-byte Virus single dump. */
Bytes makeSingleDump (const SinglePatch&, int bank = 0, int program = 0, uint8_t deviceId = 0x10);
/** Builds a parameter-change message. */
Bytes makeParamChange (int offset, int value, int part = 0x40, uint8_t deviceId = 0x10);
/** Writes the singles as a type-0 SMF with one SysEx event each. */
Bytes makeSmf (const std::vector<SinglePatch>&, bool keepBankProgram = true);
/** Concatenated .syx. */
Bytes makeSyx (const std::vector<SinglePatch>&, bool keepBankProgram = true);

bool writeFile (const std::string& path, const Bytes&);
std::optional<Bytes> readFile (const std::string& path);

/** A neutral init patch: one saw, filter open, amp env A0 D0 S127 R0-ish, FX off. */
SinglePatch makeInitPatch();

/** Text for a value (uses the parameter's value list, e.g. "Saw", "-12", "Infinite"). */
std::string valueToText (int offset, int value);

/** JSON dump of a patch (all 256 bytes with name, value, text, out-of-range flag). */
std::string patchToJson (const SinglePatch&);

/** Parameters the v1 engine ignores (input, vocoder, second output ...) that are
    set to something other than their neutral value. */
std::vector<int> ignoredParametersInUse (const SinglePatch&);
} // namespace vc
