#pragma once

#include "Chunk.h"
#include "SparseVoxelOctree.h"
#include "MarchingCubes.h"
#include "TextureManager.h"
#include <memory>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <map>

// Rendering modes for chunks
enum class ChunkRenderMode {
    TRADITIONAL_BLOCKS,    // Original cube-based rendering
    MARCHING_CUBES,       // Smooth terrain using marching cubes
    HYBRID,               // Mix of both based on block types
    ADAPTIVE              // Choose based on chunk content and distance
};

// Level of Detail system for chunks
enum class ChunkLOD {
    HIGH = 0,    // Full resolution (1.0 unit per voxel)
    MEDIUM = 1,  // Half resolution (2.0 units per voxel)
    LOW = 2,     // Quarter resolution (4.0 units per voxel)
    VERY_LOW = 3 // Eighth resolution (8.0 units per voxel)
};

class EnhancedChunkSystem {
private:
    // Core data storage
    SparseVoxelOctree octree;
    
    // Rendering components
    MarchingCubesMesh smoothMesh;
    std::vector<QuadMesh> traditionalQuads; // For greedy meshing
    
    // Chunk properties
    ChunkCoord coord;
    Vector3 worldOrigin;
    ChunkRenderMode renderMode;
    ChunkLOD currentLOD;
    
    // State management
    std::atomic<bool> isDirty{true};
    std::atomic<bool> meshDirty{true};
    std::atomic<bool> isGenerated{false};
    std::atomic<bool> isLoaded{true};
    
    // Performance metrics
    size_t lastVertexCount;
    size_t lastTriangleCount;
    float lastGenerationTime;
    
    // Internal methods
    void GenerateTraditionalMesh();
    void GenerateSmoothMesh();
    void GenerateHybridMesh();
    void UpdateLOD(float distanceToPlayer);
    
    // Optimization methods
    void OptimizeForDistance(float distance);
    bool ShouldUseSmoothTerrain() const;
    
public:
    EnhancedChunkSystem(ChunkCoord coord, float octreeSize = 32.0f);
    ~EnhancedChunkSystem() = default;
    
    // Block operations (thread-safe)
    void SetBlock(int x, int y, int z, BlockType blockType);
    BlockType GetBlock(int x, int y, int z) const;
    void SetBlocks(const std::vector<Vector3>& positions, const std::vector<BlockType>& types);
    
    // Bulk operations for world generation
    void FillRegion(const Vector3& min, const Vector3& max, BlockType blockType);
    void GenerateTerrain(int seed, float noiseScale = 0.1f);
    
    // Mesh generation and management
    void UpdateMesh(float playerDistance);
    void ForceRegenerateMesh();
    void SetRenderMode(ChunkRenderMode mode);
    void SetLOD(ChunkLOD lod);
    
    // Rendering
    void Render(TextureManager& textureManager, const Vector3& cameraPosition) const;
    void RenderWireframe() const;
    void RenderDebugInfo() const;
    
    // Internal rendering helpers
    void GenerateQuadsForFaceFromOctree(BlockFace face, const Chunk* neighbors[4]);
    void RenderQuad(const QuadMesh& quad, TextureManager& textureManager) const;
    
    // Memory and performance
    size_t GetMemoryUsage() const;
    size_t GetVertexCount() const;
    size_t GetTriangleCount() const;
    float GetLastGenerationTime() const { return lastGenerationTime; }
    
    // State queries
    bool IsDirty() const { return isDirty.load(); }
    bool IsGenerated() const { return isGenerated.load(); }
    bool IsLoaded() const { return isLoaded.load(); }
    Vector3 GetWorldOrigin() const { return worldOrigin; }
    ChunkCoord GetCoord() const { return coord; }
    ChunkRenderMode GetRenderMode() const { return renderMode; }
    ChunkLOD GetCurrentLOD() const { return currentLOD; }
    
    // Integration with traditional chunk system
    void FromTraditionalChunk(const Chunk& traditionalChunk);
    void ToTraditionalChunk(Chunk& traditionalChunk) const;
    
    // Serialization for world saving/loading
    std::vector<unsigned char> Serialize() const;
    bool Deserialize(const std::vector<unsigned char>& data);
    
