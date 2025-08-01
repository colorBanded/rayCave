#include "OptimizedChunkSystem.h"
#include "WorldGenerator.h"
#include "FastNoise.h"
#include "raymath.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <unordered_map>

// Helper function to calculate distance between two points
float CalculateDistance(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

// Helper function to convert ChunkCoord to map key
std::pair<int, int> ChunkCoordToKey(const ChunkCoord& coord) {
    return {coord.x, coord.z};
}

// EnhancedChunkSystem implementation
EnhancedChunkSystem::EnhancedChunkSystem(ChunkCoord coord, float octreeSize)
    : octree(Vector3{coord.x * 16.0f, 0.0f, coord.z * 16.0f}, octreeSize, 6),
      coord(coord),
      worldOrigin(coord.GetWorldOrigin()),
      renderMode(ChunkRenderMode::ADAPTIVE),
      currentLOD(ChunkLOD::HIGH),
      lastVertexCount(0),
      lastTriangleCount(0),
      lastGenerationTime(0.0f) {
}

void EnhancedChunkSystem::SetBlock(int x, int y, int z, BlockType blockType) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return;
    }
    
    Vector3 worldPos = {worldOrigin.x + x, worldOrigin.y + y, worldOrigin.z + z};
    octree.SetBlock(worldPos, blockType);
    MarkDirty();
}

BlockType EnhancedChunkSystem::GetBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return BlockType::AIR;
    }
    
    Vector3 worldPos = {worldOrigin.x + x, worldOrigin.y + y, worldOrigin.z + z};
    return octree.GetBlock(worldPos);
}

void EnhancedChunkSystem::SetBlocks(const std::vector<Vector3>& positions, const std::vector<BlockType>& types) {
    if (positions.size() != types.size()) return;
    
    std::vector<Vector3> worldPositions;
    worldPositions.reserve(positions.size());
    
    for (const auto& localPos : positions) {
        worldPositions.push_back({
            worldOrigin.x + localPos.x,
            worldOrigin.y + localPos.y,
            worldOrigin.z + localPos.z
        });
    }
    
    octree.SetBlocks(worldPositions, types);
    MarkDirty();
}

void EnhancedChunkSystem::FillRegion(const Vector3& min, const Vector3& max, BlockType blockType) {
    Vector3 worldMin = {worldOrigin.x + min.x, worldOrigin.y + min.y, worldOrigin.z + min.z};
    Vector3 worldMax = {worldOrigin.x + max.x, worldOrigin.y + max.y, worldOrigin.z + max.z};
    
    octree.FillRegion(worldMin, worldMax, blockType);
    MarkDirty();
}

void EnhancedChunkSystem::GenerateTerrain(int seed, float noiseScale) {
    // Simple terrain generation without FastNoise for now
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            float worldX = worldOrigin.x + x;
            float worldZ = worldOrigin.z + z;
            
            // Generate height using simple noise - simplified approach
            float height = 64.0f + 16.0f * sin(worldX * noiseScale) * cos(worldZ * noiseScale);
            int maxY = std::min(static_cast<int>(height), CHUNK_HEIGHT - 1);
            
            for (int y = 0; y <= maxY; y++) {
                BlockType blockType;
                if (y < maxY - 4) {
                    blockType = BlockType::STONE;
                } else if (y < maxY) {
                    blockType = BlockType::DIRT;
                } else {
                    blockType = BlockType::GRASS;
                }
                
                SetBlock(x, y, z, blockType);
            }
        }
    }
    
    isGenerated.store(true);
}

