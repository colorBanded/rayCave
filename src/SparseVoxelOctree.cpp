#include "SparseVoxelOctree.h"
#include "Chunk.h"
#include <algorithm>
#include <cmath>

// OctreeNode implementation
int OctreeNode::GetChildIndex(const Vector3& pos) const {
    int index = 0;
    if (pos.x >= center.x) index |= 1;
    if (pos.y >= center.y) index |= 2;
    if (pos.z >= center.z) index |= 4;
    return index;
}

bool OctreeNode::Contains(const Vector3& pos) const {
    return pos.x >= center.x - size && pos.x < center.x + size &&
           pos.y >= center.y - size && pos.y < center.y + size &&
           pos.z >= center.z - size && pos.z < center.z + size;
}

void OctreeNode::Split() {
    if (!isLeaf) return; // Already split
    
    isLeaf = false;
    float childSize = size * 0.5f;
    float quarterSize = childSize * 0.5f;
    
    // Create 8 children
    for (int i = 0; i < 8; i++) {
        Vector3 childCenter = {
            center.x + ((i & 1) ? quarterSize : -quarterSize),
            center.y + ((i & 2) ? quarterSize : -quarterSize),
            center.z + ((i & 4) ? quarterSize : -quarterSize)
        };
        
        children[i] = std::make_shared<OctreeNode>(childCenter, childSize, level + 1);
        children[i]->blockType = blockType; // Initialize with parent's block type
    }
}

bool OctreeNode::TryMerge() {
    if (isLeaf) return false; // Already a leaf
    
    // Check if all children are leaves with the same block type
    BlockType commonType = children[0]->blockType;
    for (int i = 0; i < 8; i++) {
        if (!children[i] || !children[i]->isLeaf || children[i]->blockType != commonType) {
            return false; // Can't merge
        }
    }
    
    // Merge children
    isLeaf = true;
    blockType = commonType;
    isEmpty = (commonType == BlockType::AIR);
    
    // Clear children
    for (auto& child : children) {
        child.reset();
    }
    
    return true;
}

size_t OctreeNode::GetMemoryUsage() const {
    size_t usage = sizeof(OctreeNode);
    
    if (!isLeaf) {
        for (const auto& child : children) {
            if (child) {
                usage += child->GetMemoryUsage();
            }
        }
    }
    
    return usage;
}

// SparseVoxelOctree implementation
SparseVoxelOctree::SparseVoxelOctree(Vector3 origin, float size, uint8_t maxDepth)
    : worldOrigin(origin), worldSize(size), maxDepth(maxDepth) {
    Vector3 rootCenter = {
        origin.x + size * 0.5f,
        origin.y + size * 0.5f,
        origin.z + size * 0.5f
    };
    root = std::make_shared<OctreeNode>(rootCenter, size * 0.5f, 0);
}

std::shared_ptr<OctreeNode> SparseVoxelOctree::GetNode(const Vector3& worldPos, bool createIfMissing) {
    std::shared_ptr<OctreeNode> current = root;
    
    for (int depth = 0; depth < maxDepth && current && !current->isLeaf; depth++) {
        int childIndex = current->GetChildIndex(worldPos);
        
        if (!current->children[childIndex]) {
            if (!createIfMissing) return nullptr;
            
            // Create missing child
            float childSize = OctreeUtils::GetNodeSize(worldSize * 0.5f, current->level + 1);
            float quarterSize = childSize * 0.5f;
            
            Vector3 childCenter = {
                current->center.x + ((childIndex & 1) ? quarterSize : -quarterSize),
                current->center.y + ((childIndex & 2) ? quarterSize : -quarterSize),
                current->center.z + ((childIndex & 4) ? quarterSize : -quarterSize)
            };
            
            current->children[childIndex] = std::make_shared<OctreeNode>(
                childCenter, childSize, current->level + 1);
        }
        
        current = current->children[childIndex];
    }
    
    return current;
}

