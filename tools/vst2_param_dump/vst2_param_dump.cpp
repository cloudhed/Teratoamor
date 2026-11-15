// vst2_param_dump: a minimal, black-box VST2 host for behavioural reference work.
//
// It loads a 32- or 64-bit VST2 plug-in DLL (matching this executable's bitness),
// optionally loads .fxp program files into it, and writes every parameter's
// normalised value, display text, and label to a CSV file. It only talks to the
// plug-in through the public VST2 calling interface, exactly as a DAW would; it
// does not read or interpret the plug-in's code or its private chunk format.
//
// Usage:
//   vst2_param_dump <plugin.dll> <out.csv>                 (iterate built-in programs)
//   vst2_param_dump <plugin.dll> <out.csv> a.fxp b.fxp ... (load each .fxp)

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#pragma comment(lib, "user32.lib")

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
// Minimal VST2 ABI declarations, written from the publicly documented interface.
struct AEffect;
using HostCallback = intptr_t (__cdecl*) (AEffect*, int32_t, int32_t, intptr_t, void*, float);
using DispatcherProc = intptr_t (__cdecl*) (AEffect*, int32_t, int32_t, intptr_t, void*, float);
using ProcessProc = void (__cdecl*) (AEffect*, float**, float**, int32_t);
using SetParameterProc = void (__cdecl*) (AEffect*, int32_t, float);
using GetParameterProc = float (__cdecl*) (AEffect*, int32_t);
using EntryProc = AEffect* (__cdecl*) (HostCallback);

struct AEffect
{
    int32_t magic;
    DispatcherProc dispatcher;
    ProcessProc process;
    SetParameterProc setParameter;
    GetParameterProc getParameter;
    int32_t numPrograms;
    int32_t numParams;
    int32_t numInputs;
    int32_t numOutputs;
    int32_t flags;
    intptr_t resvd1;
    intptr_t resvd2;
    int32_t initialDelay;
    int32_t realQualities;
    int32_t offQualities;
    float ioRatio;
    void* object;
    void* user;
    int32_t uniqueID;
    int32_t version;
    ProcessProc processReplacing;
    void* processDoubleReplacing;
    char future[56];
};

enum Opcode : int32_t
{
    effOpen = 0,
    effClose = 1,
    effSetProgram = 2,
    effGetProgramName = 5,
    effGetParamLabel = 6,
    effGetParamDisplay = 7,
    effGetParamName = 8,
    effSetSampleRate = 10,
    effSetBlockSize = 11,
    effMainsChanged = 12,
    effSetChunk = 24,
    effStartProcess = 71,
    effStopProcess = 72,
};

enum HostOpcode : int32_t
{
    audioMasterAutomate = 0,
    audioMasterVersion = 1,
    audioMasterCurrentId = 2,
    audioMasterIdle = 3,
    audioMasterGetTime = 7,
    audioMasterGetSampleRate = 16,
    audioMasterGetBlockSize = 17,
    audioMasterGetVendorString = 32,
    audioMasterGetProductString = 33,
    audioMasterGetVendorVersion = 34,
    audioMasterCanDo = 37,
    audioMasterGetLanguage = 38,
};

constexpr int kSampleRate = 44100;
constexpr int kBlockSize = 512;

struct TimeInfo
{
    double samplePos, sampleRate, nanoSeconds, ppqPos, tempo, barStartPos, cycleStartPos, cycleEndPos;
    int32_t timeSigNumerator, timeSigDenominator, smpteOffset, smpteFrameRate, samplesToNextClock, flags;
};

TimeInfo timeInfo { 0.0, kSampleRate, 0.0, 0.0, 120.0, 0.0, 0.0, 0.0, 4, 4, 0, 0, 0, (1 << 9) | (1 << 10) | (1 << 13) };
bool isVerbose()
{
    char* value = nullptr;
    size_t length = 0;
    const bool set = _dupenv_s (&value, &length, "VST2_DUMP_VERBOSE") == 0 && value != nullptr;
    std::free (value);
    return set;
}

const bool verbose = isVerbose();

