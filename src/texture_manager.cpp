#include "../include/texture_manager.h"
#include "../include/voxel.h"
#include <iostream>
#include <fstream>
#include <set>

TextureManager::TextureManager(const std::string& basePath) 
    : textureBasePath(basePath) {
    defaultMaterial = LoadMaterialDefault();
    
    // Try to load block data from JSON first
    if (LoadBlockData()) {
        LoadTexturesFromBlockData(); // Load textures based on block data
    } else {
        // Fallback to hardcoded texture loading if JSON fails
        std::cout << "Failed to load block data, falling back to hardcoded textures" << std::endl;
        LoadCommonTextures();
    }
}

TextureManager::~TextureManager() {
    UnloadAll();
}

bool TextureManager::LoadBlockData(const std::string& jsonFilePath) {
    std::cout << "Attempting to load block data from: " << jsonFilePath << std::endl;
    
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        std::cout << "Failed to open blocks.json file: " << jsonFilePath << std::endl;
        return false;
    }
    
    // Read the entire file
    std::string jsonContent((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    file.close();
    
    std::cout << "JSON file size: " << jsonContent.size() << " bytes" << std::endl;
    
    // Simple JSON parsing for our specific structure
    try {
        // Find the blocks array
        size_t blocksStart = jsonContent.find("\"blocks\":");
        if (blocksStart == std::string::npos) {
            std::cout << "Could not find 'blocks' array in JSON" << std::endl;
            return false;
        }
        
        // Find the opening bracket of the blocks array
        size_t arrayStart = jsonContent.find('[', blocksStart);
        if (arrayStart == std::string::npos) {
            std::cout << "Could not find opening bracket for blocks array" << std::endl;
            return false;
        }
        
        // Parse each block object
        size_t pos = arrayStart + 1;
        int blockCount = 0;
        
        while (pos < jsonContent.length()) {
            // Skip whitespace
            while (pos < jsonContent.length() && (jsonContent[pos] == ' ' || jsonContent[pos] == '\n' || jsonContent[pos] == '\t' || jsonContent[pos] == '\r')) {
                pos++;
            }
            
            // Check if we've reached the end of the array
            if (pos >= jsonContent.length() || jsonContent[pos] == ']') {
                break;
            }
            
            // Find the start of the next block object
            if (jsonContent[pos] == '{') {
                // Find the end of this block object
                size_t blockStart = pos;
                int braceCount = 1;
                pos++;
                
                while (pos < jsonContent.length() && braceCount > 0) {
                    if (jsonContent[pos] == '{') braceCount++;
                    else if (jsonContent[pos] == '}') braceCount--;
                    pos++;
                }
                
                // Extract the block JSON string
                std::string blockJson = jsonContent.substr(blockStart, pos - blockStart);
                
                // Parse this block
                BlockData block = ParseBlockFromJson(blockJson);
                if (block.id >= 0) { // Valid block
                    blockData[block.id] = block;
                    blockCount++;
                    std::cout << "Loaded block: " << block.name << " (ID: " << block.id << ")" << std::endl;
                }
            }
            
            // Skip comma and whitespace
            while (pos < jsonContent.length() && (jsonContent[pos] == ',' || jsonContent[pos] == ' ' || jsonContent[pos] == '\n' || jsonContent[pos] == '\t' || jsonContent[pos] == '\r')) {
                pos++;
            }
        }
        
        std::cout << "Successfully loaded " << blockCount << " blocks from JSON" << std::endl;
        return blockCount > 0;
        
    } catch (const std::exception& e) {
        std::cout << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

BlockData TextureManager::ParseBlockFromJson(const std::string& blockJson) {
    BlockData block;
    block.id = -1; // Invalid by default
    
    try {
        // Helper function to extract string value
        auto extractString = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":";
            size_t keyPos = blockJson.find(search);
            if (keyPos == std::string::npos) return "";
            
            size_t valueStart = blockJson.find("\"", keyPos + search.length());
            if (valueStart == std::string::npos) return "";
            valueStart++;
            
            size_t valueEnd = blockJson.find("\"", valueStart);
            if (valueEnd == std::string::npos) return "";
            
            return blockJson.substr(valueStart, valueEnd - valueStart);
        };
        
        // Helper function to extract integer value
        auto extractInt = [&](const std::string& key) -> int {
            std::string search = "\"" + key + "\":";
            size_t keyPos = blockJson.find(search);
            if (keyPos == std::string::npos) return 0;
            
            size_t valueStart = keyPos + search.length();
            while (valueStart < blockJson.length() && (blockJson[valueStart] == ' ' || blockJson[valueStart] == '\t')) {
                valueStart++;
            }
            
            size_t valueEnd = valueStart;
            while (valueEnd < blockJson.length() && (std::isdigit(blockJson[valueEnd]) || blockJson[valueEnd] == '-' || blockJson[valueEnd] == '.')) {
                valueEnd++;
            }
            
            if (valueEnd > valueStart) {
                return std::stoi(blockJson.substr(valueStart, valueEnd - valueStart));
            }
            return 0;
        };
        
        // Helper function to extract boolean value
        auto extractBool = [&](const std::string& key) -> bool {
            std::string search = "\"" + key + "\":";
            size_t keyPos = blockJson.find(search);
            if (keyPos == std::string::npos) return false;
            
            size_t valueStart = keyPos + search.length();
            while (valueStart < blockJson.length() && (blockJson[valueStart] == ' ' || blockJson[valueStart] == '\t')) {
                valueStart++;
            }
            
            return blockJson.substr(valueStart, 4) == "true";
        };
        
        // Helper function to extract float value
        auto extractFloat = [&](const std::string& key) -> float {
            std::string search = "\"" + key + "\":";
            size_t keyPos = blockJson.find(search);
            if (keyPos == std::string::npos) return 0.0f;
            
            size_t valueStart = keyPos + search.length();
            while (valueStart < blockJson.length() && (blockJson[valueStart] == ' ' || blockJson[valueStart] == '\t')) {
                valueStart++;
            }
            
            size_t valueEnd = valueStart;
            while (valueEnd < blockJson.length() && (std::isdigit(blockJson[valueEnd]) || blockJson[valueEnd] == '-' || blockJson[valueEnd] == '.')) {
                valueEnd++;
            }
            
            if (valueEnd > valueStart) {
                return std::stof(blockJson.substr(valueStart, valueEnd - valueStart));
            }
            return 0.0f;
        };
        
        // Parse basic properties
        block.id = extractInt("id");
        block.name = extractString("name");
        block.displayName = extractString("displayName");
        block.transparent = extractBool("transparent");
        block.liquid = extractBool("liquid");
        block.flammable = extractBool("flammable");
        block.breakable = extractBool("breakable");
        block.emitsLight = extractBool("emitsLight");
        block.hardness = extractFloat("hardness");
        block.lightLevel = extractInt("lightLevel");
        block.soundGroup = extractString("soundGroup");
        block.toolRequired = extractString("toolRequired");
        
        // Parse textures object
        size_t texturesStart = blockJson.find("\"textures\":");
        if (texturesStart != std::string::npos) {
            size_t objStart = blockJson.find('{', texturesStart);
            if (objStart != std::string::npos) {
                size_t objEnd = blockJson.find('}', objStart);
                if (objEnd != std::string::npos) {
                    std::string texturesJson = blockJson.substr(objStart + 1, objEnd - objStart - 1);
                    
                    // Extract texture values
                    auto extractTextureValue = [&](const std::string& texKey) -> std::string {
                        std::string search = "\"" + texKey + "\":";
                        size_t keyPos = texturesJson.find(search);
                        if (keyPos == std::string::npos) return "";
                        
                        size_t valueStart = texturesJson.find("\"", keyPos + search.length());
                        if (valueStart == std::string::npos) return "";
                        valueStart++;
                        
                        size_t valueEnd = texturesJson.find("\"", valueStart);
                        if (valueEnd == std::string::npos) return "";
                        
                        return texturesJson.substr(valueStart, valueEnd - valueStart);
                    };
                    
                    block.allTexture = extractTextureValue("all");
                    block.topTexture = extractTextureValue("top");
                    block.bottomTexture = extractTextureValue("bottom");
                    block.sideTexture = extractTextureValue("side");
                }
            }
        }
        
        // Parse tint color (simple approach - just set to white for now)
        block.tintColor = {255, 255, 255, 255};
        
    } catch (const std::exception& e) {
        std::cout << "Error parsing block JSON: " << e.what() << std::endl;
        block.id = -1; // Mark as invalid
    }
    
    return block;
}

bool TextureManager::LoadTexture(const std::string& name, const std::string& filename) {
    std::string fullPath = textureBasePath + filename;
    
    // Check if texture already loaded
    if (textures.find(name) != textures.end()) {
        return true;
    }
    
    Texture2D texture = ::LoadTexture(fullPath.c_str());
    if (texture.id == 0) {
        std::cout << "Failed to load texture: " << fullPath << std::endl;
        return false;
    }
    
    textures[name] = texture;
    std::cout << "Loaded texture: " << name << " from " << fullPath << std::endl;
    return true;
}

bool TextureManager::LoadTexture(const std::string& name) {
    return LoadTexture(name, name + ".png");
}

void TextureManager::LoadCommonTextures() {
    // Load basic block textures
    LoadTexture("stone");
    LoadTexture("dirt");
    LoadTexture("grass_top");
    LoadTexture("grass_side");
    LoadTexture("cobblestone");
    LoadTexture("planks_oak");
    LoadTexture("log_oak");
    LoadTexture("log_oak_top");
    LoadTexture("leaves_oak");
    LoadTexture("sand");
    LoadTexture("gravel");
    LoadTexture("bedrock");
    
    // Load ore textures
    LoadTexture("coal_ore");
    LoadTexture("iron_ore");
    LoadTexture("gold_ore");
    LoadTexture("diamond_ore");
    LoadTexture("redstone_ore");
    LoadTexture("lapis_ore");
    
    // Load other useful textures
    LoadTexture("brick");
    LoadTexture("obsidian");
    LoadTexture("netherrack");
    LoadTexture("glowstone");
    
    // Load GUI textures
    LoadTexture("widgets", "../gui/widgets.png");
}

void TextureManager::LoadTexturesFromBlockData() {
    // Load all unique textures referenced in block data
    std::set<std::string> uniqueTextures;
    
    for (const auto& pair : blockData) {
        const BlockData& block = pair.second;
        
        if (!block.allTexture.empty()) {
            uniqueTextures.insert(block.allTexture);
        }
        if (!block.topTexture.empty()) {
            uniqueTextures.insert(block.topTexture);
        }
        if (!block.bottomTexture.empty()) {
            uniqueTextures.insert(block.bottomTexture);
        }
        if (!block.sideTexture.empty()) {
            uniqueTextures.insert(block.sideTexture);
        }
    }
    
    // Load each unique texture
    for (const std::string& textureName : uniqueTextures) {
        LoadTexture(textureName);
    }
    
    // Also load GUI textures
    LoadTexture("widgets", "../gui/widgets.png");
    
    std::cout << "Loaded " << uniqueTextures.size() << " unique textures from block data" << std::endl;
}

Texture2D TextureManager::GetTexture(const std::string& name) const {
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second;
    }
    
    // Return a default white texture if not found
    static Texture2D whiteTexture = { 0 };
    if (whiteTexture.id == 0) {
        Image whiteImage = GenImageColor(16, 16, WHITE);
        whiteTexture = LoadTextureFromImage(whiteImage);
        UnloadImage(whiteImage);
    }
    return whiteTexture;
}

Material TextureManager::CreateMaterial(const std::string& textureName) {
    Material material = LoadMaterialDefault();
    Texture2D texture = GetTexture(textureName);
    
    if (texture.id != 0) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    }
    
    return material;
}

