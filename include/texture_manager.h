#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "raylib.h"
#include <unordered_map>
#include <string>
#include <vector>

// Block data structure for JSON parsing
struct BlockData {
    int id;
    std::string name;
    std::string displayName;
    bool transparent;
    bool liquid;
    bool flammable;
    bool breakable;
    bool emitsLight;
    float hardness;
    int lightLevel;
    std::string soundGroup;
    std::string toolRequired;
    
    // Texture mappings
    std::string topTexture;
    std::string bottomTexture;
    std::string sideTexture;
    std::string allTexture;
    
    // Tint color (RGBA)
    Color tintColor;
};

// Texture atlas system for efficient texture management
class TextureManager {
private:
    std::unordered_map<std::string, Texture2D> textures;
    std::unordered_map<int, BlockData> blockData; // Block ID to block data mapping
    Material defaultMaterial;
    std::string textureBasePath;
    
    // Helper function for JSON parsing
    BlockData ParseBlockFromJson(const std::string& blockJson);
    
public:
    TextureManager(const std::string& basePath = "assets/textures/blocks/");
    ~TextureManager();
    
    // Block data loading
    bool LoadBlockData(const std::string& jsonFilePath = "assets/data/blocks.json");
    
    // Texture loading
    bool LoadTexture(const std::string& name, const std::string& filename);
    bool LoadTexture(const std::string& name); // Uses name.png as filename
    void LoadCommonTextures(); // Load commonly used block textures
    void LoadTexturesFromBlockData(); // Load textures based on block data
    
    // Texture access
    Texture2D GetTexture(const std::string& name) const;
    Material CreateMaterial(const std::string& textureName);
    bool HasTexture(const std::string& name) const;
    
    // Block data access
    const BlockData* GetBlockData(int voxelType) const;
    std::string GetTextureNameForVoxel(int voxelType, int face = -1) const;
    
    // GUI texture helpers
    void DrawHotbarSlot(int x, int y, bool selected = false) const;
    void DrawHotbar(int centerX, int y, int selectedSlot = 0) const;
    
    // Cleanup
    void UnloadAll();
};

#endif // TEXTURE_MANAGER_H
