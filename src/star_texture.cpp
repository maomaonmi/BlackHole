// stb_image.h must be included BEFORE any Windows headers
// to avoid macro conflicts (min/max, etc.)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Now safe to include Windows/GL headers
#include "win32_gl.h"
#include "star_texture.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>

// GL function declarations
static void (*s_glGenTextures)(int, unsigned int*) = nullptr;
static void (*s_glBindTexture)(unsigned int, unsigned int) = nullptr;
static void (*s_glTexImage2D)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) = nullptr;
static void (*s_glTexParameteri)(unsigned int, unsigned int, int) = nullptr;
static void (*s_glDeleteTextures)(int, const unsigned int*) = nullptr;

static bool s_texFuncsLoaded = false;

static void LoadTexFunctions() {
    if (s_texFuncsLoaded) return;
    s_glGenTextures = (void(*)(int,unsigned int*))Win32GL_GetProcAddress("glGenTextures");
    s_glBindTexture = (void(*)(unsigned int,unsigned int))Win32GL_GetProcAddress("glBindTexture");
    s_glTexImage2D = (void(*)(unsigned int,int,int,int,int,int,unsigned int,unsigned int,const void*))Win32GL_GetProcAddress("glTexImage2D");
    s_glTexParameteri = (void(*)(unsigned int,unsigned int,int))Win32GL_GetProcAddress("glTexParameteri");
    s_glDeleteTextures = (void(*)(int,const unsigned int*))Win32GL_GetProcAddress("glDeleteTextures");
    s_texFuncsLoaded = true;
}

// Convert RGBA image to circular sun texture (alpha = 0 outside sphere)
static float smoothstepf(float edge0, float edge1, float x) {
    float t = fmaxf(0.0f, fminf(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

static unsigned char* MakeCircularSun(const unsigned char* rgba, int w, int h, int* outW, int* outH) {
    // Step 1: Find bounding box of bright pixels (the actual sun)
    int minX = w, minY = h, maxX = 0, maxY = 0;
    int brightCount = 0;
    const int brightnessThreshold = 40;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 4;
            float brightness = (rgba[idx] + rgba[idx+1] + rgba[idx+2]) / 3.0f;
            if (brightness > brightnessThreshold) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
                brightCount++;
            }
        }
    }

    if (brightCount < 100) {
        printf("[StarTex] Warning: too few bright pixels (%d), using full image\n", brightCount);
        minX = 0; minY = 0; maxX = w - 1; maxY = h - 1;
    }

    // Add small padding
    int padX = (maxX - minX) / 20;
    int padY = (maxY - minY) / 20;
    if (padX < 2) padX = 2;
    if (padY < 2) padY = 2;
    minX -= padX; if (minX < 0) minX = 0;
    minY -= padY; if (minY < 0) minY = 0;
    maxX += padX; if (maxX >= w) maxX = w - 1;
    maxY += padY; if (maxY >= h) maxY = h - 1;

    int cropW = maxX - minX + 1;
    int cropH = maxY - minY + 1;
    int cx = minX + cropW / 2;
    int cy = minY + cropH / 2;
    int radius = (cropW < cropH ? cropW : cropH) / 2;

    printf("[StarTex] Sun bbox: (%d,%d)-(%d,%d) size=%dx%d, radius=%d\n",
           minX, minY, maxX, maxY, cropW, cropH, radius);

    // Output is a square texture of size radius*2
    int sz = radius * 2;
    unsigned char* out = (unsigned char*)calloc(sz * sz * 4, 1);

    for (int y = 0; y < sz; y++) {
        for (int x = 0; x < sz; x++) {
            float nx = ((float)x - radius + 0.5f) / radius;
            float ny = ((float)y - radius + 0.5f) / radius;
            float r2 = nx * nx + ny * ny;

            int idx = (y * sz + x) * 4;

            if (r2 > 1.0f) {
                out[idx + 0] = 0;
                out[idx + 1] = 0;
                out[idx + 2] = 0;
                out[idx + 3] = 0;
            } else {
                // Map circle to cropped image region
                float u = (nx * 0.5f + 0.5f);
                float v = (ny * 0.5f + 0.5f);

                float fx = minX + u * (cropW - 1);
                float fy = minY + v * (cropH - 1);
                int ix = (int)fx, iy = (int)fy;
                float fracX = fx - ix, fracY = fy - iy;

                ix = ix < 0 ? 0 : (ix >= w - 1 ? w - 2 : ix);
                iy = iy < 0 ? 0 : (iy >= h - 1 ? h - 2 : iy);

                int i00 = (iy * w + ix) * 4;
                int i10 = (iy * w + ix + 1) * 4;
                int i01 = ((iy + 1) * w + ix) * 4;
                int i11 = ((iy + 1) * w + ix + 1) * 4;

                for (int c = 0; c < 3; c++) {
                    float v00 = rgba[i00 + c];
                    float v10 = rgba[i10 + c];
                    float v01 = rgba[i01 + c];
                    float v11 = rgba[i11 + c];

                    float v0 = v00 + (v10 - v00) * fracX;
                    float v1 = v01 + (v11 - v01) * fracX;
                    float val = v0 + (v1 - v0) * fracY;

                    out[idx + c] = (unsigned char)val;
                }

                // Limb darkening on texture edges: darken bright pixels near boundary
                float r = sqrtf(r2);
                float limbDark = sqrtf(fmaxf(1.0f - r2, 0.0f));
                for (int c = 0; c < 3; c++) {
                    out[idx + c] = (unsigned char)(out[idx + c] * limbDark);
                }

                // Hard alpha: fully opaque inside, transparent outside
                out[idx + 3] = 255;
            }
        }
    }

    *outW = sz;
    *outH = sz;
    return out;
}

