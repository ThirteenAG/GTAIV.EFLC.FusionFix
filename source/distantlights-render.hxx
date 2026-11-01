#pragma once

#include "distantlights.hxx"
#include <d3d9.h>

namespace NewDistantLights
{
    // CE's original conditional halves the aircraft basis only in TBoGT.
    // FusionFix replaces it with NOP/JMP. Read the live branch, not init order.
    inline float NativeAircraftScale(bool tbogt, std::span<const uint8_t, 2> branch)
    {
        if (!tbogt) return 1.0f;
        if (branch[0] == 0x0F && branch[1] == 0x85) return 0.5f;
        if (branch[0] == 0x90 && branch[1] == 0xE9) return 1.0f;
        return 0.0f;
    }

    template<class Roads, class Flush>
    void SubmitConsoleCoronas(bool tbogt, float nativeScale, Vector* aircraft, uint32_t* aircraftColors,
        int* aircraftCount, Vector* right, Vector* up, Roads roads, Flush flush)
    {
        // Normalize the intercepted native basis; never modify its shared vectors.
        auto roadRight = *right * (1.0f / nativeScale);
        auto roadUp = *up * (1.0f / nativeScale);
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

    // Apply after grcBegin has bound the shader's sampler, immediately around
    // DrawPrimitive. Restore the actual device state, leaving RAGE's state
    // cache and all subsequent PC draws in agreement. No texture is modified.
    template<class Device>
    class XboxCoronaSampler
    {
        static constexpr std::array<D3DSAMPLERSTATETYPE, 4> states = {
            D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_MAXMIPLEVEL };
        static constexpr std::array<DWORD, 4> values = {
            D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_NONE, 0 };
        Device* device;
        std::array<DWORD, 4> saved{};
        size_t changed{};

        void Restore()
        {
            while (changed)
            {
                --changed;
                device->SetSamplerState(0, states[changed], saved[changed]);
            }
        }

    public:
        explicit XboxCoronaSampler(Device* device) : device(device)
        {
            // Do not change anything unless every original value is known.
            for (size_t i = 0; i < states.size(); ++i)
                if (FAILED(device->GetSamplerState(0, states[i], &saved[i]))) return;
            for (size_t i = 0; i < states.size(); ++i)
            {
                ++changed;
                if (FAILED(device->SetSamplerState(0, states[i], values[i])))
                {
                    Restore();
                    return;
                }
            }
        }
        ~XboxCoronaSampler() { Restore(); }
        XboxCoronaSampler(const XboxCoronaSampler&) = delete;
        XboxCoronaSampler& operator=(const XboxCoronaSampler&) = delete;
    };

    template<class Device, class NativeFlush, class Begin, class Vertex, class End>
    void FlushCoronaBatch(bool console, Device* device, Vector* positions, uint32_t* colors,
        int* count, Vector* right, Vector* up, NativeFlush nativeFlush, Begin begin, Vertex vertex, End end)
    {
        if (!console || !device)
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
            // Xbox's corona has only the 128x128 base image. Disable mip
            // selection and force level zero of the installed PC texture.
            XboxCoronaSampler scoped(device);
            end();
        }
        *count = 0;
    }
}
