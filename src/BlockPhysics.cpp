#include "BlockPhysics.h"
#include "PhysicsPlayer.h"
#include "ChunkManager.h"
#include <cmath>

// External reference to player
extern PhysicsPlayer g_player;

// Global chunk manager reference for physics
static ChunkManager* g_chunkManager = nullptr;

void InitializeBlockPhysics(ChunkManager* chunkManager) {
    g_chunkManager = chunkManager;
}

// Check if a block is solid using the actual world data
bool IsBlockSolid(int x, int y, int z, ChunkManager* chunkManager) {
    if (!chunkManager) {
        // Fallback to simple test world if no chunk manager
        if (y < 0) return true;
        if (y == 0) return true; // Ground
        return false;
    }
    
    // Get block from the actual world
    BlockType block = chunkManager->GetBlock(static_cast<float>(x) + 0.5f, 
                                           static_cast<float>(y) + 0.5f, 
                                           static_cast<float>(z) + 0.5f);
    
    // Air blocks are not solid
    return block != BlockType::AIR;
}

// Structure to represent a plane
struct Plane {
    Vector3 normal;   // Plane normal vector
    float distance;   // Distance from origin
};

// Check if a point is in front of a plane
bool IsPointInFrontOfPlane(Vector3 point, Plane plane) {
    return (point.x * plane.normal.x + point.y * plane.normal.y + point.z * plane.normal.z) > plane.distance;
}

// Check if player's bounding box collides with a single block's faces
bool CheckBlockCollision(Vector3 position, int blockX, int blockY, int blockZ, ChunkManager* chunkManager) {
    if (!IsBlockSolid(blockX, blockY, blockZ, chunkManager)) return false;
    
    // Player bounding box corners
    float minX = position.x - PLAYER_WIDTH * 0.5f;
    float maxX = position.x + PLAYER_WIDTH * 0.5f;
    float minY = position.y;
    float maxY = position.y + PLAYER_HEIGHT;
    float minZ = position.z - PLAYER_WIDTH * 0.5f;
    float maxZ = position.z + PLAYER_WIDTH * 0.5f;
    
    // Block boundaries
    float blockMinX = (float)blockX;
    float blockMaxX = (float)blockX + 1.0f;
    float blockMinY = (float)blockY;
    float blockMaxY = (float)blockY + 1.0f;
    float blockMinZ = (float)blockZ;
    float blockMaxZ = (float)blockZ + 1.0f;
    
    // Simple AABB overlap test (this represents collision with the block's faces)
    return (maxX > blockMinX && minX < blockMaxX &&
            maxY > blockMinY && minY < blockMaxY &&
            maxZ > blockMinZ && minZ < blockMaxZ);
}

// Check if player's bounding box collides with any solid blocks using face-based collision
bool CheckCollision(Vector3 position, ChunkManager* chunkManager) {
    // Player bounding box
    float minX = position.x - PLAYER_WIDTH * 0.5f;
    float maxX = position.x + PLAYER_WIDTH * 0.5f;
    float minY = position.y;
    float maxY = position.y + PLAYER_HEIGHT;
    float minZ = position.z - PLAYER_WIDTH * 0.5f;
    float maxZ = position.z + PLAYER_WIDTH * 0.5f;
    
    // Check all blocks that the player might be intersecting
    int startX = (int)floorf(minX);
    int endX = (int)floorf(maxX);
    int startY = (int)floorf(minY);
    int endY = (int)floorf(maxY);
    int startZ = (int)floorf(minZ);
    int endZ = (int)floorf(maxZ);
    
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            for (int z = startZ; z <= endZ; z++) {
                if (CheckBlockCollision(position, x, y, z, chunkManager)) {
                    return true; // Collision detected with this block's faces
                }
            }
        }
    }
    
    return false; // No collision
}

void HandleBlockCollisions(ChunkManager* chunkManager) {
    float deltaTime = GetFrameTime();
    
    // Store original position
    Vector3 originalPos = g_player.position;
    
    // Test X movement
    Vector3 testPos = originalPos;
    testPos.x += g_player.velocity.x * deltaTime;
    if (CheckCollision(testPos, chunkManager)) {
        g_player.velocity.x = 0.0f; // Stop horizontal movement
    } else {
        g_player.position.x = testPos.x; // Apply X movement
    }
    
    // Test Z movement
    testPos = g_player.position;
    testPos.z += g_player.velocity.z * deltaTime;
    if (CheckCollision(testPos, chunkManager)) {
        g_player.velocity.z = 0.0f; // Stop horizontal movement
    } else {
        g_player.position.z = testPos.z; // Apply Z movement
    }
    
    // Test Y movement (vertical) - more precise collision resolution
    testPos = g_player.position;
    testPos.y += g_player.velocity.y * deltaTime;
    
    if (CheckCollision(testPos, chunkManager)) {
        if (g_player.velocity.y < 0.0f) {
            // Player is falling - find the exact ground level
            // Get the block they're landing on
            int groundBlockY = (int)floorf(g_player.position.y);
            
            // Find the highest solid block below the player
            for (int checkY = groundBlockY; checkY >= groundBlockY - 5; checkY--) {
                // Check around player position for solid blocks
                int playerBlockX = (int)floorf(g_player.position.x);
                int playerBlockZ = (int)floorf(g_player.position.z);
                
                // Check if there's a solid block at this level that the player is above
                if (IsBlockSolid(playerBlockX, checkY, playerBlockZ, chunkManager)) {
                    // Position player exactly on top of this block
                    g_player.position.y = (float)checkY + 1.0f;
                    g_player.onGround = true;
                    break;
                }
            }
        } else {
            // Player hit ceiling - just stop upward movement
            g_player.onGround = false;
        }
        g_player.velocity.y = 0.0f; // Stop vertical movement
    } else {
        g_player.position.y = testPos.y; // Apply Y movement
        g_player.onGround = false; // Not on ground if no collision below
    }
    
    // Final ground check - test slightly below feet to confirm ground contact
    Vector3 groundTest = g_player.position;
    groundTest.y -= 0.01f; // Test just below feet
    if (CheckCollision(groundTest, chunkManager)) {
        g_player.onGround = true;
    }
}
