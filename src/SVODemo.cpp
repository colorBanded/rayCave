#include "SparseVoxelOctree.h"
#include "MarchingCubes.h"
#include "Chunk.h"
#include "raylib.h"
#include <iostream>

// Example usage of Sparse Voxel Octree and Marching Cubes
void DemoSVOAndMarchingCubes() {
    std::cout << "=== Sparse Voxel Octree & Marching Cubes Demo ===" << std::endl;
    
    // Create a sparse voxel octree
    Vector3 origin = {-64.0f, -64.0f, -64.0f};
    float size = 128.0f;
    uint8_t maxDepth = 6;
    
    SparseVoxelOctree octree(origin, size, maxDepth);
    
    // Add some blocks to create interesting geometry
    std::cout << "Adding blocks to octree..." << std::endl;
    
    // Create a sphere of blocks
    Vector3 sphereCenter = {0.0f, 0.0f, 0.0f};
    float radius = 8.0f;
    
    for (int x = -10; x <= 10; x++) {
        for (int y = -10; y <= 10; y++) {
            for (int z = -10; z <= 10; z++) {
                Vector3 pos = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                float distance = std::sqrt(
                    (pos.x - sphereCenter.x) * (pos.x - sphereCenter.x) +
                    (pos.y - sphereCenter.y) * (pos.y - sphereCenter.y) +
                    (pos.z - sphereCenter.z) * (pos.z - sphereCenter.z)
                );
                
                if (distance <= radius) {
                    BlockType blockType = (distance < radius * 0.5f) ? BlockType::STONE : BlockType::DIRT;
                    octree.SetBlock(pos, blockType);
                }
            }
        }
    }
    
    std::cout << "Octree memory usage: " << octree.GetMemoryUsage() << " bytes" << std::endl;
    
    // Optimize the octree
    octree.Optimize();
    std::cout << "Octree memory usage after optimization: " << octree.GetMemoryUsage() << " bytes" << std::endl;
    
    // Generate marching cubes mesh
    std::cout << "Generating marching cubes mesh..." << std::endl;
    
    MarchingCubesMesh mesh;
    Vector3 meshMin = {-12.0f, -12.0f, -12.0f};
    Vector3 meshMax = {12.0f, 12.0f, 12.0f};
    
    mesh.GenerateFromOctree(octree, meshMin, meshMax, 1.0f);
    mesh.UpdateRaylibMesh();
    
    std::cout << "Generated mesh with " << mesh.GetVertexCount() << " vertices and " 
              << mesh.GetTriangleCount() << " triangles" << std::endl;
    std::cout << "Mesh memory usage: " << mesh.GetMemoryUsage() << " bytes" << std::endl;
    
    // Test integration with traditional chunk system
    std::cout << "Testing integration with traditional chunk system..." << std::endl;
    
    // Create a traditional chunk
    ChunkCoord coord(0, 0);
    Chunk traditionalChunk(coord);
    
    // Fill it with some test data
    for (int x = 0; x < 8; x++) {
        for (int z = 0; z < 8; z++) {
            for (int y = 0; y < 16; y++) {
                BlockType blockType = (y < 8) ? BlockType::STONE : 
                                     (y < 12) ? BlockType::DIRT : BlockType::GRASS;
                traditionalChunk.SetBlock(x, y, z, blockType);
            }
        }
    }
    
    // Convert to octree
    SparseVoxelOctree chunkOctree({0.0f, 0.0f, 0.0f}, 32.0f, 5);
    chunkOctree.FromChunk(traditionalChunk, {0.0f, 0.0f, 0.0f});
    
    std::cout << "Converted chunk to octree. Memory usage: " << chunkOctree.GetMemoryUsage() << " bytes" << std::endl;
    
    // Generate mesh for the chunk
    MarchingCubesMesh chunkMesh;
    chunkMesh.GenerateFromOctree(chunkOctree, {0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}, 1.0f);
    chunkMesh.UpdateRaylibMesh();
    
    std::cout << "Chunk mesh: " << chunkMesh.GetVertexCount() << " vertices, " 
              << chunkMesh.GetTriangleCount() << " triangles" << std::endl;
              
    std::cout << "=== Demo Complete ===" << std::endl;
}

// Enhanced chunk example
class SVOChunkManager {
private:
    std::unordered_map<uint64_t, std::unique_ptr<EnhancedChunk>> chunks;
    
    uint64_t ChunkKey(int x, int z) {
        return (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(z);
    }
    
public:
    void LoadChunk(int chunkX, int chunkZ) {
        uint64_t key = ChunkKey(chunkX, chunkZ);
        if (chunks.find(key) != chunks.end()) return; // Already loaded
        
        Vector3 origin = {
            static_cast<float>(chunkX * 16),
            0.0f,
            static_cast<float>(chunkZ * 16)
        };
        
        auto chunk = std::make_unique<EnhancedChunk>(origin, 16.0f);
        
        // Generate some test terrain
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                float height = 32.0f + 16.0f * sin(x * 0.2f) * cos(z * 0.2f);
                
                for (int y = 0; y < static_cast<int>(height) && y < 256; y++) {
                    BlockType blockType = (y < height - 4) ? BlockType::STONE :
                                         (y < height - 1) ? BlockType::DIRT : BlockType::GRASS;
                    chunk->SetBlock({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}, blockType);
                }
            }
        }
        
        // Update mesh
        chunk->UpdateMesh(1.0f);
        
        chunks[key] = std::move(chunk);
    }
    
    void RenderChunks(const Vector3& playerPos) {
        int playerChunkX = static_cast<int>(playerPos.x / 16.0f);
        int playerChunkZ = static_cast<int>(playerPos.z / 16.0f);
        
        // Render chunks around player
        for (int dx = -2; dx <= 2; dx++) {
            for (int dz = -2; dz <= 2; dz++) {
                int chunkX = playerChunkX + dx;
                int chunkZ = playerChunkZ + dz;
                uint64_t key = ChunkKey(chunkX, chunkZ);
                
                auto it = chunks.find(key);
                if (it != chunks.end()) {
                    it->second->Render(playerPos);
                }
            }
        }
    }
    
    void LoadChunksAroundPlayer(const Vector3& playerPos) {
        int playerChunkX = static_cast<int>(playerPos.x / 16.0f);
        int playerChunkZ = static_cast<int>(playerPos.z / 16.0f);
        
        // Load chunks in a 5x5 area around player
        for (int dx = -2; dx <= 2; dx++) {
            for (int dz = -2; dz <= 2; dz++) {
                LoadChunk(playerChunkX + dx, playerChunkZ + dz);
            }
        }
    }
    
    size_t GetTotalMemoryUsage() const {
        size_t total = 0;
        for (const auto& pair : chunks) {
            total += pair.second->GetMemoryUsage();
        }
        return total;
    }
    
    size_t GetChunkCount() const {
        return chunks.size();
    }
};
