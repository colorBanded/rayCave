#include "ChunkManager.h"
#include "RegionManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#ifdef _WIN32
    #include <direct.h>
#endif

// Helper functions for cross-platform file operations
namespace {
    bool CreateDirectoryRecursive(const std::string& path) {
        if (path.empty()) return true;
        
        // Check if directory already exists
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            return true;
        }
        
        // Find parent directory
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string parent = path.substr(0, pos);
            if (!CreateDirectoryRecursive(parent)) {
                return false;
            }
        }
        
        // Create this directory
        #ifdef _WIN32
            return _mkdir(path.c_str()) == 0 || errno == EEXIST;
        #else
            return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
        #endif
    }
    
    bool FileExists(const std::string& path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
    
    std::string GetDirectoryFromPath(const std::string& filepath) {
        size_t pos = filepath.find_last_of("/\\");
        if (pos != std::string::npos) {
            return filepath.substr(0, pos);
        }
        return "";
    }
}

ChunkManager::ChunkManager(const std::string& worldPath, int renderDist, int numThreads)
    : worldPath(worldPath), renderDistance(renderDist), loadDistance(renderDist + 2),
      numWorkerThreads(numThreads), shouldStop(false), lastPlayerChunk(0, 0) {
}

ChunkManager::~ChunkManager() {
    Shutdown();
}

bool ChunkManager::Initialize(int worldSeed) {
    std::cout << "Initializing ChunkManager with seed: " << worldSeed << std::endl;
    
    // Create world directory if it doesn't exist
    CreateDirectoryRecursive(worldPath);
    
    // Initialize world generator
    worldGenerator = std::make_unique<WorldGenerator>(worldSeed);
    
    // Initialize region manager
    regionManager = std::make_unique<RegionManager>(worldPath);
    
    // Load world metadata if it exists
    if (!LoadWorld()) {
        std::cout << "Creating new world with seed: " << worldSeed << std::endl;
        SaveWorld(); // Save initial world data
    }
    
    // Start worker threads
    shouldStop = false;
    workerThreads.reserve(numWorkerThreads);
    for (int i = 0; i < numWorkerThreads; i++) {
        workerThreads.emplace_back(&ChunkManager::WorkerThreadFunc, this);
    }
    
    std::cout << "ChunkManager initialized successfully!" << std::endl;
    return true;
}

void ChunkManager::PreGenerateSpawnChunks(Vector3 spawnPos, int radius) {
    std::cout << "Pre-generating spawn chunks around (" << spawnPos.x << ", " << spawnPos.z << ") with radius " << radius << std::endl;
    
    ChunkCoord spawnChunk = ChunkCoord::FromWorldPos(spawnPos.x, spawnPos.z);
    std::cout << "Spawn chunk coordinate: (" << spawnChunk.x << ", " << spawnChunk.z << ")" << std::endl;
    
    int chunksGenerated = 0;
    int chunksLoaded = 0;
    
    // Generate chunks in a square around spawn SYNCHRONOUSLY
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            ChunkCoord coord(spawnChunk.x + dx, spawnChunk.z + dz);
            
            // Only generate if chunk doesn't already exist
            if (GetChunk(coord) == nullptr) {
                // Generate chunk immediately and synchronously
                auto chunk = std::make_unique<Chunk>(coord);
                
                // Try to load from region file first
                if (regionManager && regionManager->LoadChunk(*chunk)) {
                    chunksLoaded++;
                } else {
                    // Generate new chunk synchronously
                    if (worldGenerator) {
                        worldGenerator->GenerateChunk(*chunk);
                        chunksGenerated++;
                        
                        // Debug block counting removed for performance
                    }
                }
                
                // Add to loaded chunks immediately
                {
                    std::lock_guard<std::mutex> lock(chunkMutex);
                    loadedChunks[coord] = std::move(chunk);
                }
            } else {
                // Chunk already exists
            }
        }
    }
    
    std::cout << "Spawn chunk pre-generation complete! Generated " << chunksGenerated << " new chunks, loaded " << chunksLoaded << " from files." << std::endl;
}

