/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Graphics settings passed to the Renderer class
///
#pragma once

struct GfxSettings {
    int  width, height;
    bool vsync;
    bool srgbColor;
    bool resizable;
    int  gpuIndex;
    bool needTimestamps;
};