bool EnhancedChunkSystem::ShouldUseSmoothTerrain() const {
    // Determine if this chunk would benefit from marching cubes
    // Check for natural terrain vs constructed blocks
    
    // Sample a few blocks to determine terrain type
    int naturalBlocks = 0;
    int totalSampled = 0;
    
    for (int x = 0; x < CHUNK_SIZE; x += 4) {
        for (int z = 0; z < CHUNK_SIZE; z += 4) {
            for (int y = 32; y < 96; y += 8) { // Sample middle heights
                BlockType block = GetBlock(x, y, z);
                if (block != BlockType::AIR) {
                    totalSampled++;
                    if (block == BlockType::STONE || block == BlockType::DIRT || 
                        block == BlockType::GRASS || block == BlockType::SAND) {
                        naturalBlocks++;
                    }
                }
            }
        }
    }
    
    // Use smooth terrain if >70% natural blocks
    return totalSampled > 0 && (static_cast<float>(naturalBlocks) / totalSampled) > 0.7f;
}

size_t EnhancedChunkSystem::GetMemoryUsage() const {
    size_t totalSize = octree.GetMemoryUsage();
    totalSize += traditionalQuads.size() * sizeof(QuadMesh);
    totalSize += smoothMesh.GetMemoryUsage();
    return totalSize;
}

size_t EnhancedChunkSystem::GetVertexCount() const {
    size_t count = traditionalQuads.size() * 4; // Each quad has 4 vertices
    count += smoothMesh.GetVertexCount();
    return count;
}

size_t EnhancedChunkSystem::GetTriangleCount() const {
    size_t count = traditionalQuads.size() * 2; // Each quad has 2 triangles
    count += smoothMesh.GetTriangleCount();
    return count;
}

