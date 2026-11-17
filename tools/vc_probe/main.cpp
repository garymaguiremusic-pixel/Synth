// vc_probe: reads a plugin's parameter list through the normal plugin API (black box), for VC-Clone's
// Sylenth and Spire modes. Nothing is unpacked or disassembled; the plugin is only hosted.
//
//   vc_probe sylenth "C:\path\Sylenth1.dll" [bank.fxb]   -> sylenth_params.json
//   vc_probe spire   "C:\path\Spire.vst3"                -> spire_params.json
//
// Sylenth1 is a VST2 plugin: hosted through the plain VST2 C ABI (struct layout and opcode numbers
// as publicly documented). Spire is VST3: hosted through JUCE.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <fstream>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

using namespace juce;

namespace vst2
{
    struct AEffect;
    using HostCallback = intptr_t (*) (AEffect*, int32_t, int32_t, intptr_t, void*, float);
    using Dispatcher = intptr_t (*) (AEffect*, int32_t, int32_t, intptr_t, void*, float);
    using SetParam = void (*) (AEffect*, int32_t, float);
    using GetParam = float (*) (AEffect*, int32_t);
    using Process = void (*) (AEffect*, float**, float**, int32_t);
    struct AEffect
    {
        int32_t magic; Dispatcher dispatcher; Process process; SetParam setParameter; GetParam getParameter;
        int32_t numPrograms, numParams, numInputs, numOutputs, flags;
        intptr_t resvd1, resvd2;
        int32_t initialDelay, realQualities, offQualities;
        float ioRatio;
        void* object; void* user;
        int32_t uniqueID, version;
        Process processReplacing; void* processDoubleReplacing;
        char future[56];
    };
    enum { effOpen = 0, effClose = 1, effSetProgram = 2, effGetProgramName = 5, effGetParamLabel = 6, effGetParamDisplay = 7,
           effGetParamName = 8, effSetSampleRate = 10, effSetBlockSize = 11, effMainsChanged = 12, effGetChunk = 23, effSetChunk = 24 };
    static intptr_t host (AEffect*, int32_t opcode, int32_t, intptr_t, void*, float)
    {
        if (opcode == 1) return 2400;    // audioMasterVersion
        return 0;
    }
}

static String jsonEscape (const String& s) { return s.replace ("\\", "\\\\").replace ("\"", "\\\""); }

