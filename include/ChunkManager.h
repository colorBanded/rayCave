#pragma once

#include "Chunk.h"
#include "WorldGenerator.h"
#include "RegionManager.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

// Priority chunk structure for faster loading
struct PriorityChunk {
    ChunkCoord coord;
    float priority; // Lower = higher priority (distance from player)
    
    PriorityChunk(ChunkCoord c, float p) : coord(c), priority(p) {}
    
    // For priority_queue (note: priority_queue is max-heap, so we reverse comparison)
    bool operator<(const PriorityChunk& other) const {
        return priority > other.priority; // Higher priority (lower distance) comes first
    }
};

class ChunkManager {
private:
    // Chunk storage
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> loadedChunks;
    
    // World generation
    std::unique_ptr<WorldGenerator> worldGenerator;
    
    // Region-based file system
    std::unique_ptr<RegionManager> regionManager;
    
    // Threading for chunk loading/generation - now with multiple worker threads
    std::vector<std::thread> workerThreads;
    std::mutex chunkMutex;
    std::priority_queue<PriorityChunk> generationQueue;
    std::queue<ChunkCoord> savingQueue;
    std::unordered_set<ChunkCoord, ChunkCoordHash> queuedChunks; // Track queued chunks efficiently
    std::atomic<bool> shouldStop;
    
    // Settings
    int renderDistance;        // Chunks to render around player
    int loadDistance;         // Chunks to keep loaded around player
    std::string worldPath;    // Path to save world data
    int numWorkerThreads;     // Number of worker threads for parallel generation
    
    // Player tracking for chunk loading
    ChunkCoord lastPlayerChunk;
    
public:
    ChunkManager(const std::string& worldPath = "world/", int renderDist = 8, int numThreads = 4);
    ~ChunkManager();
    
    // Initialization
    bool Initialize(int worldSeed = 12345);
    void Shutdown();
    
    // Pre-generate spawn chunks (call before player initialization)
    void PreGenerateSpawnChunks(Vector3 spawnPos, int radius = 2);
    
    // Chunk access
    Chunk* GetChunk(ChunkCoord coord);
    Chunk* GetChunk(int chunkX, int chunkZ) { return GetChunk(ChunkCoord(chunkX, chunkZ)); }
    
    // Get chunk from world position
    Chunk* GetChunkFromWorldPos(float worldX, float worldZ);
    
    // Block access (handles chunk boundaries)
    BlockType GetBlock(float worldX, float worldY, float worldZ);
    void SetBlock(float worldX, float worldY, float worldZ, BlockType blockType);
    
    // Player position update (triggers chunk loading/unloading)
    void UpdatePlayerPosition(Vector3 playerPos, Vector3 playerVelocity = {0, 0, 0});
    
    // Rendering
    void RenderChunks(const Vector3& playerPos, TextureManager& textureManager);
    
    // Save system
    void SaveChunk(ChunkCoord coord);
    void SaveAllChunks();
    void SaveWorld(); // Save world metadata
    
    // Load system  
    bool LoadChunk(ChunkCoord coord);
    bool LoadWorld(); // Load world metadata
    
    // Chunk management
    void LoadChunksAroundPlayer(ChunkCoord playerChunk, Vector3 playerVelocity = {0, 0, 0});
    void UnloadDistantChunks(ChunkCoord playerChunk);
    
    // Generation queue management
    void QueueChunkForGeneration(ChunkCoord coord, float priority = 0.0f);
    bool IsChunkQueued(ChunkCoord coord);
    
    // Statistics
    int GetLoadedChunkCount() const { return loadedChunks.size(); }
    int GetQueuedGenerationCount() const { return generationQueue.size(); }
    int GetQueuedSavingCount() const { return savingQueue.size(); }
    
    // Settings
    void SetRenderDistance(int distance) { renderDistance = distance; }
    void SetLoadDistance(int distance) { loadDistance = distance; }
    int GetRenderDistance() const { return renderDistance; }
    int GetLoadDistance() const { return loadDistance; }
    
    // World generator access
    WorldGenerator* GetWorldGenerator() { return worldGenerator.get(); }
    
    // Get actual surface height from loaded chunks (more accurate than noise-based height)
    int GetActualSurfaceHeight(float worldX, float worldZ);
    
private:
    // Worker thread functions
    void WorkerThreadFunc();
    void ProcessGenerationQueue();
    void ProcessSavingQueue();
    
    // File I/O (now region-based)
    std::string GetChunkFilePath(ChunkCoord coord) const; // Legacy method for compatibility
    bool ChunkFileExists(ChunkCoord coord) const;
    
    // Chunk lifecycle
    void GenerateChunk(ChunkCoord coord);
    void UnloadChunk(ChunkCoord coord);
    
    // Utility
    bool IsWithinDistance(ChunkCoord center, ChunkCoord target, int distance) const;
    std::vector<ChunkCoord> GetChunksInRadius(ChunkCoord center, int radius) const;
    
    // Thread safety
    void LockChunks() { chunkMutex.lock(); }
    void UnlockChunks() { chunkMutex.unlock(); }
};
