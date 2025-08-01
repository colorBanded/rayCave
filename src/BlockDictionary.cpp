#include "BlockDictionary.h"
#include <iostream>
#include <fstream>
#include <iterator>

// Static member definitions
BlockDictionary* BlockDictionary::instance = nullptr;

const BlockProperties BlockDictionary::defaultBlockProperties = BlockProperties();
const BiomeProperties BlockDictionary::defaultBiomeProperties = BiomeProperties();

BlockDictionary::BlockDictionary() : isLoaded(false) {
    // Constructor now just initializes the object
    // Block properties will be loaded from JSON files
}

BlockDictionary& BlockDictionary::GetInstance() {
    if (!instance) {
        instance = new BlockDictionary();
    }
    return *instance;
}

void BlockDictionary::Cleanup() {
    delete instance;
    instance = nullptr;
}

bool BlockDictionary::LoadFromFiles(const std::string& dataDirectory) {
    std::cout << "Loading block dictionary from: " << dataDirectory << std::endl;
    
    bool success = true;
    
    // Load blocks from JSON
    if (!LoadBlocks(dataDirectory + "blocks.json")) {
        std::cout << "Warning: Failed to load blocks.json, using defaults if available" << std::endl;
        success = false;
    }
    
    // Load biomes from JSON (if file exists)
    LoadBiomes(dataDirectory + "biomes.json");
    
    // Load recipes from JSON (if file exists)
    LoadRecipes(dataDirectory + "recipes.json");
    
    isLoaded = success;
    std::cout << "Block dictionary loaded successfully: " << success << std::endl;
    return success;
}

bool BlockDictionary::LoadBlocks(const std::string& filename) {
    std::cout << "Loading blocks from: " << filename << std::endl;
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: Could not open " << filename << std::endl;
        return false;
    }
    
    // Read the entire file content
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    try {
        // Parse JSON
        simdjson::dom::element doc = parser.parse(content);
        
        // Get the blocks array
        simdjson::dom::array blocksArray;
        if (doc["blocks"].get_array().get(blocksArray) != simdjson::SUCCESS) {
            std::cout << "Error: Could not find 'blocks' array in JSON" << std::endl;
            return false;
        }
        
        // Clear existing block data
        blockProperties.clear();
        nameToBlockType.clear();
        
        // Parse each block
        for (simdjson::dom::element blockElement : blocksArray) {
            int64_t blockId;
            if (blockElement["id"].get_int64().get(blockId) != simdjson::SUCCESS) {
                std::cout << "Warning: Block missing ID, skipping" << std::endl;
                continue;
            }
            
            BlockType blockType = static_cast<BlockType>(blockId);
            if (!ParseBlockFromJSON(blockElement, blockType)) {
                std::cout << "Warning: Failed to parse block with ID " << blockId << std::endl;
                continue;
            }
        }
        
        std::cout << "Successfully loaded " << blockProperties.size() << " blocks" << std::endl;
        return true;
        
    } catch (const simdjson::simdjson_error& e) {
        std::cout << "JSON parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool BlockDictionary::ParseBlockFromJSON(const simdjson::dom::element& blockElement, BlockType blockType) {
    BlockProperties props;
    
    // Parse basic properties
    std::string_view nameView;
    if (blockElement["name"].get_string().get(nameView) == simdjson::SUCCESS) {
        props.name = std::string(nameView);
    }
    
    std::string_view displayNameView;
    if (blockElement["displayName"].get_string().get(displayNameView) == simdjson::SUCCESS) {
        props.displayName = std::string(displayNameView);
    }
    
    bool transparent;
    if (blockElement["transparent"].get_bool().get(transparent) == simdjson::SUCCESS) {
        props.isTransparent = transparent;
    }
    
    bool liquid;
    if (blockElement["liquid"].get_bool().get(liquid) == simdjson::SUCCESS) {
        props.isLiquid = liquid;
    }
    
    bool flammable;
    if (blockElement["flammable"].get_bool().get(flammable) == simdjson::SUCCESS) {
        props.isFlammable = flammable;
    }
    
    bool breakable;
    if (blockElement["breakable"].get_bool().get(breakable) == simdjson::SUCCESS) {
        props.isBreakable = breakable;
    }
    
    bool emitsLight;
    if (blockElement["emitsLight"].get_bool().get(emitsLight) == simdjson::SUCCESS) {
        props.emitsLight = emitsLight;
    }
    
    double hardness;
    if (blockElement["hardness"].get_double().get(hardness) == simdjson::SUCCESS) {
        props.hardness = static_cast<float>(hardness);
    }
    
    int64_t lightLevel;
    if (blockElement["lightLevel"].get_int64().get(lightLevel) == simdjson::SUCCESS) {
        props.lightLevel = static_cast<int>(lightLevel);
    }
    
    std::string_view soundGroupView;
    if (blockElement["soundGroup"].get_string().get(soundGroupView) == simdjson::SUCCESS) {
        props.soundGroup = std::string(soundGroupView);
    }
    
    std::string_view toolRequiredView;
    if (blockElement["toolRequired"].get_string().get(toolRequiredView) == simdjson::SUCCESS) {
        props.toolRequired = std::string(toolRequiredView);
    }
    
    // Parse textures
    simdjson::dom::object texturesObj;
    if (blockElement["textures"].get_object().get(texturesObj) == simdjson::SUCCESS) {
        for (auto [key, value] : texturesObj) {
            std::string faceStr(key);
            std::string_view textureView;
            if (value.get_string().get(textureView) == simdjson::SUCCESS) {
                BlockFace face = StringToBlockFace(faceStr);
                props.textures[face] = std::string(textureView);
            }
        }
    }
    
    // Parse tint color
    simdjson::dom::array tintColorArray;
    if (blockElement["tintColor"].get_array().get(tintColorArray) == simdjson::SUCCESS) {
        props.tintColor = ParseColorFromArray(tintColorArray);
    }
    
    // Store the block properties
    blockProperties[blockType] = props;
    nameToBlockType[props.name] = blockType;
    
    return true;
}