void EnhancedChunkSystem::GenerateTraditionalMesh() {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    traditionalQuads.clear();
    
    // Use existing greedy meshing algorithm but with octree data
    const Chunk* neighbors[4] = {nullptr}; // TODO: Get actual neighbors
    
    // Generate quads for each face direction
    BlockFace faces[] = {BlockFace::TOP, BlockFace::BOTTOM, BlockFace::NORTH, 
                        BlockFace::SOUTH, BlockFace::EAST, BlockFace::WEST};
    
    for (BlockFace face : faces) {
        // Generate optimized quads for this face using octree data
        // This is similar to your existing GenerateQuadsForFace but adapted for octree
        GenerateQuadsForFaceFromOctree(face, neighbors);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    lastGenerationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    lastVertexCount = traditionalQuads.size() * 4;
    lastTriangleCount = traditionalQuads.size() * 2;
}

void EnhancedChunkSystem::GenerateSmoothMesh() {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    float resolution = 1.0f;
    switch (currentLOD) {
        case ChunkLOD::MEDIUM: resolution = 2.0f; break;
        case ChunkLOD::LOW: resolution = 4.0f; break;
        case ChunkLOD::VERY_LOW: resolution = 8.0f; break;
        default: resolution = 1.0f; break;
    }
    
    Vector3 meshMin = worldOrigin;
    Vector3 meshMax = {worldOrigin.x + CHUNK_SIZE, worldOrigin.y + CHUNK_HEIGHT, worldOrigin.z + CHUNK_SIZE};
    
    smoothMesh.GenerateFromOctree(octree, meshMin, meshMax, resolution);
    smoothMesh.UpdateRaylibMesh();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    lastGenerationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    lastVertexCount = smoothMesh.GetVertexCount();
    lastTriangleCount = smoothMesh.GetTriangleCount();
}

void EnhancedChunkSystem::GenerateHybridMesh() {
    // Generate both traditional and smooth meshes, then decide which parts to use
    GenerateTraditionalMesh();
    
    // Only generate smooth mesh for natural terrain areas
    if (ShouldUseSmoothTerrain()) {
        GenerateSmoothMesh();
    }
}

void EnhancedChunkSystem::UpdateMesh(float playerDistance) {
    if (!meshDirty.load()) return;
    
    // Update LOD based on distance
    UpdateLOD(playerDistance);
    
    // Optimize for distance
    OptimizeForDistance(playerDistance);
    
    // Generate mesh based on render mode
    switch (renderMode) {
        case ChunkRenderMode::TRADITIONAL_BLOCKS:
            GenerateTraditionalMesh();
            break;
        case ChunkRenderMode::MARCHING_CUBES:
            GenerateSmoothMesh();
            break;
        case ChunkRenderMode::HYBRID:
            GenerateHybridMesh();
            break;
        case ChunkRenderMode::ADAPTIVE:
            if (ShouldUseSmoothTerrain() && playerDistance < 64.0f) {
                GenerateSmoothMesh();
            } else {
                GenerateTraditionalMesh();
            }
            break;
    }
    
    meshDirty.store(false);
    
    // Optimize octree after mesh generation
    octree.Optimize();
}

void EnhancedChunkSystem::UpdateLOD(float distanceToPlayer) {
    ChunkLOD newLOD = ChunkLOD::HIGH;
    
    if (distanceToPlayer > 128.0f) {
        newLOD = ChunkLOD::VERY_LOW;
    } else if (distanceToPlayer > 64.0f) {
        newLOD = ChunkLOD::LOW;
    } else if (distanceToPlayer > 32.0f) {
        newLOD = ChunkLOD::MEDIUM;
    }
    
    if (newLOD != currentLOD) {
        currentLOD = newLOD;
        meshDirty.store(true);
    }
}

void EnhancedChunkSystem::OptimizeForDistance(float distance) {
    // Adjust octree parameters for distant chunks
    if (distance > 64.0f) {
        // For distant chunks, we can use lower resolution
        // This would involve reducing the effective octree depth
    }
}

void EnhancedChunkSystem::Render(TextureManager& textureManager, const Vector3& cameraPosition) const {
    if (!isLoaded.load()) return;
    
    switch (renderMode) {
        case ChunkRenderMode::TRADITIONAL_BLOCKS:
        case ChunkRenderMode::ADAPTIVE:
            // Render traditional quads if available
            for (const auto& quad : traditionalQuads) {
                RenderQuad(quad, textureManager);
            }
            break;
            
        case ChunkRenderMode::MARCHING_CUBES:
            // Render smooth mesh
            if (!smoothMesh.IsEmpty()) {
                Texture2D grassTexture = textureManager.GetBlockTexture(BlockType::GRASS, BlockFace::ALL);
                if (grassTexture.id > 0) {
                    smoothMesh.RenderWithTexture({0, 0, 0}, grassTexture);
                } else {
                    smoothMesh.Render({0, 0, 0}, GREEN);
                }
            }
            break;
            
        case ChunkRenderMode::HYBRID:
            // Render both based on content
            if (!smoothMesh.IsEmpty() && ShouldUseSmoothTerrain()) {
                Texture2D grassTexture = textureManager.GetBlockTexture(BlockType::GRASS, BlockFace::ALL);
                smoothMesh.RenderWithTexture({0, 0, 0}, grassTexture);
            } else {
                for (const auto& quad : traditionalQuads) {
                    RenderQuad(quad, textureManager);
                }
            }
            break;
    }
}

void EnhancedChunkSystem::RenderWireframe() const {
    // Render octree structure for debugging
    octree.DebugDraw();
}

void EnhancedChunkSystem::FromTraditionalChunk(const Chunk& traditionalChunk) {
    octree.FromChunk(traditionalChunk, worldOrigin);
    isGenerated.store(true);
    MarkDirty();
}

void EnhancedChunkSystem::ToTraditionalChunk(Chunk& traditionalChunk) const {
    octree.ToChunk(traditionalChunk, worldOrigin);
}

// Placeholder for the missing method - would need to implement octree-based quad generation
void EnhancedChunkSystem::GenerateQuadsForFaceFromOctree(BlockFace face, const Chunk* neighbors[4]) {
    // This would be similar to your existing GenerateQuadsForFace but working with octree data
    // For now, fall back to converting to traditional format temporarily
    Chunk tempChunk(coord);
    octree.ToChunk(tempChunk, worldOrigin);
    
    // Use existing greedy meshing logic
    // TODO: Implement direct octree-to-quad generation for better performance
}

// Helper function to render a quad (similar to your existing RenderQuad)
void EnhancedChunkSystem::RenderQuad(const QuadMesh& quad, TextureManager& textureManager) const {
    // This would be the same as your existing RenderQuad implementation
    // Copy from Chunk.cpp's RenderQuad method
    Texture2D texture = textureManager.GetBlockTexture(quad.blockType, quad.face);
    
    if (texture.id == 0) {
        // Fallback to colored cube
        Color blockColor = MAGENTA;
        switch (quad.blockType) {
            case BlockType::DIRT: blockColor = BROWN; break;
            case BlockType::GRASS: blockColor = GREEN; break;
            case BlockType::STONE: blockColor = GRAY; break;
            case BlockType::WOOD: blockColor = MAROON; break;
            case BlockType::COBBLESTONE: blockColor = DARKGRAY; break;
            default: blockColor = MAGENTA; break;
        }
        DrawCube(quad.position, quad.size.x, quad.size.y, quad.size.z, blockColor);
        return;
    }
    
    // Render textured quad (implementation would be same as existing RenderQuad)
    // ... (copy the rendering code from Chunk.cpp)
}

// OptimizedChunkManager implementation
OptimizedChunkManager::OptimizedChunkManager() 
    : lastFrameTime(16.6f), chunksUpdatedThisFrame(0) {
}

OptimizedChunkManager::~OptimizedChunkManager() {
    shouldStop.store(true);
}

void OptimizedChunkManager::SetPerformanceSettings(const PerformanceSettings& newSettings) {
    settings = newSettings;
}

size_t OptimizedChunkManager::GetTotalVertexCount() const {
    size_t total = 0;
    for (const auto& pair : chunks) {
        total += pair.second->GetVertexCount();
    }
    return total;
}

size_t OptimizedChunkManager::GetTotalTriangleCount() const {
    size_t total = 0;
    for (const auto& pair : chunks) {
        total += pair.second->GetTriangleCount();
    }
    return total;
}

void OptimizedChunkManager::LoadChunk(ChunkCoord coord) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    auto key = ChunkCoordToKey(coord);
    if (chunks.find(key) != chunks.end()) {
        return; // Already loaded
    }
    
    auto chunk = std::make_unique<EnhancedChunkSystem>(coord);
    
    // Generate terrain for the chunk
    chunk->GenerateTerrain(12345, 0.02f); // Use your existing world generation parameters
    
    chunks[key] = std::move(chunk);
    ScheduleChunkUpdate(coord);
}

