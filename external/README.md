# External code

Submodules (see .gitmodules). The Vulkan headers are the `Vulkan-Headers` submodule.

DLAA and FSR run in FusionFixHub.exe (https://github.com/ThirteenAG/FusionFixHub-App), which keeps the NVIDIA DLSS and
AMD FidelityFX SDK files; the plugin only has the bridge's client headers (FusionFixUtils `upscaler/`).
The vendor runtimes and DXVK are not in the release package: FusionFixHub downloads them.