int ChunkManager::GetActualSurfaceHeight(float worldX, float worldZ) {
    Chunk* chunk = GetChunkFromWorldPos(worldX, worldZ);
    if (!chunk) {
        // Fallback to world generator height if chunk not loaded
        if (worldGenerator) {
            return worldGenerator->GetHeightAt(worldX, worldZ);
        }
        return 70; // Default height
    }
    
    // Get local coordinates within the chunk
    ChunkCoord chunkCoord = ChunkCoord::FromWorldPos(worldX, worldZ);
    Vector3 worldOrigin = chunkCoord.GetWorldOrigin();
    int localX = static_cast<int>(worldX - worldOrigin.x);
    int localZ = static_cast<int>(worldZ - worldOrigin.z);
    
    // Make sure coordinates are within chunk bounds
    localX = std::max(0, std::min(localX, CHUNK_SIZE - 1));
    localZ = std::max(0, std::min(localZ, CHUNK_SIZE - 1));
    
    // Find the topmost non-air block
    for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
        BlockType block = chunk->GetBlock(localX, y, localZ);
        if (block != BlockType::AIR) {
            return y;
        }
    }
    
    return 1; // Default to ground level if no blocks found
}

void ChunkManager::Shutdown() {
    if (shouldStop.load()) {
        return; // Already shut down
    }
    
    std::cout << "Shutting down ChunkManager..." << std::endl;
    
    // Stop worker threads
    shouldStop = true;
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
    
    // Save all dirty chunks
    SaveAllChunks();
    
    // Clear loaded chunks
    std::lock_guard<std::mutex> lock(chunkMutex);
    loadedChunks.clear();
    
    std::cout << "ChunkManager shut down successfully!" << std::endl;
}