void SparseVoxelOctree::SetBlockRecursive(std::shared_ptr<OctreeNode>& node, const Vector3& worldPos, 
                                         BlockType blockType, int depth) {
    if (!node || depth >= maxDepth) return;
    
    if (node->isLeaf) {
        node->blockType = blockType;
        node->isEmpty = (blockType == BlockType::AIR);
        return;
    }
    
    // Need to go deeper
    int childIndex = node->GetChildIndex(worldPos);
    
    if (!node->children[childIndex]) {
        float childSize = OctreeUtils::GetNodeSize(worldSize * 0.5f, node->level + 1);
        float quarterSize = childSize * 0.5f;
        
        Vector3 childCenter = {
            node->center.x + ((childIndex & 1) ? quarterSize : -quarterSize),
            node->center.y + ((childIndex & 2) ? quarterSize : -quarterSize),
            node->center.z + ((childIndex & 4) ? quarterSize : -quarterSize)
        };
        
        node->children[childIndex] = std::make_shared<OctreeNode>(
            childCenter, childSize, node->level + 1);
        node->children[childIndex]->blockType = node->blockType; // Inherit parent's type
    }
    
    SetBlockRecursive(node->children[childIndex], worldPos, blockType, depth + 1);
    
    // Try to merge after modification
    node->TryMerge();
}

BlockType SparseVoxelOctree::GetBlockRecursive(const std::shared_ptr<OctreeNode>& node, 
                                              const Vector3& worldPos, int depth) const {
    if (!node) return BlockType::AIR;
    
    if (node->isLeaf || depth >= maxDepth) {
        return node->blockType;
    }
    
    int childIndex = node->GetChildIndex(worldPos);
    return GetBlockRecursive(node->children[childIndex], worldPos, depth + 1);
}

void SparseVoxelOctree::SetBlock(const Vector3& worldPos, BlockType blockType) {
    if (!root->Contains(worldPos)) return; // Out of bounds
    
    SetBlockRecursive(root, worldPos, blockType, 0);
}

BlockType SparseVoxelOctree::GetBlock(const Vector3& worldPos) const {
    if (!root->Contains(worldPos)) return BlockType::AIR; // Out of bounds
    
    return GetBlockRecursive(root, worldPos, 0);
}

void SparseVoxelOctree::SetBlocks(const std::vector<Vector3>& positions, const std::vector<BlockType>& types) {
    if (positions.size() != types.size()) return;
    
    for (size_t i = 0; i < positions.size(); i++) {
        SetBlock(positions[i], types[i]);
    }
}

void SparseVoxelOctree::FillRegion(const Vector3& min, const Vector3& max, BlockType blockType) {
    // Simple implementation - iterate through all positions
    // TODO: Optimize using octree structure
    for (float x = min.x; x < max.x; x += 1.0f) {
        for (float y = min.y; y < max.y; y += 1.0f) {
            for (float z = min.z; z < max.z; z += 1.0f) {
                SetBlock({x, y, z}, blockType);
            }
        }
    }
}

void SparseVoxelOctree::ClearRegion(const Vector3& min, const Vector3& max) {
    FillRegion(min, max, BlockType::AIR);
}

bool SparseVoxelOctree::IsEmpty(const Vector3& min, const Vector3& max) const {
    // Simple implementation - check all positions
    for (float x = min.x; x < max.x; x += 1.0f) {
        for (float y = min.y; y < max.y; y += 1.0f) {
            for (float z = min.z; z < max.z; z += 1.0f) {
                if (GetBlock({x, y, z}) != BlockType::AIR) {
                    return false;
                }
            }
        }
    }
    return true;
}

std::vector<Vector3> SparseVoxelOctree::GetNonEmptyBlocks(const Vector3& min, const Vector3& max) const {
    std::vector<Vector3> result;
    
    for (float x = min.x; x < max.x; x += 1.0f) {
        for (float y = min.y; y < max.y; y += 1.0f) {
            for (float z = min.z; z < max.z; z += 1.0f) {
                Vector3 pos = {x, y, z};
                if (GetBlock(pos) != BlockType::AIR) {
                    result.push_back(pos);
                }
            }
        }
    }
    
    return result;
}