intptr_t __cdecl hostCallback (AEffect*, int32_t opcode, int32_t index, intptr_t value, void* ptr, float)
{
    if (verbose)
        std::fprintf (stderr, "  host opcode %d (index %d, value %lld)\n", opcode, index, (long long) value);

    switch (opcode)
    {
        case audioMasterGetTime: return reinterpret_cast<intptr_t> (&timeInfo);
        case audioMasterVersion: return 2400;
        case audioMasterGetSampleRate: return kSampleRate;
        case audioMasterGetBlockSize: return kBlockSize;
        case audioMasterGetVendorString: if (ptr != nullptr) strcpy_s (static_cast<char*> (ptr), 64, "Teratoamor tools"); return 1;
        case audioMasterGetProductString: if (ptr != nullptr) strcpy_s (static_cast<char*> (ptr), 64, "vst2_param_dump"); return 1;
        case audioMasterGetVendorVersion: return 1;
        case audioMasterGetLanguage: return 1;
        default: return 0;
    }
}

std::string dispatchString (AEffect* effect, int32_t opcode, int32_t index)
{
    char buffer[512] = {};
    effect->dispatcher (effect, opcode, index, 0, buffer, 0.0f);
    buffer[sizeof (buffer) - 1] = '\0';
    return buffer;
}

std::string csvEscape (const std::string& text)
{
    std::string out = "\"";
    for (char c : text)
    {
        if (c == '"')
            out += "\"\"";
        else
            out += c;
    }
    return out + "\"";
}

uint32_t readBigEndian32 (const unsigned char* bytes)
{
    return (uint32_t (bytes[0]) << 24) | (uint32_t (bytes[1]) << 16) | (uint32_t (bytes[2]) << 8) | uint32_t (bytes[3]);
}

