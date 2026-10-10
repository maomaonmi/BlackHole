#pragma once

// Sun texture loading/generation
// Supports both procedural generation and external image loading

struct StarTexture {
    unsigned int texID;     // OpenGL texture ID
    int width;
    int height;
    bool valid;
};

// Generate a procedural sun texture (fallback if no image file)
StarTexture StarTexture_GenerateProcedural(int size);

// Load sun texture from image file (jpg/png/bmp) - requires stb_image.h
StarTexture StarTexture_LoadFromFile(const char* path);

// Free texture
void StarTexture_Free(StarTexture* tex);

// Get OpenGL texture ID
unsigned int StarTexture_GetID(const StarTexture* tex);