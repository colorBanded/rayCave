#pragma once

#include "raylib.h"
#include "TextureManager.h"
#include <array>
#include <vector>
#include <memory>
#include <cmath>

// Forward declarations
struct QuadMesh {
    Vector3 position;
    Vector3 size;
    BlockType blockType;
    BlockFace face;
    
    QuadMesh(Vector3 pos, Vector3 sz, BlockType type, BlockFace f) 
        : position(pos), size(sz), blockType(type), face(f) {}
};

// Chunk constants
constexpr int CHUNK_SIZE = 16;        // 16x16 blocks in X and Z
constexpr int CHUNK_HEIGHT = 256;     // 256 blocks in Y (like Minecraft)
constexpr int BLOCKS_PER_CHUNK = CHUNK_SIZE * CHUNK_SIZE * CHUNK_HEIGHT;

// Chunk coordinates (world space divided by CHUNK_SIZE)
struct ChunkCoord {
    int x, z;
    
    ChunkCoord(int x = 0, int z = 0) : x(x), z(z) {}
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && z == other.z;
    }
    
    // Convert world position to chunk coordinates
    static ChunkCoord FromWorldPos(float worldX, float worldZ) {
        return ChunkCoord(
            static_cast<int>(std::floor(worldX / CHUNK_SIZE)),
            static_cast<int>(std::floor(worldZ / CHUNK_SIZE))
        );
    }
    
    // Get world position of chunk's origin (bottom-left corner)
    Vector3 GetWorldOrigin() const {
        return {static_cast<float>(x * CHUNK_SIZE), 0.0f, static_cast<float>(z * CHUNK_SIZE)};
    }
};

// Hash function for ChunkCoord to use in unordered_map
struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& coord) const {
        return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.z) << 1);
    }
};

// Block position within a chunk (0-15 for x,z and 0-255 for y)
struct BlockPos {
    int x, y, z;
    
    BlockPos(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}
    
    // Convert to flat array index
    int ToIndex() const {
        return x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE;
    }
    
    // Create from flat array index
    static BlockPos FromIndex(int index) {
        int y = index / (CHUNK_SIZE * CHUNK_SIZE);
        int remainder = index % (CHUNK_SIZE * CHUNK_SIZE);
        int z = remainder / CHUNK_SIZE;
        int x = remainder % CHUNK_SIZE;
        return BlockPos(x, y, z);
    }
    
    // Check if position is valid within chunk bounds
    bool IsValid() const {
        return x >= 0 && x < CHUNK_SIZE && 
               y >= 0 && y < CHUNK_HEIGHT && 
               z >= 0 && z < CHUNK_SIZE;
    }
};

class Chunk {
private:
    ChunkCoord coord;
    std::array<BlockType, BLOCKS_PER_CHUNK> blocks;
    bool isGenerated;
    bool isDirty;        // Needs to be saved
    bool isLoaded;       // Is in memory
    
    // Rendering optimization
    mutable bool meshDirty;
    mutable std::vector<Vector3> vertices;
    mutable std::vector<Vector2> uvs;
    mutable std::vector<unsigned short> indices;
    mutable int visibleFaces;
    
    // Greedy meshing optimization
    mutable std::vector<QuadMesh> optimizedQuads;
    mutable bool greedyMeshDirty;

public:
    Chunk(ChunkCoord coord);
    ~Chunk() = default;
    
    // Block access
    BlockType GetBlock(const BlockPos& pos) const;
    BlockType GetBlock(int x, int y, int z) const;
    void SetBlock(const BlockPos& pos, BlockType blockType);
    void SetBlock(int x, int y, int z, BlockType blockType);
    
    // Chunk properties
    ChunkCoord GetCoord() const { return coord; }
    bool IsGenerated() const { return isGenerated; }
    bool IsDirty() const { return isDirty; }
    bool IsLoaded() const { return isLoaded; }
    
    void SetGenerated(bool generated) { isGenerated = generated; }
    void SetDirty(bool dirty) { isDirty = dirty; }
    void SetLoaded(bool loaded) { isLoaded = loaded; }
    
    // Fill entire chunk with a block type (useful for generation)
    void Fill(BlockType blockType);
    
    // Clear chunk (fill with air)
    void Clear() { Fill(BlockType::AIR); }
    
    // Rendering
    void Render(TextureManager& textureManager) const;
    void RenderOptimized(TextureManager& textureManager, const Chunk* neighbors[4] = nullptr) const;
    void UpdateMesh() const; // Rebuild mesh if dirty
    
    // Serialization
    std::vector<unsigned char> Serialize() const;
    bool Deserialize(const std::vector<unsigned char>& data);
    
    // Get the highest non-air block at given x,z coordinates
    int GetHeightAt(int x, int z) const;
    
    // Check if a face should be rendered (for mesh optimization)
    bool ShouldRenderFace(int x, int y, int z, BlockFace face, const Chunk* neighbors[4] = nullptr) const;
    
    // Debug methods
    int GetVisibleFaceCount() const { return visibleFaces; }
    int GetVertexCount() const { return vertices.size(); }
    bool IsMeshDirty() const { return meshDirty; }
    
private:
    // Helper methods
    void MarkMeshDirty() const { meshDirty = true; greedyMeshDirty = true; }
    void GenerateBlockMesh(int x, int y, int z, std::vector<Vector3>& verts, 
                          std::vector<Vector2>& uvCoords, std::vector<unsigned short>& inds, 
                          const Vector3& worldOrigin) const;
    
    // Simple rendering method
    void RenderSingleBlock(const Vector3& blockPos, BlockType blockType, TextureManager& textureManager) const;
    
    // Greedy meshing methods (currently unused)
    void GenerateGreedyMesh(const Chunk* neighbors[4] = nullptr) const;
    void GenerateQuadsForFace(BlockFace face, const Chunk* neighbors[4]) const;
    bool CanMergeQuads(int x1, int y1, int z1, int x2, int y2, int z2, BlockFace face, const Chunk* neighbors[4]) const;
    void RenderQuad(const QuadMesh& quad, TextureManager& textureManager) const;
};
