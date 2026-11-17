#pragma once
// One importer for every patch type VC-Clone plays. The file type is detected from the content:
//   Virus A/B/C   .mid / .syx           SysEx single dumps (see PresetIO.h)
//   Sylenth1      .fxb / .fxp           VST2 bank/program; chunk "1lys" (zlib) or plain parameters
//   Spire         .spf2 / .repatch      JSON with every parameter by name
// Each patch says which synth mode it belongs to, so loading it switches the mode.

#include "vc/PresetIO.h"
#include <array>
#include <map>
#include <string>
#include <vector>

namespace vc
{
enum class PatchType { Virus = 0, Sylenth = 1, Spire = 2 };

struct SylenthRaw
{
    int version = 0;
    std::vector<float> values;           // Sylenth's own parameter values (0..1), in its VST parameter order
};

struct AnyPatch
{
    PatchType type = PatchType::Virus;
    std::string name, category, author;
    SinglePatch virus;                    // PatchType::Virus
    SylenthRaw sylenth;                   // PatchType::Sylenth
    std::map<std::string, float> spire;   // PatchType::Spire: key -> normalised value
};

struct AnyImport
{
    std::vector<AnyPatch> patches;
    std::vector<ImportWarning> warnings;
    std::string error;
    std::string formatName;               // e.g. "Sylenth1 bank (FBCh, 128 presets)"
};

AnyImport importAnyData (const uint8_t* data, size_t size, const std::string& fileName);

/** A Sylenth1 program file (.fxp, FPCh chunk "1lys" + zlib payload): 244 values in Sylenth's VST order. */
std::vector<uint8_t> writeSylenthFxp (const std::vector<float>& values, const std::string& name);
/** Several Sylenth1 programs: a program file (.fxp, one) or a bank (.fxb, FBCh). */
std::vector<uint8_t> writeSylenthFile (const std::vector<std::vector<float>>& presets, const std::vector<std::string>& names, bool bank);
/** A Spire preset (.spf2 JSON): parameter key -> normalised value, in Spire's parameter order. */
std::string writeSpirePreset (const std::vector<std::pair<std::string, float>>& values, const std::string& name);
AnyImport importAnyFile (const std::string& path);

// --- pieces, exposed for tests -------------------------------------------------------------
/** Inflates a zlib stream. Returns false on error. */
bool zlibInflate (const uint8_t* data, size_t size, std::vector<uint8_t>& out, size_t sizeHint = 0);
/** Parses Sylenth1's "1lys" chunk payload (already inflated): presets of N floats + a 36-byte name. */
bool parseSylenthChunk (const std::vector<uint8_t>& payload, std::vector<AnyPatch>& out, std::string& err);
/** Parses a Spire JSON preset (.spf2 / .repatch). */
bool parseSpireJson (const std::string& text, AnyPatch& out, std::string& err);
} // namespace vc