void OptimizedChunkManager::UpdatePlayerPosition(const Vector3& playerPos, const Vector3& playerVelocity) {
    // Update chunks around player with predictive loading
    int playerChunkX = static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(std::floor(playerPos.z / CHUNK_SIZE));
    
    // Load chunks in a radius around player
    LoadChunksAroundPlayer(playerPos, 4);
    
    // Unload distant chunks
    UnloadDistantChunks(playerPos, 8);
    
    // Update LOD for all loaded chunks
    for (auto& pair : chunks) {
        Vector3 chunkCenter = pair.second->GetWorldOrigin();
        chunkCenter.x += CHUNK_SIZE * 0.5f;
        chunkCenter.z += CHUNK_SIZE * 0.5f;
        
        float distance = CalculateDistance(playerPos, chunkCenter);
        pair.second->UpdateMesh(distance);
    }
}

void OptimizedChunkManager::Update(float deltaTime, const Vector3& playerPos) {
    lastFrameTime = deltaTime * 1000.0f; // Convert to milliseconds
    chunksUpdatedThisFrame = 0;
    
    // Process chunk updates
    while (!updateQueue.empty() && chunksUpdatedThisFrame < settings.maxChunksPerFrame) {
        ChunkCoord coord = updateQueue.front();
        updateQueue.pop();
        
        auto key = ChunkCoordToKey(coord);
        auto it = chunks.find(key);
        if (it != chunks.end()) {
            Vector3 chunkCenter = it->second->GetWorldOrigin();
            chunkCenter.x += CHUNK_SIZE * 0.5f;
            chunkCenter.z += CHUNK_SIZE * 0.5f;
            
            float distance = CalculateDistance(playerPos, chunkCenter);
            it->second->UpdateMesh(distance);
            
            chunksUpdatedThisFrame++;
        }
        
        updatingChunks.erase(std::remove(updatingChunks.begin(), updatingChunks.end(), coord), updatingChunks.end());
    }
    
    // Adaptive quality adjustment
    if (settings.adaptiveQuality) {
        AdaptQualityForPerformance();
    }
}

