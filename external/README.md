# Vendored SDK subsets

Only the files needed to build `GTAIV.EFLC.FusionFix.exe` (the 64-bit DLSS/FSR helper) are kept here,
copied unmodified from the official repositories. The full repositories are 250 MB (FidelityFX SDK)
and 1.4 GB (DLSS SDK) per checkout, almost entirely prebuilt libraries for other platforms.

| Folder | Source | Version | Commit |
|---|---|---|---|
| `dlss` | https://github.com/NVIDIA/DLSS | v310.9.1 | 374959484e79a640feaba44c93ac8cfb0a03f5b5 |
| `fidelityfx` | https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK (`Kits/FidelityFX`) | v2.3.0 | 60f4ea81909200d8542eca14dccb2628b763a9a3 |

- `dlss`: the NGX headers used for D3D12 and `lib/Windows_x86_64/x64/nvsdk_ngx_s.lib`, NVIDIA's static
  loader for the NGX runtime installed with the driver. License: `dlss/LICENSE.txt`.
- `fidelityfx`: the FidelityFX API headers for D3D12 and the upscaler. License: `fidelityfx/license.md`.

The runtimes are not kept here. `before_packaging.bat` downloads them into `data/plugins` for the release
package, from the same versions: `nvngx_dlss.dll` (DLSS `lib/Windows_x86_64/rel`) and
`amd_fidelityfx_loader_dx12.dll` with `amd_fidelityfx_upscaler_dx12.dll` (FidelityFX
`Kits/FidelityFX/signedbin`). The helper loads them from the plugins or the game folder. When the headers
here are updated, update the versions in `before_packaging.bat` as well.

The Vulkan headers are the `Vulkan-Headers` submodule.