    // Thread-safe state management
    void MarkDirty() { isDirty.store(true); meshDirty.store(true); }
    void MarkClean() { isDirty.store(false); }
};

// Enhanced chunk manager that uses the new system
class OptimizedChunkManager {
private:
    // Enhanced chunk storage - using simple map for now to avoid template issues
    std::map<std::pair<int, int>, std::unique_ptr<EnhancedChunkSystem>> chunks;
    
public:
    // Performance settings
    struct PerformanceSettings {
        float maxSmoothTerrainDistance = 64.0f;    // Max distance for marching cubes
        float lodDistance[4] = {32.0f, 64.0f, 128.0f, 256.0f}; // LOD switching distances
        int maxChunksPerFrame = 2;                  // Max chunks to update per frame
        bool adaptiveQuality = true;                // Automatically adjust quality
        bool useHybridRendering = true;             // Mix smooth and block rendering
        float targetFrameTime = 16.6f;              // Target 60 FPS (16.6ms per frame)
    };

private:
    PerformanceSettings settings;
    
    // Update scheduling - simplified for now
    std::queue<ChunkCoord> updateQueue;
    std::vector<ChunkCoord> updatingChunks;  // Simplified from unordered_set
    
    // Performance monitoring
    float lastFrameTime;
    int chunksUpdatedThisFrame;
    
    // Thread management
    std::mutex chunkMutex;
    std::atomic<bool> shouldStop{false};
    
    // Internal methods
    void UpdateChunkLOD(EnhancedChunkSystem& chunk, const Vector3& playerPos);
    void ScheduleChunkUpdate(ChunkCoord coord);
    ChunkRenderMode DetermineOptimalRenderMode(const EnhancedChunkSystem& chunk, float distance) const;
    
public:
    OptimizedChunkManager();
    ~OptimizedChunkManager();
    
    // Chunk management
    void LoadChunk(ChunkCoord coord);
    void UnloadChunk(ChunkCoord coord);
    EnhancedChunkSystem* GetChunk(ChunkCoord coord);
    const EnhancedChunkSystem* GetChunk(ChunkCoord coord) const;
    
    // Player-centric operations
    void UpdatePlayerPosition(const Vector3& playerPos, const Vector3& playerVelocity);
    void LoadChunksAroundPlayer(const Vector3& playerPos, int radius);
    void UnloadDistantChunks(const Vector3& playerPos, int maxDistance);
    
    // Rendering and updates
    void Update(float deltaTime, const Vector3& playerPos);
    void RenderChunks(const Vector3& playerPos, TextureManager& textureManager);
    void RenderChunksWireframe(const Vector3& playerPos);
    
    // Performance management
    void SetPerformanceSettings(const PerformanceSettings& newSettings);
    const PerformanceSettings& GetPerformanceSettings() const { return settings; }
    void AdaptQualityForPerformance();
    
    // Statistics and debugging
    size_t GetLoadedChunkCount() const;
    size_t GetTotalVertexCount() const;
    size_t GetTotalTriangleCount() const;
    size_t GetTotalMemoryUsage() const;
    float GetAverageGenerationTime() const;
    
    // Settings
    void SetMaxSmoothTerrainDistance(float distance) { settings.maxSmoothTerrainDistance = distance; }
    void SetAdaptiveQuality(bool enabled) { settings.adaptiveQuality = enabled; }
    void SetTargetFrameTime(float milliseconds) { settings.targetFrameTime = milliseconds; }
};

// Utility functions for chunk optimization
namespace ChunkOptimization {
    // Determine if a chunk should use smooth terrain based on content
    bool ShouldUseMarchingCubes(const EnhancedChunkSystem& chunk);
    
    // Calculate optimal LOD based on distance and performance
    ChunkLOD CalculateOptimalLOD(float distance, float currentFrameTime, float targetFrameTime);
    
    // Estimate rendering cost for a chunk
    float EstimateRenderingCost(const EnhancedChunkSystem& chunk, ChunkRenderMode mode, ChunkLOD lod);
    
    // Memory optimization utilities
    void OptimizeChunkMemory(EnhancedChunkSystem& chunk);
    size_t GetOptimalOctreeDepth(const Vector3& chunkSize, float targetResolution);
}