void OptimizedChunkManager::AdaptQualityForPerformance() {
    if (lastFrameTime > settings.targetFrameTime * 1.2f) {
        // Performance is poor, reduce quality
        settings.maxSmoothTerrainDistance *= 0.9f;
        settings.maxChunksPerFrame = std::max(1, settings.maxChunksPerFrame - 1);
    } else if (lastFrameTime < settings.targetFrameTime * 0.8f) {
        // Performance is good, increase quality
        settings.maxSmoothTerrainDistance *= 1.1f;
        settings.maxChunksPerFrame = std::min(4, settings.maxChunksPerFrame + 1);
    }
}

void OptimizedChunkManager::RenderChunks(const Vector3& playerPos, TextureManager& textureManager) {
    for (const auto& pair : chunks) {
        Vector3 chunkCenter = pair.second->GetWorldOrigin();
        chunkCenter.x += CHUNK_SIZE * 0.5f;
        chunkCenter.z += CHUNK_SIZE * 0.5f;
        
        float distance = CalculateDistance(playerPos, chunkCenter);
        
        // Only render chunks within render distance
        if (distance <= settings.lodDistance[3]) {
            pair.second->Render(textureManager, playerPos);
        }
    }
}

void OptimizedChunkManager::LoadChunksAroundPlayer(const Vector3& playerPos, int radius) {
    int playerChunkX = static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(std::floor(playerPos.z / CHUNK_SIZE));
    
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            ChunkCoord coord(playerChunkX + dx, playerChunkZ + dz);
            LoadChunk(coord);
        }
    }
}

void OptimizedChunkManager::UnloadDistantChunks(const Vector3& playerPos, int maxDistance) {
    std::vector<ChunkCoord> toUnload;
    
    for (const auto& pair : chunks) {
        Vector3 chunkCenter = pair.second->GetWorldOrigin();
        chunkCenter.x += CHUNK_SIZE * 0.5f;
        chunkCenter.z += CHUNK_SIZE * 0.5f;
        
        float distance = CalculateDistance(playerPos, chunkCenter);
        if (distance > maxDistance * CHUNK_SIZE) {
            // Convert map key back to ChunkCoord
            ChunkCoord coord = {pair.first.first, pair.first.second};
            toUnload.push_back(coord);
        }
    }
    
    for (ChunkCoord coord : toUnload) {
        UnloadChunk(coord);
    }
}

void OptimizedChunkManager::UnloadChunk(ChunkCoord coord) {
    std::lock_guard<std::mutex> lock(chunkMutex);
    auto key = ChunkCoordToKey(coord);
    chunks.erase(key);
}

void OptimizedChunkManager::ScheduleChunkUpdate(ChunkCoord coord) {
    if (std::find(updatingChunks.begin(), updatingChunks.end(), coord) == updatingChunks.end()) {
        updateQueue.push(coord);
        updatingChunks.push_back(coord);
    }
}

size_t OptimizedChunkManager::GetLoadedChunkCount() const {
    return chunks.size();
}

size_t OptimizedChunkManager::GetTotalMemoryUsage() const {
    size_t total = 0;
    for (const auto& pair : chunks) {
        total += pair.second->GetMemoryUsage();
    }
    return total;
}