// Gives the plug-in the idle time and Windows message processing a DAW would.
// Some plug-ins forward parameter and program changes to their audio engine here.
void pumpHostIdle (AEffect* effect)
{
    MSG message;
    while (PeekMessageA (&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage (&message);
        DispatchMessageA (&message);
    }

    effect->dispatcher (effect, 53 /* effIdle */, 0, 0, nullptr, 0.0f);
    effect->dispatcher (effect, 19 /* effEditIdle */, 0, 0, nullptr, 0.0f);
}

// Runs silent audio so plug-ins that apply parameter changes on the audio
// thread have settled before values are read.
void runSilence (AEffect* effect, int blocks)
{
    static std::vector<float> buffers[16];
    float* channelPointers[16] = {};

    for (int i = 0; i < 16; ++i)
    {
        buffers[i].assign (kBlockSize, 0.0f);
        channelPointers[i] = buffers[i].data();
    }

    for (int i = 0; i < blocks; ++i)
    {
        pumpHostIdle (effect);
        if (effect->processReplacing != nullptr)
            effect->processReplacing (effect, channelPointers, channelPointers + 8, kBlockSize);
    }
}

// Loads an .fxp file through the plug-in's own effSetChunk or setParameter,
// as a host does. Returns the program name stored in the file header.
bool loadFxp (AEffect* effect, const std::string& path, std::string& programName)
{
    std::ifstream file (path, std::ios::binary);
    if (! file)
        return false;

    std::vector<unsigned char> data ((std::istreambuf_iterator<char> (file)), std::istreambuf_iterator<char>());
    if (data.size() < 56 || std::memcmp (data.data(), "CcnK", 4) != 0)
        return false;

    char name[29] = {};
    std::memcpy (name, data.data() + 28, 28);
    programName = name;

    if (std::memcmp (data.data() + 8, "FPCh", 4) == 0)
    {
        if (data.size() < 60)
            return false;

        const auto chunkSize = readBigEndian32 (data.data() + 56);
        if (60 + size_t (chunkSize) > data.size())
            return false;

        effect->dispatcher (effect, effSetChunk, 1, intptr_t (chunkSize), data.data() + 60, 0.0f);
        return true;
    }

    if (std::memcmp (data.data() + 8, "FxCk", 4) == 0)
    {
        const auto numParams = int (readBigEndian32 (data.data() + 24));
        for (int i = 0; i < numParams && 56 + size_t (i + 1) * 4 <= data.size(); ++i)
        {
            const auto bits = readBigEndian32 (data.data() + 56 + size_t (i) * 4);
            float value = 0.0f;
            std::memcpy (&value, &bits, sizeof (value));
            effect->setParameter (effect, i, value);
        }
        return true;
    }

    return false;
}

void writeRows (std::ofstream& csv, AEffect* effect, int programIndex, const std::string& source, const std::string& programName)
{
    for (int i = 0; i < effect->numParams; ++i)
    {
        char value[32] = {};
        std::snprintf (value, sizeof (value), "%.6f", double (effect->getParameter (effect, i)));
        csv << programIndex << ',' << csvEscape (source) << ',' << csvEscape (programName) << ',' << i << ','
            << csvEscape (dispatchString (effect, effGetParamName, i)) << ',' << value << ','
            << csvEscape (dispatchString (effect, effGetParamDisplay, i)) << ','
            << csvEscape (dispatchString (effect, effGetParamLabel, i)) << '\n';
    }
}
// ---- Render mode: play notes through the plug-in and record its output ----

struct MidiEvent
{
    int32_t type = 1; // kVstMidiType
    int32_t byteSize = sizeof (MidiEvent);
    int32_t deltaFrames = 0;
    int32_t flags = 1; // realtime event
    int32_t noteLength = 0;
    int32_t noteOffset = 0;
    char midiData[4] = {};
    char detune = 0;
    char noteOffVelocity = 0;
    char reserved1 = 0;
    char reserved2 = 0;
};

struct EventList
{
    int32_t numEvents = 0;
    intptr_t reserved = 0;
    void* events[2] = {};
};

constexpr int32_t effProcessEvents = 25;

void sendNote (AEffect* effect, int note, int velocity, bool on, int deltaFrames)
{
    static MidiEvent event;
    static EventList list;
    event = MidiEvent {};
    event.deltaFrames = deltaFrames;
    event.midiData[0] = char (on ? 0x90 : 0x80);
    event.midiData[1] = char (note);
    event.midiData[2] = char (on ? velocity : 0);
    list.numEvents = 1;
    list.events[0] = &event;
    effect->dispatcher (effect, effProcessEvents, 0, 0, &list, 0.0f);
}

bool writeWav (const std::string& path, const std::vector<float>& left, const std::vector<float>& right)
{
    std::ofstream out (path, std::ios::binary);
    if (! out)
        return false;

    const uint32_t frames = uint32_t (left.size());
    const uint32_t dataBytes = frames * 2 * 4;
    auto put32 = [&out] (uint32_t v) { out.write (reinterpret_cast<const char*> (&v), 4); };
    auto put16 = [&out] (uint16_t v) { out.write (reinterpret_cast<const char*> (&v), 2); };

    out.write ("RIFF", 4); put32 (36 + dataBytes); out.write ("WAVE", 4);
    out.write ("fmt ", 4); put32 (16); put16 (3); put16 (2); put32 (kSampleRate); put32 (kSampleRate * 8); put16 (8); put16 (32);
    out.write ("data", 4); put32 (dataBytes);

    for (uint32_t i = 0; i < frames; ++i)
    {
        out.write (reinterpret_cast<const char*> (&left[i]), 4);
        out.write (reinterpret_cast<const char*> (&right[i]), 4);
    }

    return bool (out);
}

// Values of each program as first loaded, captured before any job overrides them.
float programValues (AEffect* effect, int program, int index)
{
    static std::vector<std::vector<float>> cache;
    if (cache.empty())
        cache.resize (size_t (std::max (1, effect->numPrograms)));

    auto& values = cache[size_t (program)];
    if (values.empty())
    {
        for (int i = 0; i < effect->numParams; ++i)
            values.push_back (effect->getParameter (effect, i));
    }

    return values[size_t (index)];
}

// Each job line: out.wav program note velocity total_seconds note_off_seconds [param_id=value ...]
// Lines starting with '#' are ignored. Parameters are applied after the program is selected.
int runRenderJobs (AEffect* effect, const std::string& jobsPath)
{
    std::ifstream jobs (jobsPath);
    if (! jobs)
    {
        std::fprintf (stderr, "Could not read %s\n", jobsPath.c_str());
        return 1;
    }

    int failures = 0;
    std::string line;
    std::vector<float> blockL (kBlockSize), blockR (kBlockSize), silence (kBlockSize, 0.0f);

    while (std::getline (jobs, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == '\r')
            continue;

        char outPath[1024] = {};
        int program = 0, note = 60, velocity = 100;
        double totalSeconds = 2.0, offSeconds = 1.0;
        int consumed = 0;

        if (std::sscanf (line.c_str(), "%1023s %d %d %d %lf %lf%n", outPath, &program, &note, &velocity, &totalSeconds, &offSeconds, &consumed) < 6)
        {
            std::fprintf (stderr, "Bad job line: %s\n", line.c_str());
            ++failures;
            continue;
        }

        // Selecting the already-current program may be ignored, so step through another program first.
        effect->dispatcher (effect, effSetProgram, 0, (program + 1) % std::max (1, effect->numPrograms), nullptr, 0.0f);
        runSilence (effect, 4);
        effect->dispatcher (effect, effSetProgram, 0, program, nullptr, 0.0f);
        runSilence (effect, 16); // let the program change settle before overriding parameters

        // Some plug-ins only forward values that differ between programs to their audio engine,
        // so an earlier job's overrides could survive. Re-send every program value explicitly,
        // nudging each one so the plug-in sees a real change. (VST2 parameter edits also change
        // the current program, so the original values are cached on first use, before any override.)
        programValues (effect, program, 0);
        for (int i = 0; i < effect->numParams; ++i)
        {
            const float value = effect->getParameter (effect, i);
            effect->setParameter (effect, i, value < 0.5f ? value + 0.01f : value - 0.01f);
        }
        runSilence (effect, 4);
        for (int i = 0; i < effect->numParams; ++i)
            effect->setParameter (effect, i, programValues (effect, program, i));
        runSilence (effect, 4);

        const char* rest = line.c_str() + consumed;
        int paramIndex = 0;
        float paramValue = 0.0f;
        int used = 0;
        while (std::sscanf (rest, " %d=%f%n", &paramIndex, &paramValue, &used) == 2)
        {
            if (paramIndex >= 0 && paramIndex < effect->numParams)
            {
                effect->setParameter (effect, paramIndex, paramValue);
                if (verbose)
                    std::fprintf (stderr, "  set %d=%.4f -> reads %.4f\n", paramIndex, double (paramValue), double (effect->getParameter (effect, paramIndex)));
            }
            rest += used;
        }

        // Let parameter changes settle and any previous sound die away.
        runSilence (effect, int (1.0 * kSampleRate / kBlockSize));

        const int totalFrames = int (totalSeconds * kSampleRate);
        const int offFrame = int (offSeconds * kSampleRate);
        std::vector<float> left, right;
        left.reserve (size_t (totalFrames));
        right.reserve (size_t (totalFrames));

        bool noteOffSent = false;
        for (int start = 0; start < totalFrames; start += kBlockSize)
        {
            if (start == 0)
                sendNote (effect, note, velocity, true, 0);
            if (! noteOffSent && offFrame >= start && offFrame < start + kBlockSize)
            {
                sendNote (effect, note, velocity, false, offFrame - start);
                noteOffSent = true;
            }

            float* inputs[2] = { silence.data(), silence.data() };
            std::fill (blockL.begin(), blockL.end(), 0.0f);
            std::fill (blockR.begin(), blockR.end(), 0.0f);
            float* outputs[2] = { blockL.data(), blockR.data() };
            effect->processReplacing (effect, inputs, outputs, kBlockSize);

            const int count = std::min (kBlockSize, totalFrames - start);
            left.insert (left.end(), blockL.begin(), blockL.begin() + count);
            right.insert (right.end(), blockR.begin(), blockR.begin() + count);
        }

        if (! noteOffSent)
            sendNote (effect, note, velocity, false, 0);

        if (! writeWav (outPath, left, right))
        {
            std::fprintf (stderr, "Could not write %s\n", outPath);
            ++failures;
        }
    }

    return failures;
}
} // namespace