static int probeSylenth (const File& dll, const File& bank, const File& out)
{
#if JUCE_WINDOWS
    HMODULE mod = LoadLibraryW (dll.getFullPathName().toWideCharPointer());
    if (! mod) { std::printf ("could not load %s\n", dll.getFullPathName().toRawUTF8()); return 1; }
    using Main = vst2::AEffect* (*) (vst2::HostCallback);
    auto entry = (Main) GetProcAddress (mod, "VSTPluginMain");
    if (! entry) entry = (Main) GetProcAddress (mod, "main");
    if (! entry) { std::printf ("no VST2 entry point\n"); return 1; }
    auto* fx = entry (vst2::host);
    if (! fx || fx->magic != 0x56737450) { std::printf ("not a VST2 plugin\n"); return 1; }
    fx->dispatcher (fx, vst2::effOpen, 0, 0, nullptr, 0);
    fx->dispatcher (fx, vst2::effSetSampleRate, 0, 0, nullptr, 44100.0f);
    fx->dispatcher (fx, vst2::effSetBlockSize, 0, 512, nullptr, 0);
    fx->dispatcher (fx, vst2::effMainsChanged, 0, 1, nullptr, 0);

    auto str = [&] (int op, int idx) { char b[256] = {}; fx->dispatcher (fx, op, idx, 0, b, 0); return String (b).trim(); };
    String j;
    j << "{\n  \"plugin\": \"Sylenth1\",\n  \"uniqueID\": " << fx->uniqueID << ",\n  \"version\": " << fx->version
      << ",\n  \"numParams\": " << fx->numParams << ",\n  \"numPrograms\": " << fx->numPrograms << ",\n  \"params\": [\n";
    for (int i = 0; i < fx->numParams; ++i)
    {
        const float orig = fx->getParameter (fx, i);
        j << "    { \"index\": " << i << ", \"name\": \"" << jsonEscape (str (vst2::effGetParamName, i)) << "\", \"label\": \""
          << jsonEscape (str (vst2::effGetParamLabel, i)) << "\", \"default\": " << orig << ", \"display\": {";
        // the value -> text curve at 33 points (exact units / list entries)
        for (int k = 0; k <= 32; ++k)
        {
            const float v = (float) k / 32.0f;
            fx->setParameter (fx, i, v);
            j << (k ? ", " : "") << "\"" << String (v, 5) << "\": \"" << jsonEscape (str (vst2::effGetParamDisplay, i)) << "\"";
        }
        fx->setParameter (fx, i, orig);
        j << "} }" << (i + 1 < fx->numParams ? "," : "") << "\n";
    }
    j << "  ]";

    // load the user's bank into the real plugin and read the parameters back, preset by preset
    if (bank.existsAsFile())
    {
        MemoryBlock mb;
        bank.loadFileAsData (mb);
        const auto* d = (const uint8_t*) mb.getData();
        if (mb.getSize() > 160 && std::memcmp (d, "CcnK", 4) == 0 && std::memcmp (d + 8, "FBCh", 4) == 0)
        {
            const uint32_t len = ((uint32_t) d[156] << 24) | ((uint32_t) d[157] << 16) | ((uint32_t) d[158] << 8) | d[159];
            fx->dispatcher (fx, vst2::effSetChunk, 0, (intptr_t) len, (void*) (d + 160), 0);
            j << ",\n  \"bank\": \"" << jsonEscape (bank.getFileName()) << "\",\n  \"presets\": [\n";
            const int n = jmin (fx->numPrograms, 512);
            bool first = true;
            for (int pgm = 0; pgm < n; ++pgm)
            {
                fx->dispatcher (fx, vst2::effSetProgram, 0, pgm, nullptr, 0);
                const String name = str (vst2::effGetProgramName, 0);
                if (name.isEmpty()) continue;
                j << (first ? "" : ",\n") << "    { \"program\": " << pgm << ", \"name\": \"" << jsonEscape (name) << "\", \"values\": [";
                for (int i = 0; i < fx->numParams; ++i) j << (i ? "," : "") << String (fx->getParameter (fx, i), 6);
                j << "] }";
                first = false;
            }
            j << "\n  ]";
        }
    }
    j << "\n}\n";
    fx->dispatcher (fx, vst2::effMainsChanged, 0, 0, nullptr, 0);
    fx->dispatcher (fx, vst2::effClose, 0, 0, nullptr, 0);
    out.replaceWithText (j);
    std::printf ("wrote %s (%d parameters)\n", out.getFullPathName().toRawUTF8(), fx->numParams);
    return 0;
#else
    (void) dll; (void) bank; (void) out;
    std::printf ("Sylenth1 is a Windows VST2 plugin: run this on Windows\n");
    return 1;
#endif
}

static int probeVst3 (const File& path, const File& out)
{
    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> found;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, path.getFullPathName());
    if (found.isEmpty()) { std::printf ("no VST3 plugin in %s\n", path.getFullPathName().toRawUTF8()); return 1; }
    std::printf ("opening %s\n", found[0]->name.toRawUTF8()); std::fflush (stdout);
    String err;
    auto inst = fm.createPluginInstance (*found[0], 44100.0, 512, err);
    if (! inst) { std::printf ("could not open: %s\n", err.toRawUTF8()); return 1; }
    inst->prepareToPlay (44100.0, 512);
    String j;
    j << "{\n  \"plugin\": \"" << jsonEscape (found[0]->name) << "\",\n  \"version\": \"" << jsonEscape (found[0]->version) << "\",\n  \"params\": [\n";
    auto& params = inst->getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        if (i % 100 == 0) { std::printf ("  parameter %d of %d\n", i, params.size()); std::fflush (stdout); }
        String id;
        if (auto* hp = dynamic_cast<HostedAudioProcessorParameter*> (p)) id = hp->getParameterID();
        const float orig = p->getValue();
        j << "    { \"index\": " << i << ", \"id\": \"" << jsonEscape (id) << "\", \"name\": \"" << jsonEscape (p->getName (128))
          << "\", \"label\": \"" << jsonEscape (p->getLabel()) << "\", \"default\": " << orig << ", \"steps\": " << p->getNumSteps()
          << ", \"display\": {";
        for (int k = 0; k <= 16; ++k)
        {
            const float v = (float) k / 16.0f;
            j << (k ? ", " : "") << "\"" << String (v, 4) << "\": \"" << jsonEscape (p->getText (v, 64)) << "\"";
        }
        // every step of 1/999 (Spire stores list entries as index / 999): only where the text changes
        j << "}, \"changes\": [";
        String last;
        bool first = true;
        for (int k = 0; k <= 999; ++k)
        {
            const String t = p->getText ((float) k / 999.0f, 64);
            if (k > 0 && t == last) continue;
            j << (first ? "" : ", ") << "[" << k << ", \"" << jsonEscape (t) << "\"]";
            first = false;
            last = t;
        }
        j << "] }" << (i + 1 < params.size() ? "," : "") << "\n";
    }
    j << "  ]\n}\n";
    out.replaceWithText (j);
    std::printf ("wrote %s (%d parameters)\n", out.getFullPathName().toRawUTF8(), params.size());
    return 0;
}

