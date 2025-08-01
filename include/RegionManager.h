#pragma once

#include "Chunk.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>

// Region coordinates (each region contains REGION_SIZE x REGION_SIZE chunks)
constexpr int REGION_SIZE = 32; // 32x32 chunks per region file

struct RegionCoord {
    int x, z;
    
    RegionCoord(int x = 0, int z = 0) : x(x), z(z) {}
    
    // Get region coordinate from chunk coordinate
    static RegionCoord FromChunkCoord(ChunkCoord chunkCoord) {
        return RegionCoord(
            chunkCoord.x < 0 ? (chunkCoord.x - REGION_SIZE + 1) / REGION_SIZE : chunkCoord.x / REGION_SIZE,
            chunkCoord.z < 0 ? (chunkCoord.z - REGION_SIZE + 1) / REGION_SIZE : chunkCoord.z / REGION_SIZE
        );
    }
    
    // Get local chunk coordinates within this region (0-31, 0-31)
    static void GetLocalChunkCoord(ChunkCoord chunkCoord, int& localX, int& localZ) {
        localX = chunkCoord.x - (FromChunkCoord(chunkCoord).x * REGION_SIZE);
        localZ = chunkCoord.z - (FromChunkCoord(chunkCoord).z * REGION_SIZE);
        
        // Handle negative coordinates properly
        if (localX < 0) localX += REGION_SIZE;
        if (localZ < 0) localZ += REGION_SIZE;
    }
    
    bool operator==(const RegionCoord& other) const {
        return x == other.x && z == other.z;
    }
    
    std::string ToString() const {
        return "r." + std::to_string(x) + "." + std::to_string(z) + ".mcr";
    }
};

// Hash function for RegionCoord
namespace std {
    template<>
    struct hash<RegionCoord> {
        size_t operator()(const RegionCoord& coord) const {
            return std::hash<long long>()(((long long)coord.x << 32) | coord.z);
        }
    };
}

// Region file header structure
struct RegionHeader {
    static constexpr uint32_t MAGIC = 0x52454749; // "REGI" in little endian
    static constexpr uint32_t VERSION = 1;
    
    uint32_t magic;
    uint32_t version;
    uint32_t chunkOffsets[REGION_SIZE * REGION_SIZE]; // Byte offsets for each chunk (0 = not present)
    uint32_t chunkSizes[REGION_SIZE * REGION_SIZE];   // Sizes in bytes for each chunk
    uint32_t lastModified[REGION_SIZE * REGION_SIZE]; // Last modified timestamps
    
    RegionHeader() : magic(MAGIC), version(VERSION) {
        std::fill(chunkOffsets, chunkOffsets + REGION_SIZE * REGION_SIZE, 0);
        std::fill(chunkSizes, chunkSizes + REGION_SIZE * REGION_SIZE, 0);
        std::fill(lastModified, lastModified + REGION_SIZE * REGION_SIZE, 0);
    }
    
    int GetChunkIndex(int localX, int localZ) const {
        return localZ * REGION_SIZE + localX;
    }
};

class RegionManager {
private:
    std::string worldPath;
    std::mutex regionMutex;
    
    // Cache of loaded region headers to avoid repeated file reads
    std::unordered_map<RegionCoord, std::unique_ptr<RegionHeader>> headerCache;
    
    // File I/O
    std::string GetRegionFilePath(RegionCoord regionCoord) const;
    bool RegionFileExists(RegionCoord regionCoord) const;
    
    // Region header management
    RegionHeader* GetRegionHeader(RegionCoord regionCoord);
    bool SaveRegionHeader(RegionCoord regionCoord, const RegionHeader& header);
    bool LoadRegionHeader(RegionCoord regionCoord, RegionHeader& header);
    
    // Utility functions
    void CreateDirectoryIfNeeded(const std::string& filePath);
    
public:
    RegionManager(const std::string& worldPath);
    ~RegionManager() = default;
    
    // Chunk I/O operations
    bool SaveChunk(const Chunk& chunk);
    bool LoadChunk(Chunk& chunk);
    bool ChunkExists(ChunkCoord chunkCoord);
    
    // Region management
    bool DeleteChunk(ChunkCoord chunkCoord);
    void CompactRegion(RegionCoord regionCoord); // Remove fragmentation
    
    // Statistics
    size_t GetRegionFileSize(RegionCoord regionCoord);
    int GetChunkCountInRegion(RegionCoord regionCoord);
    
    // Cleanup
    void ClearCache();
};
