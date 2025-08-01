#pragma once

#include "raylib.h"
#include "SparseVoxelOctree.h"
#include <vector>
#include <array>
#include <unordered_map>

// Marching Cubes lookup tables and configuration
class MarchingCubes {
private:
    // Cube corner positions (relative to cube center)
    static const std::array<Vector3, 8> CUBE_CORNERS;
    
    // Edge connections between corners
    static const std::array<std::array<int, 2>, 12> CUBE_EDGES;
    
    // Marching cubes lookup table - maps cube configuration to triangles
    static const std::array<std::array<int, 16>, 256> TRIANGLE_TABLE;
    
    // Edge lookup table - maps cube configuration to active edges
    static const std::array<int, 256> EDGE_TABLE;
    
    // Interpolation method for smooth surfaces
    Vector3 InterpolateVertex(const Vector3& p1, const Vector3& p2, float val1, float val2, float isolevel = 0.5f) const;
    
    // Calculate density/occupancy value for a position
    float GetDensity(const SparseVoxelOctree& octree, const Vector3& pos) const;
    
    // Generate normals using gradient estimation
    Vector3 CalculateNormal(const SparseVoxelOctree& octree, const Vector3& pos) const;

public:
    MarchingCubes() = default;
    ~MarchingCubes() = default;
    
    // Generate mesh for a single cube
    void ProcessCube(const SparseVoxelOctree& octree, const Vector3& cubePos, float cubeSize,
                    std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                    std::vector<unsigned int>& indices) const;
    
    // Generate mesh for a region
    void GenerateMesh(const SparseVoxelOctree& octree, const Vector3& min, const Vector3& max, 
                     float resolution, std::vector<Vector3>& vertices, 
                     std::vector<Vector3>& normals, std::vector<unsigned int>& indices) const;
    
    // Optimized mesh generation using octree structure
    void GenerateOptimizedMesh(const SparseVoxelOctree& octree, const Vector3& min, const Vector3& max,
                              std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                              std::vector<unsigned int>& indices) const;
    
    // Mesh simplification and optimization
    void SimplifyMesh(std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                     std::vector<unsigned int>& indices, float threshold = 0.01f) const;
    
    // Generate mesh with texture coordinates
    void GenerateMeshWithUV(const SparseVoxelOctree& octree, const Vector3& min, const Vector3& max,
                           float resolution, std::vector<Vector3>& vertices, 
                           std::vector<Vector3>& normals, std::vector<Vector2>& uvs,
                           std::vector<unsigned int>& indices) const;
};

// Utility class for managing marching cubes meshes
class MarchingCubesMesh {
private:
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Vector2> uvs;
    std::vector<unsigned int> indices;
    
    // Raylib mesh object
    Mesh raylibMesh;
    bool meshGenerated;
    bool meshDirty;

public:
    MarchingCubesMesh();
    ~MarchingCubesMesh();
    
    // Mesh generation
    void GenerateFromOctree(const SparseVoxelOctree& octree, const Vector3& min, 
                           const Vector3& max, float resolution = 1.0f);
    void UpdateFromOctree(const SparseVoxelOctree& octree, const Vector3& min, 
                         const Vector3& max, float resolution = 1.0f);
    
    // Mesh management
    void UpdateRaylibMesh();
    void Clear();
    bool IsEmpty() const { return vertices.empty(); }
    
    // Rendering
    void Render(const Vector3& position, const Color& color = WHITE) const;
    void RenderWithTexture(const Vector3& position, const Texture2D& texture) const;
    
    // Getters
    const std::vector<Vector3>& GetVertices() const { return vertices; }
    const std::vector<Vector3>& GetNormals() const { return normals; }
    const std::vector<Vector2>& GetUVs() const { return uvs; }
    const std::vector<unsigned int>& GetIndices() const { return indices; }
    const Mesh& GetRaylibMesh() const { return raylibMesh; }
    
    // Statistics
    size_t GetVertexCount() const { return vertices.size(); }
    size_t GetTriangleCount() const { return indices.size() / 3; }
    size_t GetMemoryUsage() const;
};

// Enhanced chunk class that integrates SVO and Marching Cubes
class EnhancedChunk {
private:
    SparseVoxelOctree octree;
    MarchingCubesMesh marchingMesh;
    
    // Traditional chunk data for compatibility
    Vector3 worldOrigin;
    bool isDirty;
    bool meshDirty;
    
    // LOD system
    float lodDistance;
    int currentLOD;
    std::vector<MarchingCubesMesh> lodMeshes; // Different LOD levels

public:
    EnhancedChunk(const Vector3& origin, float size = 16.0f);
    ~EnhancedChunk() = default;
    
    // Block operations
    void SetBlock(const Vector3& localPos, BlockType blockType);
    BlockType GetBlock(const Vector3& localPos) const;
    
    // Mesh generation
    void UpdateMesh(float resolution = 1.0f);
    void UpdateLODMesh(int lodLevel, float resolution);
    
    // Rendering
    void Render(const Vector3& cameraPosition, const Texture2D& texture = {}) const;
    
    // Integration with existing Chunk class
    void FromTraditionalChunk(const class Chunk& chunk);
    void ToTraditionalChunk(class Chunk& chunk) const;
    
    // Getters/Setters
    Vector3 GetWorldOrigin() const { return worldOrigin; }
    bool IsDirty() const { return isDirty; }
    void MarkDirty() { isDirty = true; meshDirty = true; }
    
    // Memory usage
    size_t GetMemoryUsage() const;
};
