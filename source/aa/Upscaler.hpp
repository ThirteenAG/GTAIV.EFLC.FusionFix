#pragma once

#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <share.h>
#include <mutex>
#include <string>
#include "../upscaler/dxvk_interop.hpp"
#include <upscaler/upscaler_bridge_client.h>
#include "../upscaler/graphics_api.h"

namespace Antialiasing
{
    enum class Mode
    {
        Off, TAA, DLAA, FSR
    };

    enum class Backend : uint32_t
    {
        DLSS = static_cast<uint32_t>(UpscalerBridge::Backend::DLSS),
        FSR = static_cast<uint32_t>(UpscalerBridge::Backend::FSR),
    };

    // ---------------------------------------------------------------------------------------------------------------
    // Math

    struct Matrix
    {
        double m[4][4]{};

        static Matrix Identity()
        {
            Matrix r;
            for (int i = 0; i < 4; ++i)
                r.m[i][i] = 1.0;
            return r;
        }

        // Sixteen floats, row by row
        static Matrix From(const float* f)
        {
            Matrix r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = f[i * 4 + j];
            return r;
        }

        void To(float* f) const
        {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    f[i * 4 + j] = static_cast<float>(m[i][j]);
        }

        Matrix operator*(const Matrix& b) const
        {
            Matrix r;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = m[i][0] * b.m[0][j] + m[i][1] * b.m[1][j] + m[i][2] * b.m[2][j] + m[i][3] * b.m[3][j];
            return r;
        }

        Matrix Inverse() const
        {
            // Gauss-Jordan with partial pivoting
            Matrix a = *this;
            Matrix r = Identity();
            for (int c = 0; c < 4; ++c)
            {
                int pivot = c;
                for (int i = c + 1; i < 4; ++i)
                    if (std::abs(a.m[i][c]) > std::abs(a.m[pivot][c]))
                        pivot = i;
                if (std::abs(a.m[pivot][c]) < 1e-12)
                    return Identity();
                std::swap(a.m[c], a.m[pivot]);
                std::swap(r.m[c], r.m[pivot]);
                double inv = 1.0 / a.m[c][c];
                for (int j = 0; j < 4; ++j)
                {
                    a.m[c][j] *= inv;
                    r.m[c][j] *= inv;
                }
                for (int i = 0; i < 4; ++i)
                {
                    if (i == c)
                        continue;
                    double f = a.m[i][c];
                    for (int j = 0; j < 4; ++j)
                    {
                        a.m[i][j] -= f * a.m[c][j];
                        r.m[i][j] -= f * r.m[c][j];
                    }
                }
            }
            return r;
        }
    };

    inline float Halton(uint32_t index, uint32_t base)
    {
        float f = 1.0f;
        float r = 0.0f;
        while (index > 0)
        {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(index % base);
            index /= base;
        }
        return r;
    }

    // How far the rendered content moves this frame, in pixels of the render size: x right, y down. DLSS and FSR take
    // the same value (checked in GTA IV: with the opposite sign the noise of a still image doubles).
    struct Jitter
    {
        float X = 0.0f;
        float Y = 0.0f;
    };

    struct Options
    {
        std::wstring BridgeName;
        std::string ClientName;
        std::filesystem::path Log;
        std::filesystem::path HubLog;
        uint32_t DLSSPreset{};
        bool D3D9On12Interop{};
    };

    namespace detail
    {
        inline Options& Config()
        {
            static Options options;
            return options;
        }

        inline void Log(const char* format, ...)
        {
            static std::mutex mutex;
            static FILE* file = nullptr;
            static bool opened = false;
            std::lock_guard lock(mutex);
            if (!opened)
            {
                opened = true;
                if (!Config().Log.empty())
                    file = _wfsopen(Config().Log.c_str(), L"w", _SH_DENYWR);
            }
            if (!file)
                return;
            SYSTEMTIME t;
            GetLocalTime(&t);
            fprintf(file, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
            va_list args;
            va_start(args, format);
            vfprintf(file, format, args);
            va_end(args);
            fputc('\n', file);
            fflush(file);
        }

        template <typename T>
        void SafeRelease(T*& p)
        {
            if (p)
            {
                p->Release();
                p = nullptr;
            }
        }

        inline uint32_t AdapterVendor(IDirect3DDevice9* device)
        {
            uint32_t vendor = 0;
            IDirect3D9* d3d = nullptr;
            if (SUCCEEDED(device->GetDirect3D(&d3d)) && d3d)
            {
                D3DDEVICE_CREATION_PARAMETERS parameters{};
                D3DADAPTER_IDENTIFIER9 identifier{};
                if (SUCCEEDED(device->GetCreationParameters(&parameters)) && SUCCEEDED(d3d->GetAdapterIdentifier(parameters.AdapterOrdinal, 0, &identifier)))
                    vendor = identifier.VendorId;
                d3d->Release();
            }
            return vendor;
        }
    }
}

#include "UpscalerD3D9.inl"
