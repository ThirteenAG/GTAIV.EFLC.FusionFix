#pragma once

// Shared between the game plugin (x86) and GTAIV.EFLC.FusionFix.exe (x64), which runs DLSS and FSR.
//
// The game creates a file mapping holding UpscalerProtocol::Shared and two auto-reset events, and starts
// the helper with their base name. Every request is: the game writes the parameters and the command,
// signals <name>.Request and waits for <name>.Response. The helper answers each request exactly once.
//
// GPU synchronization uses a D3D12 fence created by the helper, which the game opens on its own queue:
// imported as a Vulkan timeline semaphore with DXVK, or opened on the D3D12 device of D3D9on12. The game
// signals WaitValue once the inputs are copied into the shared textures, the helper waits for it on its
// queue, runs the upscaler and signals SignalValue, which the game waits for before copying the output
// back. The helper answers Evaluate after its GPU work is submitted, so the game never waits on the GPU
// for a value that nobody will signal.

#include <cstdint>
#include <cstddef>

namespace UpscalerProtocol
{
    constexpr uint32_t Version = 2;
    constexpr uint32_t PathLength = 520;

    constexpr const wchar_t* ArgumentName = L"--upscaler";

    enum class Command : uint32_t
    {
        None,
        Configure,      // (re)create the shared textures and the upscaler for Backend at Width x Height
        Evaluate,       // upscale one frame
        Shutdown,
    };

    enum class Backend : uint32_t
    {
        None,
        DLSS,
        FSR,
    };

    enum class Status : uint32_t
    {
        Pending,
        Ok,
        Failed,
    };

    // Shared textures, all of them at the render size:
    // Color     DXGI_FORMAT_R16G16B16A16_FLOAT  HDR scene
    // Depth     DXGI_FORMAT_R32_FLOAT           standard [0, 1] depth
    // Motion    DXGI_FORMAT_R16G16_FLOAT        previous - current position in texture coordinates, no jitter
    // Reactive  DXGI_FORMAT_R16_FLOAT           0 to 1, how much a pixel should follow the current frame
    // Output    DXGI_FORMAT_R16G16B16A16_FLOAT  written by the upscaler
    enum class Texture : uint32_t
    {
        Color, Depth, Motion, Reactive, Output, Count
    };

    // The inputs the game copies every frame
    constexpr uint32_t InputCount = static_cast<uint32_t>(Texture::Output);

    namespace ConfigureFlags
    {
        constexpr uint32_t ReactiveMask = 1 << 0;   // Reactive is written every frame and used by FSR
    }

#pragma pack(push, 8)
    struct Shared
    {
        uint32_t Version;
        uint32_t GameProcessId;

        // Written by the game before starting the helper
        uint32_t AdapterLuidLow;
        int32_t AdapterLuidHigh;
        wchar_t GameDirectory[PathLength];
        wchar_t PluginsDirectory[PathLength];
        wchar_t LogPath[PathLength];

        // Written by the helper, answering the implicit start request
        uint32_t DLSSAvailable;
        uint32_t FSRAvailable;
        char Message[512];

        // Request
        Command RequestCommand;
        uint32_t RequestSerial;

        // Configure
        Backend ConfigureBackend;
        uint32_t Width;
        uint32_t Height;
        uint32_t DLSSPreset;          // NVSDK_NGX_DLSS_Hint_Render_Preset, 0 is the default
        uint32_t Flags;               // ConfigureFlags
        uint32_t Reserved;

        // Evaluate
        uint64_t WaitValue;           // signalled by the game when the inputs are ready
        uint64_t SignalValue;         // signalled by the helper when the output is ready
        float JitterX;                // pixels, direction the rendered content moved (y down)
        float JitterY;
        float MotionScaleX;           // converts motion vectors to pixels
        float MotionScaleY;
        float CameraNear;
        float CameraFar;
        float CameraFovY;             // radians
        float FrameTimeMs;
        float Sharpness;
        uint32_t Reset;

        // Response
        uint32_t ResponseSerial;
        Status ResponseStatus;

        // Configure results, handles already duplicated into the game process
        uint64_t TextureHandles[static_cast<size_t>(Texture::Count)];
        uint64_t TextureSizes[static_cast<size_t>(Texture::Count)];
        uint64_t FenceHandle;
    };
#pragma pack(pop)

    static_assert(sizeof(wchar_t) == 2);
    static_assert(offsetof(Shared, WaitValue) % 8 == 0);
    static_assert(offsetof(Shared, TextureHandles) % 8 == 0);
    static_assert(sizeof(Shared) == 3840, "The layout must be identical in the x86 and x64 builds");

    inline const wchar_t* MappingSuffix = L".Mapping";
    inline const wchar_t* RequestSuffix = L".Request";
    inline const wchar_t* ResponseSuffix = L".Response";
}