// Lists only: one knob per kind of list (osc1_wave stands for osc2..4_wave and so on; every *_target / *_Trg
// knob shares one list), read at every step of 1/999 until the text stops changing. Takes about a minute.
static int probeVst3Lists (const File& path, const File& out)
{
    AudioPluginFormatManager fm;
    fm.addFormat (new VST3PluginFormat());
    OwnedArray<PluginDescription> found;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, path.getFullPathName());
    if (found.isEmpty()) { std::printf ("no VST3 plugin in %s\n", path.getFullPathName().toRawUTF8()); return 1; }
    std::printf ("opening %s\n", found[0]->name.toRawUTF8()); std::fflush (stdout);
    String err;
    auto inst = fm.createPluginInstance (*found[0], 44100.0, 512, err);
    if (! inst) { std::printf ("could not open: %s\n", err.toRawUTF8()); return 1; }
    auto& params = inst->getParameters();
    auto isNumber = [] (const String& t) { return t.trim().containsOnly ("0123456789.-+ ") && t.trim().isNotEmpty(); };
    StringArray done;
    String j;
    j << "{\n  \"plugin\": \"" << jsonEscape (found[0]->name) << "\",\n  \"lists\": [\n";
    bool firstParam = true;
    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        const String name = p->getName (128);
        String kind = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-");
        if (kind.containsIgnoreCase ("target") || kind.containsIgnoreCase ("Trg")) kind = "target";
        if (kind.containsIgnoreCase ("Src")) kind = "source";
        if (done.contains (kind)) continue;
        // a list shows text (not a number) somewhere in its first steps
        bool list = false;
        for (int k : { 1, 2, 3, 5, 10, 62 })
            if (! isNumber (p->getText ((float) k / 999.0f, 64))) { list = true; break; }
        if (! list) continue;
        done.add (kind);
        std::printf ("  list %s\n", name.toRawUTF8()); std::fflush (stdout);
        j << (firstParam ? "" : ",\n") << "    { \"index\": " << i << ", \"name\": \"" << jsonEscape (name) << "\", \"entries\": [";
        firstParam = false;
        String last;
        int same = 0;
        bool first = true;
        for (int k = 0; k <= 999 && same < 40; ++k)
        {
            const String t = p->getText ((float) k / 999.0f, 64);
            if (k > 0 && t == last) { ++same; continue; }
            same = 0;
            j << (first ? "" : ", ") << "[" << k << ", \"" << jsonEscape (t) << "\"]";
            first = false;
            last = t;
        }
        j << "] }";
    }
    j << "\n  ]\n}\n";
    out.replaceWithText (j);
    std::printf ("wrote %s (%d lists)\n", out.getFullPathName().toRawUTF8(), done.size());
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 3)
    {
        std::puts ("usage:\n  vc_probe sylenth <Sylenth1.dll> [bank.fxb]\n  vc_probe spire <Spire.vst3>\n"
                   "  vc_probe spirelists <Spire.vst3>   (fast: list entries only)\n  vc_probe virus <Osirus.vst3>\n  vc_probe vcclone <VC-Clone.vst3>");
        return 2;
    }
    const String what (argv[1]);
    const File plugin (String::fromUTF8 (argv[2]));
    const File cwd = File::getCurrentWorkingDirectory();
    if (what == "sylenth") return probeSylenth (plugin, argc > 3 ? File (String::fromUTF8 (argv[3])) : File(), cwd.getChildFile ("sylenth_params.json"));
    if (what == "spire") return probeVst3 (plugin, cwd.getChildFile ("spire_params.json"));
    if (what == "spirelists") return probeVst3Lists (plugin, cwd.getChildFile ("spire_lists.json"));
    if (what == "virus") return probeVst3 (plugin, cwd.getChildFile ("virus_params.json"));        // Osirus (or any Virus VST3)
    if (what == "vcclone") return probeVst3 (plugin, cwd.getChildFile ("vcclone_params.json"));    // our own plugin, for a cross-check
    std::puts ("first argument: sylenth, spire, spirelists, virus or vcclone");
    return 2;
}
