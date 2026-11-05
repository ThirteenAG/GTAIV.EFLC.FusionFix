#pragma once

#include "distantlights.hxx"

namespace DistantLights
{
    // Read the live TBoGT branch: original JNE halves the basis; FusionFix's
    // NOP/JMP bypasses it. Zero denotes an unsupported patch.
    inline float NativeAircraftScale(bool tbogt, std::span<const uint8_t, 2> branch)
    {
        if (!tbogt)
            return 1.0f;
        if (branch[0] == 0x0F && branch[1] == 0x85)
            return 0.5f;
        if (branch[0] == 0x90 && branch[1] == 0xE9)
            return 1.0f;
        return 0.0f;
    }

    template<class Roads, class Flush>
    void SubmitConsoleCoronas(bool tbogt, float nativeScale, Vector* aircraft, uint32_t* aircraftColors,
        int* aircraftCount, Vector* right, Vector* up, Roads roads, Flush flush)
    {
        // Normalize the intercepted native basis; never modify its shared vectors.
        auto roadRight = *right * (1.0f / nativeScale);
        auto roadUp = *up * (1.0f / nativeScale);
        // Private Console batch; CE's native StoreSpriteInBuffer holds 1,820 sprites.
        std::array<Vector, 64> positions;
        std::array<uint32_t, 64> colors;
        int count = 0;
        roads([&](Vector light, uint32_t color)
        {
            positions[count] = light;
            colors[count++] = color;
            if (count == int(positions.size()))
                flush(positions.data(), colors.data(), &count, &roadRight, &roadUp);
        });
        // Finish every road batch before applying the aircraft-only size change.
        flush(positions.data(), colors.data(), &count, &roadRight, &roadUp);
        auto aircraftRight = roadRight * (tbogt ? 0.5f : 1.0f);
        auto aircraftUp = roadUp * (tbogt ? 0.5f : 1.0f);
        flush(aircraft, aircraftColors, aircraftCount, &aircraftRight, &aircraftUp);
    }

    template<class NativeFlush, class Begin, class Vertex, class End>
    void FlushCoronaBatch(bool console, Vector* positions, uint32_t* colors,
        int* count, Vector* right, Vector* up, NativeFlush nativeFlush, Begin begin, Vertex vertex, End end)
    {
        if (!console)
        {
            nativeFlush(positions, colors, count, right, up);
            return;
        }
        if (*count > 0)
        {
            // PC's original two triangles, corner arithmetic and winding;
            // retail Xbox maps V oppositely at each corresponding corner.
            const auto difference = *right - *up;
            const auto sum = *right + *up;
            begin(3, 6 * *count);
            for (int i = 0; i < *count; ++i)
            {
                const auto a = positions[i] - difference;
                const auto b = positions[i] + difference;
                const auto c = positions[i] - sum;
                const auto d = positions[i] + sum;
                auto emit = [&](const Vector& p, float u, float v)
                {
                    vertex(p.x, p.y, p.z, 0.0f, 0.0f, -1.0f, colors[i], u, v);
                };
                emit(a, 0.0f, 0.0f);
                emit(b, 1.0f, 1.0f);
                emit(c, 0.0f, 1.0f);
                emit(b, 1.0f, 1.0f);
                emit(d, 1.0f, 0.0f);
                emit(a, 0.0f, 0.0f);
            }
            end();
        }
        *count = 0;
    }
}