// Upload RGBA data as OpenGL texture
static StarTexture UploadTexture(unsigned char* data, int w, int h) {
    StarTexture tex = {0, w, h, false};
    if (!s_glGenTextures) return tex;

    s_glGenTextures(1, &tex.texID);
    s_glBindTexture(0x0DE1, tex.texID);
    s_glTexImage2D(0x0DE1, 0, 4, w, h, 0, 0x1908, 0x1401, data);

    s_glTexParameteri(0x0DE1, 0x2801, 0x2601); // MIN_LINEAR
    s_glTexParameteri(0x0DE1, 0x2800, 0x2601); // MAG_LINEAR
    s_glTexParameteri(0x0DE1, 0x2802, 0x812F); // WRAP_S CLAMP
    s_glTexParameteri(0x0DE1, 0x2803, 0x812F); // WRAP_T CLAMP
    s_glBindTexture(0x0DE1, 0);

    tex.valid = true;
    return tex;
}

// Load sun texture from image file using stb_image
StarTexture StarTexture_LoadFromFile(const char* path) {
    LoadTexFunctions();
    if (!s_glGenTextures) return {0, 0, 0, false};

    int w, h, channels;
    unsigned char* img = stbi_load(path, &w, &h, &channels, 4);
    if (!img) {
        printf("[StarTex] Failed to load: %s (%s)\n", path, stbi_failure_reason());
        return {0, 0, 0, false};
    }

    printf("[StarTex] Loaded image: %s (%dx%d, %d channels)\n", path, w, h, channels);

    // Convert to circular sun texture
    int texW, texH;
    unsigned char* sunData = MakeCircularSun(img, w, h, &texW, &texH);
    stbi_image_free(img);

    StarTexture tex = UploadTexture(sunData, texW, texH);
    free(sunData);

    if (tex.valid) {
        printf("[StarTex] Sun texture ready: %dx%d, texID=%u\n", texW, texH, tex.texID);
    }
    return tex;
}

// ---- Procedural fallback ----
static float hash2f(float x, float y) {
    return fmodf(sinf(x * 127.1f + y * 311.7f) * 43758.5453f, 1.0f);
}

static float noise2f(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = hash2f((float)ix, (float)iy);
    float b = hash2f((float)(ix + 1), (float)iy);
    float c = hash2f((float)ix, (float)(iy + 1));
    float d = hash2f((float)(ix + 1), (float)(iy + 1));
    return a + (b - a) * fx + (c - a) * fy + (d - b - c + a) * fx * fy;
}