bool TextureManager::HasTexture(const std::string& name) const {
    return textures.find(name) != textures.end();
}

void TextureManager::UnloadAll() {
    for (auto& pair : textures) {
        UnloadTexture(pair.second);
    }
    textures.clear();
}

const BlockData* TextureManager::GetBlockData(int voxelType) const {
    auto it = blockData.find(voxelType);
    if (it != blockData.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string TextureManager::GetTextureNameForVoxel(int voxelType, int face) const {
    const BlockData* block = GetBlockData(voxelType);
    if (!block) {
        return "stone"; // Fallback texture
    }
    
    // Use face-specific textures first, fall back to "all" texture if needed
    switch (face) {
        case FACE_TOP:
            return !block->topTexture.empty() ? block->topTexture : 
                   (!block->allTexture.empty() ? block->allTexture : "stone");
        case FACE_BOTTOM:
            return !block->bottomTexture.empty() ? block->bottomTexture : 
                   (!block->allTexture.empty() ? block->allTexture : "stone");
        case FACE_FRONT:
        case FACE_BACK:
        case FACE_LEFT:
        case FACE_RIGHT:
            return !block->sideTexture.empty() ? block->sideTexture : 
                   (!block->allTexture.empty() ? block->allTexture : "stone");
        default:
            // If no face specified or unknown face, prefer "all" texture, then "side", then fallback
            if (!block->allTexture.empty()) return block->allTexture;
            if (!block->sideTexture.empty()) return block->sideTexture;
            if (!block->topTexture.empty()) return block->topTexture;
            return "stone";
    }
}

void TextureManager::DrawHotbarSlot(int x, int y, bool selected) const {
    Texture2D widgets = GetTexture("widgets");
    if (widgets.id == 0) return;
    
    // Hotbar slot coordinates in widgets.png
    Rectangle sourceRect;
    if (selected) {
        // Selected slot texture (24x24 with border)
        sourceRect = {0, 22, 24, 24};
    } else {
        // Normal slot texture (20x20)
        sourceRect = {0, 0, 20, 22};
    }
    
    Rectangle destRect = {(float)x, (float)y, sourceRect.width * 2, sourceRect.height * 2}; // Scale 2x
    DrawTexturePro(widgets, sourceRect, destRect, {0, 0}, 0.0f, WHITE);
}

void TextureManager::DrawHotbar(int centerX, int y, int selectedSlot) const {
    const int HOTBAR_SLOTS = 9;
    const int SLOT_SIZE = 40; // 20 pixels * 2 scale
    const int SELECTED_SLOT_SIZE = 48; // 24 pixels * 2 scale
    const int HOTBAR_WIDTH = HOTBAR_SLOTS * SLOT_SIZE;
    
    int startX = centerX - HOTBAR_WIDTH / 2;
    
    // Draw hotbar background first (full hotbar texture)
    Texture2D widgets = GetTexture("widgets");
    if (widgets.id != 0) {
        Rectangle sourceRect = {0, 0, 182, 22}; // Full hotbar background
        Rectangle destRect = {(float)(startX - 4), (float)y, 182 * 2, 22 * 2}; // Scale 2x
        DrawTexturePro(widgets, sourceRect, destRect, {0, 0}, 0.0f, WHITE);
    }
    
    // Draw individual slots and selection highlight
    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        int slotX = startX + i * SLOT_SIZE;
        bool isSelected = (i == selectedSlot);
        
        if (isSelected) {
            // Draw selection highlight - centered horizontally and vertically
            // Adjust position: 2 pixels left, centered vertically
            int highlightX = slotX - 2; // Move 2 pixels left
            int highlightY = y - 2; // Center vertically (22px hotbar + 24px selection = 2px offset)
            DrawHotbarSlot(highlightX, highlightY, true);
        }
        
        // Here you would draw the item icon in the slot
        // For now, we'll just show the slot numbers
        DrawText(TextFormat("%d", i + 1), slotX + 15, y + 25, 12, WHITE);
    }
}