Chunk* ChunkManager::GetChunk(ChunkCoord coord) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    auto it = loadedChunks.find(coord);
    if (it != loadedChunks.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

Chunk* ChunkManager::GetChunkFromWorldPos(float worldX, float worldZ) {
    ChunkCoord coord = ChunkCoord::FromWorldPos(worldX, worldZ);
    return GetChunk(coord);
}

BlockType ChunkManager::GetBlock(float worldX, float worldY, float worldZ) {
    if (worldY < 0 || worldY >= CHUNK_HEIGHT) {
        return BlockType::AIR;
    }
    
    Chunk* chunk = GetChunkFromWorldPos(worldX, worldZ);
    if (chunk == nullptr) {
        return BlockType::AIR;
    }
    
    // Convert world coordinates to chunk-local coordinates (optimized - avoid redundant calculations)
    int chunkX = static_cast<int>(floor(worldX / CHUNK_SIZE));
    int chunkZ = static_cast<int>(floor(worldZ / CHUNK_SIZE));
    int localX = static_cast<int>(worldX) - (chunkX * CHUNK_SIZE);
    int localZ = static_cast<int>(worldZ) - (chunkZ * CHUNK_SIZE);
    
    // Handle negative coordinates properly (branchless where possible)
    localX = (localX < 0) ? localX + CHUNK_SIZE : localX;
    localZ = (localZ < 0) ? localZ + CHUNK_SIZE : localZ;
    
    return chunk->GetBlock(localX, static_cast<int>(worldY), localZ);
}

void ChunkManager::SetBlock(float worldX, float worldY, float worldZ, BlockType blockType) {
    if (worldY < 0 || worldY >= CHUNK_HEIGHT) {
        return;
    }
    
    Chunk* chunk = GetChunkFromWorldPos(worldX, worldZ);
    if (chunk == nullptr) {
        return; // Chunk not loaded
    }
    
    // Convert world coordinates to chunk-local coordinates (optimized - avoid redundant calculations)
    int chunkX = static_cast<int>(floor(worldX / CHUNK_SIZE));
    int chunkZ = static_cast<int>(floor(worldZ / CHUNK_SIZE));
    int localX = static_cast<int>(worldX) - (chunkX * CHUNK_SIZE);
    int localZ = static_cast<int>(worldZ) - (chunkZ * CHUNK_SIZE);
    
    // Handle negative coordinates properly (branchless where possible)
    localX = (localX < 0) ? localX + CHUNK_SIZE : localX;
    localZ = (localZ < 0) ? localZ + CHUNK_SIZE : localZ;
    
    chunk->SetBlock(localX, static_cast<int>(worldY), localZ, blockType);
}

void ChunkManager::UpdatePlayerPosition(Vector3 playerPos, Vector3 playerVelocity) {
    ChunkCoord playerChunk = ChunkCoord::FromWorldPos(playerPos.x, playerPos.z);
    
    if (!(playerChunk == lastPlayerChunk)) {
        lastPlayerChunk = playerChunk;
        
        // Immediately generate the chunk the player is standing on if it doesn't exist
        if (GetChunk(playerChunk) == nullptr) {
            GenerateChunk(playerChunk);
        }
        
        // Also immediately generate adjacent chunks for smoother movement
        ChunkCoord adjacentChunks[] = {
            ChunkCoord(playerChunk.x + 1, playerChunk.z),     // East
            ChunkCoord(playerChunk.x - 1, playerChunk.z),     // West
            ChunkCoord(playerChunk.x, playerChunk.z + 1),     // South
            ChunkCoord(playerChunk.x, playerChunk.z - 1)      // North
        };
        
        for (const auto& coord : adjacentChunks) {
            if (GetChunk(coord) == nullptr) {
                GenerateChunk(coord);
            }
        }
        
        // Load chunks around player (remaining ones in priority order)
        LoadChunksAroundPlayer(playerChunk, playerVelocity);
        
        // Unload distant chunks
        UnloadDistantChunks(playerChunk);
    }
}

void ChunkManager::LoadChunksAroundPlayer(ChunkCoord playerChunk, Vector3 playerVelocity) {
    std::vector<ChunkCoord> chunksToLoad = GetChunksInRadius(playerChunk, loadDistance);
    
    // Pre-calculate movement direction once using squared magnitude for performance
    float velocityMagnitudeSq = playerVelocity.x * playerVelocity.x + playerVelocity.z * playerVelocity.z;
    Vector3 movementDirection = {0, 0, 0};
    bool hasMovement = velocityMagnitudeSq > 0.01f; // 0.1f squared
    
    if (hasMovement) {
        float invMagnitude = 1.0f / sqrtf(velocityMagnitudeSq); // Only sqrt when needed
        movementDirection.x = playerVelocity.x * invMagnitude;
        movementDirection.z = playerVelocity.z * invMagnitude;
    }
    
    for (const ChunkCoord& coord : chunksToLoad) {
        if (GetChunk(coord) == nullptr && !IsChunkQueued(coord)) {
            // Calculate base priority based on Manhattan distance
            float distance = static_cast<float>(std::abs(coord.x - playerChunk.x) + std::abs(coord.z - playerChunk.z));
            
            // Adjust priority based on movement direction (optimized)
            if (hasMovement) {
                float dx = static_cast<float>(coord.x - playerChunk.x);
                float dz = static_cast<float>(coord.z - playerChunk.z);
                
                // Normalize chunk direction efficiently using squared distance
                float chunkDistanceSq = dx * dx + dz * dz;
                if (chunkDistanceSq > 0) {
                    // Use fast inverse square root approximation or avoid sqrt when possible
                    float alignment = (dx * movementDirection.x + dz * movementDirection.z) / sqrtf(chunkDistanceSq);
                    
                    // Reduce priority if chunk is in movement direction
                    if (alignment > 0) {
                        distance *= (1.0f - alignment * 0.5f);
                    }
                }
            }
            
            QueueChunkForGeneration(coord, distance);
        }
    }
}

void ChunkManager::UnloadDistantChunks(ChunkCoord playerChunk) {
    std::vector<ChunkCoord> chunksToUnload;
    
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        for (const auto& pair : loadedChunks) {
            if (!IsWithinDistance(playerChunk, pair.first, loadDistance + 1)) {
                chunksToUnload.push_back(pair.first);
            }
        }
    }
    
    for (const ChunkCoord& coord : chunksToUnload) {
        UnloadChunk(coord);
    }
}

