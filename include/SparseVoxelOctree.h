#pragma once

#include "raylib.h"
#include "BlockDictionary.h"
#include <array>
#include <memory>
#include <vector>
#include <cstdint>

// Forward declarations
class MarchingCubes;

// Octree node structure for sparse voxel representation
struct OctreeNode {
    // Child nodes (nullptr if empty, shared_ptr for memory management)
    std::array<std::shared_ptr<OctreeNode>, 8> children;
    
    // Voxel data - only stored at leaf nodes
    BlockType blockType;
    
    // Node properties
    bool isLeaf;
    bool isEmpty; // Optimization: true if all children are air
    uint8_t level; // 0 = leaf, higher = internal node
    
    // Spatial properties
    Vector3 center;
    float size; // Half-size of the node
    
    OctreeNode(Vector3 center, float size, uint8_t level = 0) 
        : blockType(BlockType::AIR), isLeaf(true), isEmpty(true), 
          level(level), center(center), size(size) {
        children.fill(nullptr);
    }
    
    // Get child index based on position relative to center
    int GetChildIndex(const Vector3& pos) const;
    
    // Check if a position is within this node
    bool Contains(const Vector3& pos) const;
    
    // Split this node into 8 children
    void Split();
    
    // Merge children back into this node if possible
    bool TryMerge();
    
    // Calculate memory usage of this subtree
    size_t GetMemoryUsage() const;
};

class SparseVoxelOctree {
private:
    std::shared_ptr<OctreeNode> root;
    Vector3 worldOrigin;
    float worldSize; // Size of the entire octree
    uint8_t maxDepth; // Maximum depth of the octree
    
    // Internal methods
    std::shared_ptr<OctreeNode> GetNode(const Vector3& worldPos, bool createIfMissing = false);
    void SetBlockRecursive(std::shared_ptr<OctreeNode>& node, const Vector3& worldPos, 
                          BlockType blockType, int depth);
    BlockType GetBlockRecursive(const std::shared_ptr<OctreeNode>& node, 
                               const Vector3& worldPos, int depth) const;
    
    // Optimization methods
    void OptimizeSubtree(std::shared_ptr<OctreeNode>& node);
    void PruneEmptyNodes(std::shared_ptr<OctreeNode>& node);

public:
    SparseVoxelOctree(Vector3 origin, float size, uint8_t maxDepth = 8);
    ~SparseVoxelOctree() = default;
    
    // Basic voxel operations
    void SetBlock(const Vector3& worldPos, BlockType blockType);
    BlockType GetBlock(const Vector3& worldPos) const;
    
    // Bulk operations for efficiency
    void SetBlocks(const std::vector<Vector3>& positions, const std::vector<BlockType>& types);
    void FillRegion(const Vector3& min, const Vector3& max, BlockType blockType);
    void ClearRegion(const Vector3& min, const Vector3& max);
    
    // Query operations
    bool IsEmpty(const Vector3& min, const Vector3& max) const;
    std::vector<Vector3> GetNonEmptyBlocks(const Vector3& min, const Vector3& max) const;
    
    // Memory and performance
    size_t GetMemoryUsage() const;
    void Optimize(); // Merge nodes where possible
    void Clear();
    
    // Integration with existing chunk system
    void FromChunk(const class Chunk& chunk, const Vector3& chunkWorldOrigin);
    void ToChunk(class Chunk& chunk, const Vector3& chunkWorldOrigin) const;
    
    // Marching cubes integration
    void GenerateMarchingCubesMesh(const Vector3& min, const Vector3& max, 
                                  std::vector<Vector3>& vertices, 
                                  std::vector<Vector3>& normals,
                                  std::vector<unsigned int>& indices) const;
    
    // Debug and visualization
    void DebugDraw() const;
    void DebugDrawNode(const std::shared_ptr<OctreeNode>& node, Color color) const;
    
    // Getters
    Vector3 GetWorldOrigin() const { return worldOrigin; }
    float GetWorldSize() const { return worldSize; }
    uint8_t GetMaxDepth() const { return maxDepth; }
    const std::shared_ptr<OctreeNode>& GetRoot() const { return root; }
};

// Utility functions
namespace OctreeUtils {
    // Convert world position to octree coordinates
    Vector3 WorldToOctree(const Vector3& worldPos, const Vector3& origin, float size);
    
    // Convert octree coordinates to world position
    Vector3 OctreeToWorld(const Vector3& octreePos, const Vector3& origin, float size);
    
    // Calculate the size of a node at a given level
    float GetNodeSize(float rootSize, uint8_t level);
    
    // Calculate the maximum number of nodes at a given depth
    uint64_t GetMaxNodesAtDepth(uint8_t depth);
}