size_t SparseVoxelOctree::GetMemoryUsage() const {
    return root ? root->GetMemoryUsage() : 0;
}

void SparseVoxelOctree::OptimizeSubtree(std::shared_ptr<OctreeNode>& node) {
    if (!node || node->isLeaf) return;
    
    // Recursively optimize children first
    for (auto& child : node->children) {
        if (child) {
            OptimizeSubtree(child);
        }
    }
    
    // Try to merge this node
    node->TryMerge();
}

void SparseVoxelOctree::Optimize() {
    OptimizeSubtree(root);
}

void SparseVoxelOctree::Clear() {
    root = std::make_shared<OctreeNode>(
        Vector3{worldOrigin.x + worldSize * 0.5f, worldOrigin.y + worldSize * 0.5f, worldOrigin.z + worldSize * 0.5f},
        worldSize * 0.5f, 0
    );
}

void SparseVoxelOctree::FromChunk(const Chunk& chunk, const Vector3& chunkWorldOrigin) {
    // Convert traditional chunk data to octree
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType blockType = chunk.GetBlock(x, y, z);
                if (blockType != BlockType::AIR) {
                    Vector3 worldPos = {
                        chunkWorldOrigin.x + x,
                        chunkWorldOrigin.y + y,
                        chunkWorldOrigin.z + z
                    };
                    SetBlock(worldPos, blockType);
                }
            }
        }
    }
}

void SparseVoxelOctree::ToChunk(Chunk& chunk, const Vector3& chunkWorldOrigin) const {
    // Convert octree data back to traditional chunk
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                Vector3 worldPos = {
                    chunkWorldOrigin.x + x,
                    chunkWorldOrigin.y + y,
                    chunkWorldOrigin.z + z
                };
                BlockType blockType = GetBlock(worldPos);
                chunk.SetBlock(x, y, z, blockType);
            }
        }
    }
}

void SparseVoxelOctree::DebugDraw() const {
    if (root) {
        DebugDrawNode(root, RED);
    }
}

void SparseVoxelOctree::DebugDrawNode(const std::shared_ptr<OctreeNode>& node, Color color) const {
    if (!node) return;
    
    // Draw node bounds as wireframe cube
    Vector3 min = {node->center.x - node->size, node->center.y - node->size, node->center.z - node->size};
    Vector3 max = {node->center.x + node->size, node->center.y + node->size, node->center.z + node->size};
    
    DrawCubeWires({node->center.x, node->center.y, node->center.z}, 
                  node->size * 2, node->size * 2, node->size * 2, color);
    
    // Recursively draw children
    if (!node->isLeaf) {
        Color childColor = {
            static_cast<unsigned char>(color.r * 0.8f),
            static_cast<unsigned char>(color.g * 0.8f),
            static_cast<unsigned char>(color.b * 0.8f),
            color.a
        };
        
        for (const auto& child : node->children) {
            if (child) {
                DebugDrawNode(child, childColor);
            }
        }
    }
}

// OctreeUtils implementation
namespace OctreeUtils {
    Vector3 WorldToOctree(const Vector3& worldPos, const Vector3& origin, float size) {
        return {
            (worldPos.x - origin.x) / size,
            (worldPos.y - origin.y) / size,
            (worldPos.z - origin.z) / size
        };
    }
    
    Vector3 OctreeToWorld(const Vector3& octreePos, const Vector3& origin, float size) {
        return {
            origin.x + octreePos.x * size,
            origin.y + octreePos.y * size,
            origin.z + octreePos.z * size
        };
    }
    
    float GetNodeSize(float rootSize, uint8_t level) {
        return rootSize / (1 << level);
    }
    
    uint64_t GetMaxNodesAtDepth(uint8_t depth) {
        return 1ULL << (depth * 3);
    }
}
