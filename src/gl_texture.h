#pragma once

#include <windows.h>
// Do NOT include gl/gl.h here - it declares gl* functions that conflict
// with our dynamic loading declarations in main.cpp

struct GLTextureUpload {
    unsigned int tex = 0;
    int width = 0;
    int height = 0;
    bool active = false;
};

GLTextureUpload GLTex_Init(int width, int height);
void GLTex_Shutdown(GLTextureUpload* glt);
void GLTex_Upload(GLTextureUpload* glt, const void* data, int stride);
void GLTex_UploadRegion(GLTextureUpload* glt, const void* data, int stride, int dstX, int dstY, int regionW, int regionH);
void GLTex_Resize(GLTextureUpload* glt, int newW, int newH);
unsigned int GLTex_GetTexture(const GLTextureUpload* glt);
