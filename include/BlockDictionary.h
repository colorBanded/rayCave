#pragma once

#include "raylib.h"
#include "simdjson.h"
#include <unordered_map>
#include <string>
#include <vector>

// Enhanced block types - we'll load these from JSON
enum class BlockType {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    STONE = 3,
    WOOD = 4,
    COBBLESTONE = 5,
    SAND = 6,
    WATER = 7,
    LAVA = 8,
    IRON_ORE = 9,
    COAL_ORE = 10,
    DIAMOND_ORE = 11,
    GOLD_ORE = 12,
    BEDROCK = 13,
    OBSIDIAN = 14,
    GLASS = 15,
    LEAVES = 16,
    PLANKS = 17,
    BRICK = 18,
    SNOW = 19,
    ICE = 20,
    CACTUS = 21,
    CLAY = 22,
    GRAVEL = 23,
    NETHERRACK = 24,
    SOUL_SAND = 25,
    GLOWSTONE = 26,
    // Add more as needed
    MAX_BLOCK_TYPES = 256  // Reserve space for future blocks
};

enum class BlockFace {
    TOP,
    BOTTOM,
    NORTH,
    SOUTH,
    EAST,
    WEST,
    ALL  // For blocks with same texture on all faces
};

// Block properties loaded from JSON
struct BlockProperties {
    std::string name;
    std::string displayName;
    bool isTransparent;
    bool isLiquid;
    bool isFlammable;
    bool isBreakable;
    float hardness;
    int lightLevel;
    bool emitsLight;
    std::string soundGroup;
    std::unordered_map<BlockFace, std::string> textures;  // Face -> texture filename
    Color tintColor;
    
    // Crafting and drop information
    std::string toolRequired;
    std::vector<std::pair<BlockType, int>> drops;  // What blocks drop when broken
    
    BlockProperties() : 
        isTransparent(false), isLiquid(false), isFlammable(false), 
        isBreakable(true), hardness(1.0f), lightLevel(0), emitsLight(false),
        soundGroup("stone"), tintColor(WHITE) {}
        
    BlockProperties(const std::string& n, const std::string& dn, bool trans, bool liquid, 
                   bool flamm, bool breakable, float hard, int light, bool emits, 
                   const std::string& sound, const std::unordered_map<BlockFace, std::string>& tex,
                   Color color, const std::string& tool, const std::vector<std::pair<BlockType, int>>& dr) :
        name(n), displayName(dn), isTransparent(trans), isLiquid(liquid), isFlammable(flamm),
        isBreakable(breakable), hardness(hard), lightLevel(light), emitsLight(emits),
        soundGroup(sound), textures(tex), tintColor(color), toolRequired(tool), drops(dr) {}
};

// Biome information for world generation
struct BiomeProperties {
    std::string name;
    std::string displayName;
    float temperature;
    float humidity;
    Color grassColor;
    Color foliageColor;
    Color waterColor;
    BlockType surfaceBlock;
    BlockType subsurfaceBlock;
    BlockType stoneBlock;
    std::vector<std::string> structures;  // Available structures in this biome
    std::vector<std::pair<BlockType, float>> ores;  // Ore type and spawn rate
    
    BiomeProperties() : 
        temperature(0.7f), humidity(0.4f), grassColor(GREEN), 
        foliageColor(DARKGREEN), waterColor(BLUE),
        surfaceBlock(BlockType::GRASS), subsurfaceBlock(BlockType::DIRT),
        stoneBlock(BlockType::STONE) {}
};

// Recipe system for crafting
struct CraftingRecipe {
    std::string name;
    std::vector<std::vector<std::string>> pattern;  // 3x3 crafting grid pattern
    std::unordered_map<char, BlockType> ingredients;  // Character -> block type mapping
    BlockType result;
    int resultCount;
    
    CraftingRecipe() : result(BlockType::AIR), resultCount(1) {}
};

class BlockDictionary {
private:
    static BlockDictionary* instance;
    
    std::unordered_map<BlockType, BlockProperties> blockProperties;
    std::unordered_map<std::string, BlockType> nameToBlockType;
    std::unordered_map<std::string, BiomeProperties> biomes;
    std::vector<CraftingRecipe> recipes;
    
    simdjson::dom::parser parser;
    bool isLoaded;
    
    // Private constructor for singleton
    BlockDictionary();
    
public:
    ~BlockDictionary() = default;
    
    // Singleton access
    static BlockDictionary& GetInstance();
    static void Cleanup();
    
    // Load data from JSON files
    bool LoadFromFiles(const std::string& dataDirectory = "data/");
    bool LoadBlocks(const std::string& filename);
    bool LoadBiomes(const std::string& filename);
    bool LoadRecipes(const std::string& filename);
    
    // Block property access
    const BlockProperties& GetBlockProperties(BlockType blockType) const;
    BlockType GetBlockTypeByName(const std::string& name) const;
    std::string GetBlockName(BlockType blockType) const;
    std::string GetBlockDisplayName(BlockType blockType) const;
    
    // Block behavior queries
    bool IsTransparent(BlockType blockType) const;
    bool IsLiquid(BlockType blockType) const;
    bool IsBreakable(BlockType blockType) const;
    bool EmitsLight(BlockType blockType) const;
    int GetLightLevel(BlockType blockType) const;
    float GetHardness(BlockType blockType) const;
    Color GetTintColor(BlockType blockType) const;
    
    // Texture information
    std::string GetTextureName(BlockType blockType, BlockFace face = BlockFace::ALL) const;
    bool HasCustomTexture(BlockType blockType, BlockFace face) const;
    
    // Biome access
    const BiomeProperties& GetBiomeProperties(const std::string& biomeName) const;
    std::vector<std::string> GetAllBiomeNames() const;
    
    // Crafting system
    const std::vector<CraftingRecipe>& GetAllRecipes() const;
    CraftingRecipe* FindRecipe(const std::vector<std::vector<BlockType>>& pattern) const;
    
    // Utility functions
    bool IsValidBlockType(BlockType blockType) const;
    std::vector<BlockType> GetAllBlockTypes() const;
    void ReloadData();  // Hot-reload for development
    
    // Debug and development
    void PrintBlockInfo(BlockType blockType) const;
    void ExportToJSON(const std::string& filename) const;
    
private:
    // JSON parsing helpers
    bool ParseBlockFromJSON(const simdjson::dom::element& blockElement, BlockType blockType);
    bool ParseBiomeFromJSON(const simdjson::dom::element& biomeElement, const std::string& biomeName);
    bool ParseRecipeFromJSON(const simdjson::dom::element& recipeElement);
    
    // Helper functions
    BlockFace StringToBlockFace(const std::string& faceStr) const;
    Color ParseColor(const simdjson::dom::element& colorElement) const;
    Color ParseColorFromArray(const simdjson::dom::array& colorArray) const;
    BlockType ParseBlockType(const std::string& blockName) const;
    
    // Default properties
    static const BlockProperties defaultBlockProperties;
    static const BiomeProperties defaultBiomeProperties;
};

// Global convenience functions
inline BlockDictionary& GetBlockDictionary() {
    return BlockDictionary::GetInstance();
}