void ChunkManager::RenderChunks(const Vector3& playerPos, TextureManager& textureManager) {
    ChunkCoord playerChunk = ChunkCoord::FromWorldPos(playerPos.x, playerPos.z);
    std::vector<ChunkCoord> chunksToRender = GetChunksInRadius(playerChunk, renderDistance);
    
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    for (const ChunkCoord& coord : chunksToRender) {
        auto it = loadedChunks.find(coord);
        if (it != loadedChunks.end() && it->second->IsGenerated()) {
            // Get neighboring chunks for optimized rendering
            const Chunk* neighbors[4] = {nullptr, nullptr, nullptr, nullptr};
            
            // North neighbor (z-1)
            ChunkCoord northCoord(coord.x, coord.z - 1);
            auto northIt = loadedChunks.find(northCoord);
            if (northIt != loadedChunks.end()) neighbors[0] = northIt->second.get();
            
            // South neighbor (z+1)
            ChunkCoord southCoord(coord.x, coord.z + 1);
            auto southIt = loadedChunks.find(southCoord);
            if (southIt != loadedChunks.end()) neighbors[1] = southIt->second.get();
            
            // East neighbor (x+1)
            ChunkCoord eastCoord(coord.x + 1, coord.z);
            auto eastIt = loadedChunks.find(eastCoord);
            if (eastIt != loadedChunks.end()) neighbors[2] = eastIt->second.get();
            
            // West neighbor (x-1)
            ChunkCoord westCoord(coord.x - 1, coord.z);
            auto westIt = loadedChunks.find(westCoord);
            if (westIt != loadedChunks.end()) neighbors[3] = westIt->second.get();
            
            // Use optimized rendering with neighbor information
            it->second->RenderOptimized(textureManager, neighbors);
        }
    }
}

void ChunkManager::QueueChunkForGeneration(ChunkCoord coord, float priority) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    // Efficiently check if already queued using the set
    if (queuedChunks.find(coord) == queuedChunks.end()) {
        generationQueue.emplace(coord, priority);
        queuedChunks.insert(coord);
    }
}

bool ChunkManager::IsChunkQueued(ChunkCoord coord) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    // Check if chunk is already loaded
    if (loadedChunks.find(coord) != loadedChunks.end()) {
        return true;
    }
    
    // Efficiently check if in generation queue using the set
    return queuedChunks.find(coord) != queuedChunks.end();
}

void ChunkManager::WorkerThreadFunc() {
    while (!shouldStop.load()) {
        bool didWork = false;
        
        // Process generation queue
        PriorityChunk priorityChunk(ChunkCoord(0, 0), 0.0f);
        {
            std::lock_guard<std::mutex> lock(chunkMutex);
            if (!generationQueue.empty()) {
                priorityChunk = generationQueue.top();
                generationQueue.pop();
                queuedChunks.erase(priorityChunk.coord); // Remove from queued set
                didWork = true;
            }
        }
        
        if (didWork) {
            GenerateChunk(priorityChunk.coord);
        }
        
        // Process saving queue
        ProcessSavingQueue();
        
        // Only sleep if we didn't do any work to prevent busy waiting
        if (!didWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void ChunkManager::ProcessGenerationQueue() {
    PriorityChunk priorityChunk(ChunkCoord(0, 0), 0.0f);
    bool hasWork = false;
    
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        if (!generationQueue.empty()) {
            priorityChunk = generationQueue.top();
            generationQueue.pop();
            queuedChunks.erase(priorityChunk.coord); // Remove from queued set
            hasWork = true;
        }
    }
    
    if (hasWork) {
        GenerateChunk(priorityChunk.coord);
    }
}

void ChunkManager::ProcessSavingQueue() {
    ChunkCoord coord(0, 0);
    bool hasWork = false;
    
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        if (!savingQueue.empty()) {
            coord = savingQueue.front();
            savingQueue.pop();
            hasWork = true;
        }
    }
    
    if (hasWork) {
        SaveChunk(coord);
    }
}

void ChunkManager::GenerateChunk(ChunkCoord coord) {
    // Try to load from region file first
    auto chunk = std::make_unique<Chunk>(coord);
    
    if (regionManager && regionManager->LoadChunk(*chunk)) {
        // Chunk loaded from file
    } else {
        // Generate new chunk
        if (worldGenerator) {
            worldGenerator->GenerateChunk(*chunk);
        }
    }
    
    // Add to loaded chunks
    {
        std::lock_guard<std::mutex> lock(chunkMutex);
        loadedChunks[coord] = std::move(chunk);
    }
}

void ChunkManager::UnloadChunk(ChunkCoord coord) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    auto it = loadedChunks.find(coord);
    if (it != loadedChunks.end()) {
        // Queue for saving if dirty
        if (it->second->IsDirty()) {
            savingQueue.push(coord);
        }
        
        loadedChunks.erase(it);
    }
}

