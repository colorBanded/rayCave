#include "TextureManager.h"
#include "BlockDictionary.h"
#include "rlgl.h"
#include <iostream>

// Global instance
TextureManager g_textureManager;

TextureManager::TextureManager() : isInitialized(false), cubeModelLoaded(false) {
}

TextureManager::~TextureManager() {
    Cleanup();
}

bool TextureManager::Initialize() {
    if (isInitialized) {
        return true;
    }
    
    std::cout << "Initializing TextureManager..." << std::endl;
    
    // Initialize BlockDictionary first
    if (!BlockDictionary::GetInstance().LoadFromFiles("assets/data/")) {
        std::cout << "Warning: Could not load block dictionary, using fallback textures." << std::endl;
    }
    
    // Create reusable cube model
    if (!cubeModelLoaded) {
        Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        cubeModel = LoadModelFromMesh(cubeMesh);
        cubeModelLoaded = true;
        std::cout << "Created reusable cube model." << std::endl;
    }
    
    // Load textures using BlockDictionary
    if (!LoadTexturesFromBlockDictionary()) {
        std::cout << "Could not load textures from BlockDictionary, using default colored textures." << std::endl;
        LoadDefaultTextures();
    }
    
    isInitialized = true;
    std::cout << "TextureManager initialized with " << textures.size() << " textures." << std::endl;
    return true;
}

Texture2D TextureManager::GetBlockTexture(BlockType blockType, BlockFace face) {
    if (!isInitialized) {
        std::cout << "TextureManager not initialized!" << std::endl;
        return Texture2D{0};
    }
    
    // Use BlockDictionary to get texture name
    std::string textureName = BlockDictionary::GetInstance().GetTextureName(blockType, face);
    
    auto it = textures.find(textureName);
    if (it != textures.end()) {
        return it->second;
    }
    
    // Fallback to "all" texture if specific face not found
    if (face != BlockFace::ALL) {
        textureName = BlockDictionary::GetInstance().GetTextureName(blockType, BlockFace::ALL);
        it = textures.find(textureName);
        if (it != textures.end()) {
            return it->second;
        }
    }
    
    // Return default texture if not found
    auto defaultIt = textures.find("default");
    if (defaultIt != textures.end()) {
        return defaultIt->second;
    }
    
    return Texture2D{0};
}

bool TextureManager::LoadTexturesFromBlockDictionary() {
    const std::string basePath = "assets/textures/blocks/";
    bool anyLoaded = false;
    
    // Get all block types from BlockDictionary
    const auto& blockTypes = BlockDictionary::GetInstance().GetAllBlockTypes();
    
    // For each block type, load its textures
    for (BlockType blockType : blockTypes) {
        const auto& properties = BlockDictionary::GetInstance().GetBlockProperties(blockType);
        
        // Try to load textures for different faces
        std::vector<BlockFace> faces = {
            BlockFace::TOP, BlockFace::BOTTOM, BlockFace::NORTH, 
            BlockFace::SOUTH, BlockFace::EAST, BlockFace::WEST, BlockFace::ALL
        };
        
        for (BlockFace face : faces) {
            std::string textureName = BlockDictionary::GetInstance().GetTextureName(blockType, face);
            
            // Skip if no texture name or already loaded
            if (textureName.empty() || textures.find(textureName) != textures.end()) {
                continue;
            }
            
            std::string filename = basePath + textureName + ".png";
            if (LoadBlockTexture(filename, textureName)) {
                anyLoaded = true;
            }
        }
    }
    
    // Create default fallback texture if we loaded any textures
    if (anyLoaded) {
        Image img = GenImageColor(16, 16, MAGENTA);
        textures["default"] = LoadTextureFromImage(img);
        UnloadImage(img);
        std::cout << "Loaded textures from BlockDictionary successfully!" << std::endl;
    }
    
    return anyLoaded;
}