Color BlockDictionary::ParseColor(const simdjson::dom::element& colorElement) const {
    Color color = WHITE;
    int64_t r, g, b, a = 255;
    
    if (colorElement["r"].get_int64().get(r) == simdjson::SUCCESS) color.r = static_cast<unsigned char>(r);
    if (colorElement["g"].get_int64().get(g) == simdjson::SUCCESS) color.g = static_cast<unsigned char>(g);
    if (colorElement["b"].get_int64().get(b) == simdjson::SUCCESS) color.b = static_cast<unsigned char>(b);
    if (colorElement["a"].get_int64().get(a) == simdjson::SUCCESS) color.a = static_cast<unsigned char>(a);
    
    return color;
}

Color BlockDictionary::ParseColorFromArray(const simdjson::dom::array& colorArray) const {
    Color color = WHITE;
    
    // Expect array format: [r, g, b, a]
    size_t index = 0;
    for (auto element : colorArray) {
        int64_t value;
        if (element.get_int64().get(value) == simdjson::SUCCESS) {
            switch (index) {
                case 0: color.r = static_cast<unsigned char>(value); break;
                case 1: color.g = static_cast<unsigned char>(value); break;
                case 2: color.b = static_cast<unsigned char>(value); break;
                case 3: color.a = static_cast<unsigned char>(value); break;
            }
        }
        index++;
        if (index >= 4) break;
    }
    
    return color;
}

BlockFace BlockDictionary::StringToBlockFace(const std::string& faceStr) const {
    if (faceStr == "top") return BlockFace::TOP;
    if (faceStr == "bottom") return BlockFace::BOTTOM;
    if (faceStr == "north") return BlockFace::NORTH;
    if (faceStr == "south") return BlockFace::SOUTH;
    if (faceStr == "east") return BlockFace::EAST;
    if (faceStr == "west") return BlockFace::WEST;
    if (faceStr == "side") return BlockFace::ALL;  // Handle "side" as ALL for compatibility
    return BlockFace::ALL;
}

BlockType BlockDictionary::ParseBlockType(const std::string& blockName) const {
    auto it = nameToBlockType.find(blockName);
    if (it != nameToBlockType.end()) {
        return it->second;
    }
    return BlockType::AIR;
}

const BlockProperties& BlockDictionary::GetBlockProperties(BlockType blockType) const {
    auto it = blockProperties.find(blockType);
    if (it != blockProperties.end()) {
        return it->second;
    }
    return defaultBlockProperties;
}

BlockType BlockDictionary::GetBlockTypeByName(const std::string& name) const {
    auto it = nameToBlockType.find(name);
    if (it != nameToBlockType.end()) {
        return it->second;
    }
    return BlockType::AIR;
}

std::string BlockDictionary::GetBlockName(BlockType blockType) const {
    const auto& props = GetBlockProperties(blockType);
    return props.name;
}

std::string BlockDictionary::GetBlockDisplayName(BlockType blockType) const {
    const auto& props = GetBlockProperties(blockType);
    return props.displayName.empty() ? props.name : props.displayName;
}

bool BlockDictionary::IsTransparent(BlockType blockType) const {
    return GetBlockProperties(blockType).isTransparent;
}

bool BlockDictionary::IsLiquid(BlockType blockType) const {
    return GetBlockProperties(blockType).isLiquid;
}

