#pragma once
#include "raylib.h"
#include "BlockDictionary.h"
#include <unordered_map>
#include <string>

class TextureManager {
private:
    std::unordered_map<std::string, Texture2D> textures;
    bool isInitialized;
    
    // Reusable cube model for efficient rendering
    Model cubeModel;
    bool cubeModelLoaded;
    
public:
    TextureManager();
    ~TextureManager();
    
    // Initialize and load all textures
    bool Initialize();
    
    // Load textures from files
    bool LoadTexturesFromFiles();
    
    // Get texture for a specific block and face (now using BlockDictionary)
    Texture2D GetBlockTexture(BlockType blockType, BlockFace face = BlockFace::ALL);
    
    // Check if texture manager is ready
    bool IsInitialized() const { return isInitialized; }
    
    // Draw a textured cube (helper function)
    void DrawTexturedCube(Vector3 position, float size, BlockType blockType);
    
    // Draw a cube with per-face textures
    void DrawMultiFaceCube(Vector3 position, float size, BlockType blockType);
    
    // Cleanup
    void Cleanup();
    
private:
    // Helper methods
    bool LoadTexturesFromBlockDictionary();
    bool LoadBlockTexture(const std::string& filename, const std::string& key);
    void LoadDefaultTextures();
};

// Global texture manager instance
extern TextureManager g_textureManager;