bool TextureManager::LoadBlockTexture(const std::string& filename, const std::string& key) {
    Texture2D texture = LoadTexture(filename.c_str());
    if (texture.id == 0) {
        std::cout << "Failed to load texture: " << filename << std::endl;
        return false;
    }
    
    textures[key] = texture;
    std::cout << "Loaded texture: " << key << " from " << filename << std::endl;
    return true;
}

void TextureManager::LoadDefaultTextures() {
    // Create simple colored textures for now (16x16 pixels)
    // Later you can replace these with LoadBlockTexture() calls
    
    // Create a more natural grass top texture (brown dirt with green top edge)
    Image img = GenImageColor(16, 16, {101, 67, 33, 255}); // Brown dirt base
    // Add some green pixels on top to simulate grass
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 16; x++) {
            ImageDrawPixel(&img, x, y, {34, 139, 34, 255}); // Forest green
        }
    }
    textures["grass_top"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    // Create grass side texture (dirt with green edge on top)
    img = GenImageColor(16, 16, {101, 67, 33, 255}); // Brown dirt
    // Add green edge on top
    for (int x = 0; x < 16; x++) {
        ImageDrawPixel(&img, x, 0, {34, 139, 34, 255}); // Green top edge
    }
    textures["grass_side"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    // Plain dirt texture
    img = GenImageColor(16, 16, {101, 67, 33, 255}); // Brown dirt
    textures["dirt_all"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    img = GenImageColor(16, 16, GRAY);
    textures["stone_all"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    img = GenImageColor(16, 16, MAROON);
    textures["wood_all"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    img = GenImageColor(16, 16, DARKGRAY);
    textures["cobblestone_all"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    // Default fallback texture
    img = GenImageColor(16, 16, MAGENTA);
    textures["default"] = LoadTextureFromImage(img);
    UnloadImage(img);
    
    std::cout << "Generated default colored textures." << std::endl;
}

void TextureManager::Cleanup() {
    if (!isInitialized) {
        return;
    }
    
    std::cout << "Cleaning up TextureManager..." << std::endl;
    
    // Cleanup cube model
    if (cubeModelLoaded) {
        UnloadModel(cubeModel);
        cubeModelLoaded = false;
    }
    
    for (auto& pair : textures) {
        UnloadTexture(pair.second);
    }
    textures.clear();
    isInitialized = false;
}

void TextureManager::DrawMultiFaceCube(Vector3 position, float size, BlockType blockType) {
    if (!isInitialized) {
        std::cout << "TextureManager not initialized!" << std::endl;
        return;
    }
    
    float halfSize = size / 2.0f;
    
    // Get all textures once and cache them
    Texture2D topTexture, bottomTexture, sideTexture;
    
    // For grass blocks, use specific textures without green tinting
    if (blockType == BlockType::GRASS) {
        topTexture = GetBlockTexture(blockType, BlockFace::TOP);
        sideTexture = GetBlockTexture(blockType, BlockFace::NORTH);
        bottomTexture = GetBlockTexture(BlockType::DIRT, BlockFace::ALL);
        
        // Fallback logic
        if (topTexture.id == 0) topTexture = sideTexture.id > 0 ? sideTexture : GetBlockTexture(BlockType::DIRT, BlockFace::ALL);
        if (sideTexture.id == 0) sideTexture = topTexture.id > 0 ? topTexture : GetBlockTexture(BlockType::DIRT, BlockFace::ALL);
        if (bottomTexture.id == 0) bottomTexture = GetBlockTexture(BlockType::DIRT, BlockFace::ALL);
    } else {
        // For other blocks, get textures normally
        topTexture = GetBlockTexture(blockType, BlockFace::TOP);
        bottomTexture = GetBlockTexture(blockType, BlockFace::BOTTOM);
        sideTexture = GetBlockTexture(blockType, BlockFace::NORTH);
    }
    
    // Check if we have any valid textures
    if (topTexture.id == 0 && bottomTexture.id == 0 && sideTexture.id == 0) {
        // Fallback to colored cube only if no textures are available at all
        Color blockColor = MAGENTA;
        switch (blockType) {
            case BlockType::GRASS: blockColor = {101, 67, 33, 255}; break;
            case BlockType::DIRT: blockColor = BROWN; break;
            case BlockType::STONE: blockColor = GRAY; break;
            case BlockType::WOOD: blockColor = MAROON; break;
            case BlockType::COBBLESTONE: blockColor = DARKGRAY; break;
            default: blockColor = MAGENTA; break;
        }
        DrawCube(position, size, size, size, blockColor);
        return;
    }
    
    // Use fallback textures if specific ones are missing
    if (topTexture.id == 0) topTexture = sideTexture.id > 0 ? sideTexture : textures["default"];
    if (bottomTexture.id == 0) bottomTexture = sideTexture.id > 0 ? sideTexture : textures["default"];
    if (sideTexture.id == 0) sideTexture = topTexture.id > 0 ? topTexture : textures["default"];
    
    // Optimize: Group faces by texture to minimize texture binding calls
    struct FaceData {
        unsigned int textureId;
        std::vector<std::pair<Vector3*, Vector2*>> faces; // vertex and UV arrays
    };
    
    // Define vertices and UVs for each face
    Vector3 topVerts[4] = {
        {position.x - halfSize, position.y + halfSize, position.z + halfSize},
        {position.x + halfSize, position.y + halfSize, position.z + halfSize},
        {position.x + halfSize, position.y + halfSize, position.z - halfSize},
        {position.x - halfSize, position.y + halfSize, position.z - halfSize}
    };
    Vector3 bottomVerts[4] = {
        {position.x - halfSize, position.y - halfSize, position.z - halfSize},
        {position.x + halfSize, position.y - halfSize, position.z - halfSize},
        {position.x + halfSize, position.y - halfSize, position.z + halfSize},
        {position.x - halfSize, position.y - halfSize, position.z + halfSize}
    };
    Vector3 frontVerts[4] = {
        {position.x - halfSize, position.y - halfSize, position.z + halfSize},
        {position.x + halfSize, position.y - halfSize, position.z + halfSize},
        {position.x + halfSize, position.y + halfSize, position.z + halfSize},
        {position.x - halfSize, position.y + halfSize, position.z + halfSize}
    };
    Vector3 backVerts[4] = {
        {position.x - halfSize, position.y - halfSize, position.z - halfSize},
        {position.x - halfSize, position.y + halfSize, position.z - halfSize},
        {position.x + halfSize, position.y + halfSize, position.z - halfSize},
        {position.x + halfSize, position.y - halfSize, position.z - halfSize}
    };
    Vector3 rightVerts[4] = {
        {position.x + halfSize, position.y - halfSize, position.z + halfSize},
        {position.x + halfSize, position.y - halfSize, position.z - halfSize},
        {position.x + halfSize, position.y + halfSize, position.z - halfSize},
        {position.x + halfSize, position.y + halfSize, position.z + halfSize}
    };
    Vector3 leftVerts[4] = {
        {position.x - halfSize, position.y - halfSize, position.z - halfSize},
        {position.x - halfSize, position.y - halfSize, position.z + halfSize},
        {position.x - halfSize, position.y + halfSize, position.z + halfSize},
        {position.x - halfSize, position.y + halfSize, position.z - halfSize}
    };
    
    Vector2 uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    Vector2 uvs_flipped[4] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    
    // Set global color once
    rlColor4ub(255, 255, 255, 255);
    
    // Draw top face
    if (topTexture.id > 0) {
        rlSetTexture(topTexture.id);
        rlBegin(RL_QUADS);
            rlNormal3f(0.0f, 1.0f, 0.0f);
            for (int i = 0; i < 4; i++) {
                rlTexCoord2f(uvs[i].x, uvs[i].y);
                rlVertex3f(topVerts[i].x, topVerts[i].y, topVerts[i].z);
            }
        rlEnd();
    }
    
    // Draw bottom face
    if (bottomTexture.id != topTexture.id) rlSetTexture(bottomTexture.id);
    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, -1.0f, 0.0f);
        for (int i = 0; i < 4; i++) {
            rlTexCoord2f(uvs[i].x, uvs[i].y);
            rlVertex3f(bottomVerts[i].x, bottomVerts[i].y, bottomVerts[i].z);
        }
    rlEnd();
    
    // Draw all side faces with the same texture in one batch if possible
    if (sideTexture.id != bottomTexture.id) rlSetTexture(sideTexture.id);
    
    // Front face
    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);
        for (int i = 0; i < 4; i++) {
            rlTexCoord2f(uvs_flipped[i].x, uvs_flipped[i].y);
            rlVertex3f(frontVerts[i].x, frontVerts[i].y, frontVerts[i].z);
        }
    rlEnd();
    
    // Back face
    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, -1.0f);
        for (int i = 0; i < 4; i++) {
            rlTexCoord2f(uvs_flipped[i].x, uvs_flipped[i].y);
            rlVertex3f(backVerts[i].x, backVerts[i].y, backVerts[i].z);
        }
    rlEnd();
    
    // Right face
    rlBegin(RL_QUADS);
        rlNormal3f(1.0f, 0.0f, 0.0f);
        for (int i = 0; i < 4; i++) {
            rlTexCoord2f(uvs_flipped[i].x, uvs_flipped[i].y);
            rlVertex3f(rightVerts[i].x, rightVerts[i].y, rightVerts[i].z);
        }
    rlEnd();
    
    // Left face
    rlBegin(RL_QUADS);
        rlNormal3f(-1.0f, 0.0f, 0.0f);
        for (int i = 0; i < 4; i++) {
            rlTexCoord2f(uvs_flipped[i].x, uvs_flipped[i].y);
            rlVertex3f(leftVerts[i].x, leftVerts[i].y, leftVerts[i].z);
        }
    rlEnd();
    
    rlSetTexture(0); // Unbind texture
}

void TextureManager::DrawTexturedCube(Vector3 position, float size, BlockType blockType) {
    if (!isInitialized || !cubeModelLoaded) return;
    
    // Get the primary texture for this block type
    Texture2D texture = GetBlockTexture(blockType, BlockFace::ALL);
    
    // Special handling for grass - prefer the side texture if available
    if (blockType == BlockType::GRASS) {
        Texture2D grassSide = GetBlockTexture(blockType, BlockFace::NORTH);
        if (grassSide.id > 0) {
            texture = grassSide;
        } else {
            // Try grass top if side isn't available
            Texture2D grassTop = GetBlockTexture(blockType, BlockFace::TOP);
            if (grassTop.id > 0) {
                texture = grassTop;
            }
        }
    }
    
    // If no "all" texture, try to get a side texture
    if (texture.id == 0) {
        texture = GetBlockTexture(blockType, BlockFace::NORTH);
    }
    
    // If still no texture, try top texture
    if (texture.id == 0) {
        texture = GetBlockTexture(blockType, BlockFace::TOP);
    }
    
    // Fallback to default texture
    if (texture.id == 0) {
        texture = textures["default"];
    }
    
    if (texture.id > 0) {
        // Set the texture on the reusable model
        cubeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        
        // Draw the model at the specified position and size
        DrawModelEx(cubeModel, position, {0.0f, 1.0f, 0.0f}, 0.0f, {size, size, size}, WHITE);
    } else {
        // Ultimate fallback - draw colored cube
        Color blockColor = MAGENTA;
        switch (blockType) {
            case BlockType::GRASS: blockColor = GREEN; break;
            case BlockType::DIRT: blockColor = BROWN; break;
            case BlockType::STONE: blockColor = GRAY; break;
            case BlockType::WOOD: blockColor = MAROON; break;
            case BlockType::COBBLESTONE: blockColor = DARKGRAY; break;
            default: blockColor = MAGENTA; break;
        }
        DrawCube(position, size, size, size, blockColor);
    }
}