bool BlockDictionary::IsBreakable(BlockType blockType) const {
    return GetBlockProperties(blockType).isBreakable;
}

bool BlockDictionary::EmitsLight(BlockType blockType) const {
    return GetBlockProperties(blockType).emitsLight;
}

int BlockDictionary::GetLightLevel(BlockType blockType) const {
    return GetBlockProperties(blockType).lightLevel;
}

float BlockDictionary::GetHardness(BlockType blockType) const {
    return GetBlockProperties(blockType).hardness;
}

Color BlockDictionary::GetTintColor(BlockType blockType) const {
    return GetBlockProperties(blockType).tintColor;
}

std::string BlockDictionary::GetTextureName(BlockType blockType, BlockFace face) const {
    const auto& props = GetBlockProperties(blockType);
    
    // First try the specific face
    auto it = props.textures.find(face);
    if (it != props.textures.end()) {
        return it->second;
    }
    
    // Then try the ALL face (default texture)
    it = props.textures.find(BlockFace::ALL);
    if (it != props.textures.end()) {
        return it->second;
    }
    
    // Fallback to block name
    return props.name;
}

bool BlockDictionary::HasCustomTexture(BlockType blockType, BlockFace face) const {
    const auto& props = GetBlockProperties(blockType);
    return props.textures.find(face) != props.textures.end();
}

bool BlockDictionary::IsValidBlockType(BlockType blockType) const {
    return blockProperties.find(blockType) != blockProperties.end();
}

std::vector<BlockType> BlockDictionary::GetAllBlockTypes() const {
    std::vector<BlockType> types;
    for (const auto& [blockType, _] : blockProperties) {
        types.push_back(blockType);
    }
    return types;
}

void BlockDictionary::ReloadData() {
    blockProperties.clear();
    nameToBlockType.clear();
    biomes.clear();
    recipes.clear();
    
    LoadFromFiles("data/");
}

void BlockDictionary::PrintBlockInfo(BlockType blockType) const {
    const auto& props = GetBlockProperties(blockType);
    std::cout << "=== Block Info: " << props.displayName << " ===" << std::endl;
    std::cout << "Name: " << props.name << std::endl;
    std::cout << "ID: " << static_cast<int>(blockType) << std::endl;
    std::cout << "Transparent: " << (props.isTransparent ? "Yes" : "No") << std::endl;
    std::cout << "Liquid: " << (props.isLiquid ? "Yes" : "No") << std::endl;
    std::cout << "Breakable: " << (props.isBreakable ? "Yes" : "No") << std::endl;
    std::cout << "Hardness: " << props.hardness << std::endl;
    std::cout << "Light Level: " << props.lightLevel << std::endl;
    std::cout << "Sound Group: " << props.soundGroup << std::endl;
    std::cout << "Tool Required: " << props.toolRequired << std::endl;
    std::cout << "Textures:" << std::endl;
    for (const auto& [face, texture] : props.textures) {
        std::cout << "  " << static_cast<int>(face) << ": " << texture << std::endl;
    }
}

void BlockDictionary::ExportToJSON(const std::string& filename) const {
    // Stub implementation - not used in simplified version
    std::cout << "ExportToJSON not implemented in simplified version" << std::endl;
}

// Stub implementations for biomes and recipes (can be expanded later)
bool BlockDictionary::LoadBiomes(const std::string& filename) {
    // TODO: Implement biome loading
    std::cout << "Biome loading not yet implemented" << std::endl;
    return true;
}

bool BlockDictionary::LoadRecipes(const std::string& filename) {
    // TODO: Implement recipe loading  
    std::cout << "Recipe loading not yet implemented" << std::endl;
    return true;
}

bool BlockDictionary::ParseBiomeFromJSON(const simdjson::dom::element& biomeElement, const std::string& biomeName) {
    // TODO: Implement biome parsing
    return true;
}

bool BlockDictionary::ParseRecipeFromJSON(const simdjson::dom::element& recipeElement) {
    // TODO: Implement recipe parsing
    return true;
}

const BiomeProperties& BlockDictionary::GetBiomeProperties(const std::string& biomeName) const {
    auto it = biomes.find(biomeName);
    if (it != biomes.end()) {
        return it->second;
    }
    return defaultBiomeProperties;
}

std::vector<std::string> BlockDictionary::GetAllBiomeNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : biomes) {
        names.push_back(name);
    }
    return names;
}

const std::vector<CraftingRecipe>& BlockDictionary::GetAllRecipes() const {
    return recipes;
}

CraftingRecipe* BlockDictionary::FindRecipe(const std::vector<std::vector<BlockType>>& pattern) const {
    // TODO: Implement recipe matching
    return nullptr;
}