void ChunkManager::SaveChunk(ChunkCoord coord) {
    Chunk* chunk = GetChunk(coord);
    if (chunk != nullptr && chunk->IsDirty()) {
        if (regionManager && regionManager->SaveChunk(*chunk)) {
            chunk->SetDirty(false);
        }
    }
}

void ChunkManager::SaveAllChunks() {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    std::cout << "Saving all chunks..." << std::endl;
    int savedCount = 0;
    
    for (auto& pair : loadedChunks) {
        if (pair.second->IsDirty()) {
            if (regionManager && regionManager->SaveChunk(*pair.second)) {
                pair.second->SetDirty(false);
                savedCount++;
            }
        }
    }
    
    std::cout << "Saved " << savedCount << " chunks to region files" << std::endl;
}

// Legacy method - kept for potential compatibility/debugging
std::string ChunkManager::GetChunkFilePath(ChunkCoord coord) const {
    return worldPath + "chunks/chunk_" + std::to_string(coord.x) + "_" + std::to_string(coord.z) + ".dat";
}

bool ChunkManager::ChunkFileExists(ChunkCoord coord) const {
    return regionManager && regionManager->ChunkExists(coord);
}

void ChunkManager::SaveWorld() {
    std::string worldFile = worldPath + "world.dat";
    
    std::ofstream file(worldFile, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Failed to save world metadata!" << std::endl;
        return;
    }
    
    // Write world metadata
    file.write("WORLD", 5);
    int version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(int));
    
    int seed = worldGenerator ? worldGenerator->GetSeed() : 12345;
    file.write(reinterpret_cast<const char*>(&seed), sizeof(int));
    
    file.close();
    std::cout << "World metadata saved successfully!" << std::endl;
}

bool ChunkManager::LoadWorld() {
    std::string worldFile = worldPath + "world.dat";
    
    if (!FileExists(worldFile)) {
        return false;
    }
    
    std::ifstream file(worldFile, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read world metadata
    char header[6];
    file.read(header, 5);
    header[5] = '\0';
    
    if (std::string(header) != "WORLD") {
        file.close();
        return false;
    }
    
    int version;
    file.read(reinterpret_cast<char*>(&version), sizeof(int));
    
    if (version != 1) {
        file.close();
        return false;
    }
    
    int seed;
    file.read(reinterpret_cast<char*>(&seed), sizeof(int));
    
    if (worldGenerator) {
        worldGenerator->SetSeed(seed);
    }
    
    file.close();
    std::cout << "Loaded existing world with seed: " << seed << std::endl;
    return true;
}

bool ChunkManager::IsWithinDistance(ChunkCoord center, ChunkCoord target, int distance) const {
    int dx = abs(center.x - target.x);
    int dz = abs(center.z - target.z);
    return dx <= distance && dz <= distance;
}

std::vector<ChunkCoord> ChunkManager::GetChunksInRadius(ChunkCoord center, int radius) const {
    std::vector<ChunkCoord> chunks;
    int totalChunks = (2 * radius + 1) * (2 * radius + 1);
    chunks.reserve(totalChunks);
    
    // Generate chunks in spiral order (closest first) to avoid sorting
    chunks.push_back(center); // Add center first
    
    for (int ring = 1; ring <= radius; ring++) {
        // Add chunks in a square ring around the center
        for (int x = center.x - ring; x <= center.x + ring; x++) {
            for (int z = center.z - ring; z <= center.z + ring; z++) {
                // Skip chunks not on the ring edge and the center (already added)
                if (abs(x - center.x) == ring || abs(z - center.z) == ring) {
                    if (x != center.x || z != center.z) { // Skip center
                        chunks.emplace_back(x, z);
                    }
                }
            }
        }
    }
    
    return chunks;
}
