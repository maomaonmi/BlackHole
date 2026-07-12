#pragma once

#include <d3d11.h>

// Texture frame data — supports both GPU and CPU paths
struct TextureFrame {
    ID3D11Texture2D* d3dTex = nullptr;    // GPU path (zero-copy for D3D11 renderer)
    const void*      cpuData = nullptr;   // CPU path (for OpenGL upload)
    int              rowPitch = 0;         // Row stride in bytes
    int              width = 0;
    int              height = 0;
};

// Abstract texture source interface
class ITextureSource {
public:
    virtual ~ITextureSource() = default;
    virtual bool Init(HMONITOR hmon) = 0;
    virtual void Shutdown() = 0;
    virtual TextureFrame AcquireFrame() = 0;
    virtual void ReleaseFrame() = 0;
    virtual bool IsActive() const = 0;
};
