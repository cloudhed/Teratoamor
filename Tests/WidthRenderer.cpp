// Offline renderer for the matched Width comparison documented in DEVELOPMENT_PLAN.md.
// Uses Teratoamor's actual DSP engine; no plugin host or audio device is involved.
#include "DSP/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr int sampleRate = 44100;
    constexpr int channels = 2;
    constexpr int bitsPerSample = 16;
    constexpr int blockSize = 256;

    struct Mode
    {
        FilterMode value;
        const char* filename;
    };

    constexpr std::array<Mode, 4> modes {{
        { FilterMode::bpWide,     "bpwide" },
        { FilterMode::bpNarrow,   "bpnarrow" },
        { FilterMode::peakWide,   "peakwide" },
        { FilterMode::peakNarrow, "peaknarrow" }
    }};

    constexpr std::array<int, 6> widths {{ 0, 25, 50, 75, 90, 100 }};
    constexpr std::array<int, 3> notes {{ 36, 60, 84 }};

    void writeU16 (std::ofstream& stream, std::uint16_t value)
    {
        const char bytes[] { static_cast<char> (value & 0xff), static_cast<char> ((value >> 8) & 0xff) };
        stream.write (bytes, 2);
    }

    void writeU32 (std::ofstream& stream, std::uint32_t value)
    {
        const char bytes[] {
            static_cast<char> (value & 0xff), static_cast<char> ((value >> 8) & 0xff),
            static_cast<char> ((value >> 16) & 0xff), static_cast<char> ((value >> 24) & 0xff)
        };
        stream.write (bytes, 4);
    }

    bool writeWav (const std::filesystem::path& path, const std::vector<float>& left,
                   const std::vector<float>& right)
    {
        if (left.size() != right.size())
            return false;

        std::ofstream stream (path, std::ios::binary);
        if (! stream)
            return false;

        const auto dataBytes = static_cast<std::uint32_t> (left.size() * channels * sizeof (std::int16_t));
        stream.write ("RIFF", 4); writeU32 (stream, 36u + dataBytes); stream.write ("WAVE", 4);
        stream.write ("fmt ", 4); writeU32 (stream, 16); writeU16 (stream, 1); writeU16 (stream, channels);
        writeU32 (stream, sampleRate); writeU32 (stream, sampleRate * channels * bitsPerSample / 8);
        writeU16 (stream, channels * bitsPerSample / 8); writeU16 (stream, bitsPerSample);
        stream.write ("data", 4); writeU32 (stream, dataBytes);

        for (size_t i = 0; i < left.size(); ++i)
            for (float sample : { left[i], right[i] })
            {
                const float bounded = std::clamp (sample, -1.0f, 1.0f);
                const auto pcm = static_cast<std::int16_t> (std::lrint (bounded * (bounded < 0.0f ? 32768.0f : 32767.0f)));
                writeU16 (stream, static_cast<std::uint16_t> (pcm));
            }

        return static_cast<bool> (stream);
    }

    void renderSeconds (Engine& engine, std::vector<float>& left, std::vector<float>& right, int seconds)
    {
        const int total = seconds * sampleRate;
        std::array<float, blockSize> blockLeft {}, blockRight {};
        for (int done = 0; done < total;)
        {
            const int count = std::min (blockSize, total - done);
            engine.render (blockLeft.data(), blockRight.data(), count);
            left.insert (left.end(), blockLeft.begin(), blockLeft.begin() + count);
            right.insert (right.end(), blockRight.begin(), blockRight.begin() + count);
            done += count;
        }
    }

    bool renderFile (const std::filesystem::path& path, FilterMode mode, float width)
    {
        Engine engine;
        engine.prepare (sampleRate);

        EngineParams params;
        params.master = 100.0f;
        params.elements[0].enabled = true;
        params.elements[0].filterMode = mode;
        params.elements[0].width = width;
        // Remaining Element 1 settings retain the measured default-patch values;
        // Elements 2 and 3 retain their default disabled state.
        engine.setParams (params);

        std::vector<float> left, right;
        left.reserve (16 * sampleRate);
        right.reserve (16 * sampleRate);

        renderSeconds (engine, left, right, 1); // pre-roll
        constexpr float velocity = 100.0f / 127.0f;
        for (int note : notes)
        {
            engine.noteOn (note, velocity);
            renderSeconds (engine, left, right, 4);
            engine.noteOff (note);
            renderSeconds (engine, left, right, 1);
        }

        return writeWav (path, left, right);
    }
}

int main (int argc, char* argv[])
{
    const std::filesystem::path output = argc > 1
        ? std::filesystem::path (argv[1])
        : std::filesystem::path ("reference/teratoamor/width_across_all_elements");

    std::error_code error;
    std::filesystem::create_directories (output, error);
    if (error)
    {
        std::cerr << "Could not create output directory: " << error.message() << '\n';
        return 1;
    }

    for (const auto& mode : modes)
        for (int width : widths)
        {
            const std::string padded = width == 0 ? "000" : width < 100 ? "0" + std::to_string (width) : "100";
            const auto path = output / ("teratoamor_" + std::string (mode.filename) + "_width_" + padded + ".wav");
            if (! renderFile (path, mode.value, static_cast<float> (width)))
            {
                std::cerr << "Failed to write " << path.string() << '\n';
                return 1;
            }
            std::cout << path.string() << '\n';
        }

    return 0;
}
