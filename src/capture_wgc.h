#pragma once

#include <windows.h>
#include <d3d11.h>

struct WGCCapture {
    ID3D11Device*        d3dDev       = nullptr;
    ID3D11DeviceContext* d3dCtx       = nullptr;
    ID3D11Texture2D*     stagingTex   = nullptr;
    void*                wgcSession   = nullptr; // IGraphicsCaptureSession (opaque)
    void*                wgcPool      = nullptr; // ID3D11CaptureFramePool (opaque)
    int                  width        = 0;
    int                  height       = 0;
    bool                 active       = false;
    bool                 borderSuppressed = false;
};

// Initialize WGC capture for a specific monitor
bool WGC_Init(WGCCapture* wgc, HMONITOR hmon);

// Shutdown and release all resources
void WGC_Shutdown(WGCCapture* wgc);

// Try to get next frame (non-blocking). Returns D3D11 texture or nullptr.
// Caller must release the returned texture when done.
ID3D11Texture2D* WGC_GetFrame(WGCCapture* wgc);

// Copy D3D11 texture to staging and map for CPU readback
bool WGC_CopyToStaging(WGCCapture* wgc, ID3D11Texture2D* srcTex, D3D11_MAPPED_SUBRESOURCE* outMapped);

// Unmap staging texture after CPU read
void WGC_UnmapStaging(WGCCapture* wgc);

// Release a frame back to the pool
void WGC_ReleaseFrame(WGCCapture* wgc);

// Probe if border suppression is supported (Win11 22H2+)
bool WGC_ProbeBorderSupport();