void step (const char* text)
{
    std::fprintf (stderr, "[step] %s\n", text);
    std::fflush (stderr);
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf (stderr, "Usage: %s <plugin.dll> <out.csv> [program.fxp ...]\n", argv[0]);
        return 2;
    }

    HMODULE module = LoadLibraryA (argv[1]);
    if (module == nullptr)
    {
        std::fprintf (stderr, "Could not load %s (Windows error %lu). Is the DLL the same bitness as this tool?\n", argv[1], GetLastError());
        return 1;
    }

    auto entry = reinterpret_cast<EntryProc> (GetProcAddress (module, "VSTPluginMain"));
    if (entry == nullptr)
        entry = reinterpret_cast<EntryProc> (GetProcAddress (module, "main"));
    if (entry == nullptr)
    {
        std::fprintf (stderr, "The DLL has no VST2 entry point.\n");
        return 1;
    }

    step ("calling entry point");
    AEffect* effect = entry (hostCallback);
    if (effect == nullptr || effect->magic != 0x56737450) // 'VstP'
    {
        std::fprintf (stderr, "The plug-in did not return a valid VST2 effect.\n");
        return 1;
    }

    step ("effOpen");
    effect->dispatcher (effect, effOpen, 0, 0, nullptr, 0.0f);
    effect->dispatcher (effect, effSetSampleRate, 0, 0, nullptr, float (kSampleRate));
    effect->dispatcher (effect, effSetBlockSize, 0, kBlockSize, nullptr, 0.0f);
    step ("resume");
    effect->dispatcher (effect, effMainsChanged, 0, 1, nullptr, 0.0f);
    effect->dispatcher (effect, effStartProcess, 0, 0, nullptr, 0.0f);
    step ("process silence");
    runSilence (effect, 8);

    std::printf ("Plug-in: %d parameters, %d programs, %d in / %d out\n",
                 effect->numParams, effect->numPrograms, effect->numInputs, effect->numOutputs);

    const std::string secondArgument = argv[2];
    if (secondArgument.size() > 5 && secondArgument.substr (secondArgument.size() - 5) == ".jobs")
    {
        const int failures = runRenderJobs (effect, secondArgument);
        effect->dispatcher (effect, effStopProcess, 0, 0, nullptr, 0.0f);
        effect->dispatcher (effect, effMainsChanged, 0, 0, nullptr, 0.0f);
        effect->dispatcher (effect, effClose, 0, 0, nullptr, 0.0f);
        return failures == 0 ? 0 : 1;
    }

    std::ofstream csv (argv[2], std::ios::binary);
    if (! csv)
    {
        std::fprintf (stderr, "Could not write %s\n", argv[2]);
        return 1;
    }

    csv << "program,source,program_name,param_id,param_name,normalised,display,label\n";

    if (argc == 3)
    {
        for (int p = 0; p < effect->numPrograms; ++p)
        {
            effect->dispatcher (effect, effSetProgram, 0, p, nullptr, 0.0f);
            runSilence (effect, 8);
            writeRows (csv, effect, p, "built-in", dispatchString (effect, effGetProgramName, 0));
        }
    }
    else
    {
        for (int f = 3; f < argc; ++f)
        {
            std::string programName;
            if (! loadFxp (effect, argv[f], programName))
            {
                std::fprintf (stderr, "Skipped unreadable .fxp: %s\n", argv[f]);
                continue;
            }

            runSilence (effect, 8);
            writeRows (csv, effect, f - 3, argv[f], programName);
        }
    }

    effect->dispatcher (effect, effStopProcess, 0, 0, nullptr, 0.0f);
    effect->dispatcher (effect, effMainsChanged, 0, 0, nullptr, 0.0f);
    effect->dispatcher (effect, effClose, 0, 0, nullptr, 0.0f);
    std::printf ("Wrote %s\n", argv[2]);
    return 0;
}
