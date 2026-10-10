#include "gl_texture.h"
#include "win32_gl.h"  // For Win32GL_GetProcAddress
#include <cstdio>
#include <cstring>

// Dynamically loaded GL functions needed here
static void (*s_glGenTextures)(int, unsigned int*) = nullptr;
static void (*s_glDeleteTextures)(int, const unsigned int*) = nullptr;
static void (*s_glBindTexture)(unsigned int, unsigned int) = nullptr;
static void (*s_glTexParameteri)(unsigned int, unsigned int, int) = nullptr;
static void (*s_glTexImage2D)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) = nullptr;
static void (*s_glTexSubImage2D)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) = nullptr;
static void (*s_glPixelStorei)(unsigned int, int) = nullptr;

static bool s_loaded = false;

static void EnsureGLLoaded() {
    if (s_loaded) return;
    s_loaded = true;
    // Use Win32GL_GetProcAddress which handles both core and extension functions
    s_glGenTextures   = (void(*)(int,unsigned int*))Win32GL_GetProcAddress("glGenTextures");
    s_glDeleteTextures = (void(*)(int,const unsigned int*))Win32GL_GetProcAddress("glDeleteTextures");
    s_glBindTexture   = (void(*)(unsigned int,unsigned int))Win32GL_GetProcAddress("glBindTexture");
    s_glTexParameteri = (void(*)(unsigned int,unsigned int,int))Win32GL_GetProcAddress("glTexParameteri");
    s_glTexImage2D    = (void(*)(unsigned int,int,int,int,int,int,unsigned int,unsigned int,const void*))Win32GL_GetProcAddress("glTexImage2D");
    s_glTexSubImage2D = (void(*)(unsigned int,int,int,int,int,int,unsigned int,unsigned int,const void*))Win32GL_GetProcAddress("glTexSubImage2D");
    s_glPixelStorei   = (void(*)(unsigned int,int))Win32GL_GetProcAddress("glPixelStorei");
    
    // Verify critical functions
    if (!s_glGenTextures || !s_glBindTexture || !s_glTexImage2D) {
        fprintf(stderr, "[GLTex] Failed to load critical GL texture functions\n");
    }
}

GLTextureUpload GLTex_Init(int width, int height) {
    EnsureGLLoaded();
    GLTextureUpload glt = {};
    glt.width = width;
    glt.height = height;

    s_glGenTextures(1, &glt.tex);
    s_glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, glt.tex);

    s_glTexParameteri(0x0DE1, 0x2801/*GL_TEXTURE_MIN_FILTER*/, 0x2601/*GL_LINEAR*/);
    s_glTexParameteri(0x0DE1, 0x2800/*GL_TEXTURE_MAG_FILTER*/, 0x2601/*GL_LINEAR*/);
    s_glTexParameteri(0x0DE1, 0x2802/*GL_TEXTURE_WRAP_S*/, 0x812F/*GL_CLAMP_TO_EDGE*/);
    s_glTexParameteri(0x0DE1, 0x2803/*GL_TEXTURE_WRAP_T*/, 0x812F/*GL_CLAMP_TO_EDGE*/);

    // Initialize with grey placeholder
    unsigned char grey = 80;
    s_glTexImage2D(0x0DE1, 0, 0x1908/*GL_RGBA*/, width, height, 0,
                   0x80E1/*GL_BGRA_EXT*/, 0x1401/*GL_UNSIGNED_BYTE*/, nullptr);

    s_glBindTexture(0x0DE1, 0);
    glt.active = true;
    return glt;
}

void GLTex_Shutdown(GLTextureUpload* glt) {
    if (!glt || !glt->active) return;
    EnsureGLLoaded();
    if (glt->tex) s_glDeleteTextures(1, &glt->tex);
    glt->tex = 0;
    glt->active = false;
}

void GLTex_Upload(GLTextureUpload* glt, const void* data, int stride) {
    if (!glt || !glt->active || !data) return;
    EnsureGLLoaded();

    s_glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, glt->tex);

    int expectedStride = glt->width * 4;
    if (stride == expectedStride) {
        // Tight packing — single call
        s_glTexSubImage2D(0x0DE1, 0, 0, 0, glt->width, glt->height,
                          0x80E1/*GL_BGRA_EXT*/, 0x1401/*GL_UNSIGNED_BYTE*/, data);
    } else {
        // Row padding — set UNPACK_ROW_LENGTH
        if (s_glPixelStorei) s_glPixelStorei(0x0CF2/*GL_UNPACK_ROW_LENGTH*/, stride / 4);
        s_glTexSubImage2D(0x0DE1, 0, 0, 0, glt->width, glt->height,
                          0x80E1/*GL_BGRA_EXT*/, 0x1401/*GL_UNSIGNED_BYTE*/, data);
        if (s_glPixelStorei) s_glPixelStorei(0x0CF2/*GL_UNPACK_ROW_LENGTH*/, 0);
    }

    s_glBindTexture(0x0DE1, 0);
}

void GLTex_UploadRegion(GLTextureUpload* glt, const void* data, int stride, int dstX, int dstY, int regionW, int regionH) {
    if (!glt || !glt->active || !data) return;
    EnsureGLLoaded();

    s_glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, glt->tex);

    int expectedStride = regionW * 4;
    if (stride != expectedStride && s_glPixelStorei) {
        s_glPixelStorei(0x0CF2/*GL_UNPACK_ROW_LENGTH*/, stride / 4);
    }
    s_glTexSubImage2D(0x0DE1, 0, dstX, dstY, regionW, regionH,
                      0x80E1/*GL_BGRA_EXT*/, 0x1401/*GL_UNSIGNED_BYTE*/, data);
    if (stride != expectedStride && s_glPixelStorei) {
        s_glPixelStorei(0x0CF2/*GL_UNPACK_ROW_LENGTH*/, 0);
    }

    s_glBindTexture(0x0DE1, 0);
}

void GLTex_Resize(GLTextureUpload* glt, int newW, int newH) {
    if (!glt || !glt->active) return;
    EnsureGLLoaded();

    s_glBindTexture(0x0DE1/*GL_TEXTURE_2D*/, glt->tex);
    s_glTexImage2D(0x0DE1, 0, 0x1908/*GL_RGBA*/, newW, newH, 0,
                   0x80E1/*GL_BGRA_EXT*/, 0x1401/*GL_UNSIGNED_BYTE*/, nullptr);
    s_glBindTexture(0x0DE1, 0);

    glt->width = newW;
    glt->height = newH;
}

unsigned int GLTex_GetTexture(const GLTextureUpload* glt) {
    return glt ? glt->tex : 0;
}