static float fbm2f(float x, float y, int octaves) {
    float v = 0.0f, a = 0.5f;
    for (int i = 0; i < octaves; i++) {
        v += a * noise2f(x, y);
        x *= 2.0f; y *= 2.0f;
        a *= 0.5f;
    }
    return v;
}

static float voronoi2f(float x, float y) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float minDist = 1e6f;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int cx = ix + dx, cy = iy + dy;
            float px = (float)cx + hash2f((float)cx, (float)cy) * 0.8f;
            float py = (float)cy + hash2f((float)(cx + 100), (float)(cy + 100)) * 0.8f;
            float d = sqrtf((x - px) * (x - px) + (y - py) * (y - py));
            if (d < minDist) minDist = d;
        }
    }
    return minDist;
}

StarTexture StarTexture_GenerateProcedural(int size) {
    LoadTexFunctions();
    if (!s_glGenTextures) return {0, 0, 0, false};

    std::vector<unsigned char> data(size * size * 4);
    float invSize = 1.0f / (float)size;
    float halfSize = (float)size * 0.5f;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float nx = ((float)x - halfSize) * invSize;
            float ny = ((float)y - halfSize) * invSize;
            float r2 = nx * nx + ny * ny;
            float r = sqrtf(r2);
            int idx = (y * size + x) * 4;

            if (r > 0.98f) {
                data[idx + 0] = 0; data[idx + 1] = 0; data[idx + 2] = 0; data[idx + 3] = 0;
                continue;
            }

            float cosTheta = sqrtf(fmaxf(1.0f - r2, 0.0f));
            float scale = 1.0f / fmaxf(cosTheta, 0.05f);
            float su = nx * scale * 8.0f;
            float sv = ny * scale * 8.0f;
            float limbDark = 0.3f + 0.7f * cosTheta;

            float cellDist = voronoi2f(su + 0.5f, sv + 0.5f);
            float granule = 1.0f - smoothstepf(0.02f, 0.35f, cellDist);
            granule = powf(granule, 0.6f + hash2f(floorf(su), floorf(sv)) * 0.4f);

            float largeScale = fbm2f(su * 0.3f, sv * 0.3f, 4);
            largeScale = 0.7f + 0.3f * largeScale;

            float spotNoise = fbm2f(su * 0.15f + 50.0f, sv * 0.15f + 50.0f, 5);
            float spot = smoothstepf(0.55f, 0.65f, spotNoise) * 0.4f;

            float turb = fbm2f(su * 2.0f, sv * 2.0f, 4) - 0.5f;

            float intensity = granule * 0.6f + 0.4f;
            intensity *= largeScale;
            intensity -= spot;
            intensity += turb * 0.08f;
            intensity *= limbDark;
            intensity = fmaxf(0.05f, fminf(1.2f, intensity));

            float edgeRedShift = (1.0f - cosTheta) * 0.15f;
            float ri = intensity * (1.0f + edgeRedShift);
            float gi = intensity * 0.92f;
            float bi = intensity * 0.55f - edgeRedShift * 0.2f;
            ri = fmaxf(0.0f, fminf(1.0f, ri));
            gi = fmaxf(0.0f, fminf(1.0f, gi));
            bi = fmaxf(0.0f, fminf(1.0f, bi));

            data[idx + 0] = (unsigned char)(ri * 255);
            data[idx + 1] = (unsigned char)(gi * 255);
            data[idx + 2] = (unsigned char)(bi * 255);
            data[idx + 3] = (unsigned char)(limbDark * 255);
        }
    }

    return UploadTexture(data.data(), size, size);
}

void StarTexture_Free(StarTexture* tex) {
    if (tex && tex->valid && s_glDeleteTextures) {
        s_glDeleteTextures(1, &tex->texID);
        tex->valid = false;
        tex->texID = 0;
    }
}

unsigned int StarTexture_GetID(const StarTexture* tex) {
    return tex ? tex->texID : 0;
}
