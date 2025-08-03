#include "BlockInteraction.h"
#include "PhysicsPlayer.h"
#include "rlgl.h"
#include <cmath>
#include <iostream>

// Global instance
BlockInteraction* g_blockInteraction = nullptr;

BlockInteraction::BlockInteraction(ChunkManager* cm) 
    : chunkManager(cm), blockReachDistance(5.0f), selectedBlockType(BlockType::STONE) {
}

void BlockInteraction::Initialize() {
    std::cout << "BlockInteraction initialized with reach distance: " << blockReachDistance << std::endl;
}

void BlockInteraction::Update(float deltaTime, const Camera3D& camera) {
    // Handle block selection (1-9 keys for different block types)
    HandleBlockSelection();
    
    // Handle mouse input
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        HandleLeftClick(camera);
    }
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        HandleRightClick(camera);
    }
    
    // Handle continuous left mouse hold for breaking
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        HandleLeftMouseHold(deltaTime, camera);
    } else {
        // Stop breaking if not holding left mouse
        if (breakingInfo.isBreaking) {
            StopBreaking();
        }
    }
}

BlockHitInfo BlockInteraction::RaycastBlocks(const Camera3D& camera, float maxDistance) const {
    BlockHitInfo result;
    
    if (!chunkManager) {
        return result;
    }
    
    // Calculate ray direction from camera
    Vector3 rayOrigin = camera.position;
    Vector3 rayDirection = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    
    float closestDistance = maxDistance;
    bool hitFound = false;
    
    // Step through the ray using DDA-like algorithm
    float stepSize = 0.1f; // Small step size for accuracy
    int maxSteps = static_cast<int>(maxDistance / stepSize);
    
    for (int i = 0; i < maxSteps; i++) {
        float distance = i * stepSize;
        Vector3 currentPos = Vector3Add(rayOrigin, Vector3Scale(rayDirection, distance));
        
        // Get block at current position
        BlockType blockType = chunkManager->GetBlock(currentPos.x, currentPos.y, currentPos.z);
        
        // Skip air blocks
        if (blockType == BlockType::AIR) {
            continue;
        }
        
        // We hit a solid block
        Vector3 blockPos = {
            floorf(currentPos.x),
            floorf(currentPos.y),
            floorf(currentPos.z)
        };
        
        // More precise intersection test - use exact same coordinates as vertex rendering
        // From Chunk.cpp: vertices range from (x, y, z) to (x+1, y+1, z+1)
        float x = blockPos.x;
        float y = blockPos.y;
        float z = blockPos.z;
        Vector3 blockMin = {x, y, z};
        Vector3 blockMax = {x + 1.0f, y + 1.0f, z + 1.0f};
        
        float exactDistance;
        Vector3 normal;
        if (IntersectRayAABB(rayOrigin, rayDirection, blockMin, blockMax, exactDistance, normal)) {
            if (exactDistance < closestDistance) {
                result.hit = true;
                result.blockPosition = blockPos;
                result.hitPoint = Vector3Add(rayOrigin, Vector3Scale(rayDirection, exactDistance));
                result.normal = normal;
                result.blockType = blockType;
                result.distance = exactDistance;
                closestDistance = exactDistance;
                hitFound = true;
                break; // Take the first hit we find
            }
        }
    }
    
    return result;
}

void BlockInteraction::HandleLeftClick(const Camera3D& camera) {
    BlockHitInfo hitInfo = RaycastBlocks(camera, blockReachDistance);
    
    if (hitInfo.hit && hitInfo.blockType != BlockType::AIR) {
        // Start breaking the block
        breakingInfo.blockPosition = hitInfo.blockPosition;
        breakingInfo.breakingProgress = 0.0f;
        breakingInfo.breakStartTime = GetTime();
        breakingInfo.isBreaking = true;
        
        std::cout << "Started breaking block at (" << hitInfo.blockPosition.x << ", " 
                  << hitInfo.blockPosition.y << ", " << hitInfo.blockPosition.z << ")" << std::endl;
    }
}

void BlockInteraction::HandleRightClick(const Camera3D& camera) {
    BlockHitInfo hitInfo = RaycastBlocks(camera, blockReachDistance);
    
    if (hitInfo.hit) {
        // Calculate placement position (adjacent to hit block)
        Vector3 placementPos = Vector3Add(hitInfo.blockPosition, hitInfo.normal);
        
        // Check if placement position is valid
        if (IsValidPlacementPosition(placementPos)) {
            // Check if player would intersect with the new block
            Vector3 playerFeet = g_player.position;
            Vector3 playerHead = Vector3Add(playerFeet, {0, 1.8f, 0}); // Player is ~1.8 blocks tall
            
            // Simple player collision check
            bool wouldIntersectPlayer = false;
            if (placementPos.x <= playerFeet.x + 0.3f && placementPos.x >= playerFeet.x - 0.3f &&
                placementPos.z <= playerFeet.z + 0.3f && placementPos.z >= playerFeet.z - 0.3f &&
                placementPos.y <= playerHead.y && placementPos.y >= playerFeet.y) {
                wouldIntersectPlayer = true;
            }
            
            if (!wouldIntersectPlayer) {
                // Place the block
                chunkManager->SetBlock(placementPos.x, placementPos.y, placementPos.z, selectedBlockType);
                std::cout << "Placed block type " << static_cast<int>(selectedBlockType) 
                          << " at (" << placementPos.x << ", " << placementPos.y << ", " << placementPos.z << ")" << std::endl;
            } else {
                std::cout << "Cannot place block: would intersect with player" << std::endl;
            }
        }
    }
}

void BlockInteraction::HandleLeftMouseHold(float deltaTime, const Camera3D& camera) {
    if (!breakingInfo.isBreaking) {
        return;
    }
    
    BlockHitInfo hitInfo = RaycastBlocks(camera, blockReachDistance);
    
    // Check if we're still looking at the same block
    if (!hitInfo.hit || 
        hitInfo.blockPosition.x != breakingInfo.blockPosition.x ||
        hitInfo.blockPosition.y != breakingInfo.blockPosition.y ||
        hitInfo.blockPosition.z != breakingInfo.blockPosition.z) {
        // Player looked away from the block being broken
        StopBreaking();
        return;
    }
    
    // Update breaking progress
    float breakTime = GetBlockBreakTime(hitInfo.blockType);
    float elapsedTime = GetTime() - breakingInfo.breakStartTime;
    breakingInfo.breakingProgress = elapsedTime / breakTime;
    
    // Check if block is fully broken
    if (breakingInfo.breakingProgress >= 1.0f) {
        // Break the block
        chunkManager->SetBlock(breakingInfo.blockPosition.x, breakingInfo.blockPosition.y, 
                              breakingInfo.blockPosition.z, BlockType::AIR);
        
        std::cout << "Broke block at (" << breakingInfo.blockPosition.x << ", " 
                  << breakingInfo.blockPosition.y << ", " << breakingInfo.blockPosition.z << ")" << std::endl;
        
        StopBreaking();
    }
}

void BlockInteraction::StopBreaking() {
    breakingInfo.isBreaking = false;
    breakingInfo.breakingProgress = 0.0f;
}

float BlockInteraction::GetBlockBreakTime(BlockType blockType) const {
    // Basic breaking times (in seconds) - can be expanded
    switch (blockType) {
        case BlockType::STONE:
        case BlockType::COBBLESTONE:
            return 1.5f;
        case BlockType::DIRT:
        case BlockType::GRASS:
        case BlockType::SAND:
            return 0.5f;
        case BlockType::WOOD:
        case BlockType::PLANKS:
            return 1.0f;
        case BlockType::IRON_ORE:
        case BlockType::COAL_ORE:
            return 3.0f;
        case BlockType::DIAMOND_ORE:
            return 5.0f;
        case BlockType::OBSIDIAN:
            return 15.0f;
        case BlockType::BEDROCK:
            return 999999.0f; // Effectively unbreakable
        default:
            return 1.0f;
    }
}

void BlockInteraction::DrawUI(const Camera3D& camera) {
    // Draw crosshair in center of screen
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;
    
    // Simple crosshair
    DrawLine(centerX - CROSSHAIR_SIZE, centerY, centerX + CROSSHAIR_SIZE, centerY, WHITE);
    DrawLine(centerX, centerY - CROSSHAIR_SIZE, centerX, centerY + CROSSHAIR_SIZE, WHITE);
    
    // Get block we're looking at
    BlockHitInfo hitInfo = RaycastBlocks(camera, blockReachDistance);
    
    if (hitInfo.hit) {
        // Draw block outline
        DrawBlockOutline(hitInfo.blockPosition);
        
        // Draw breaking progress if breaking
        if (breakingInfo.isBreaking && 
            (breakingInfo.blockPosition.x == hitInfo.blockPosition.x &&
             breakingInfo.blockPosition.y == hitInfo.blockPosition.y &&
             breakingInfo.blockPosition.z == hitInfo.blockPosition.z)) {
            DrawBreakingProgress(hitInfo.blockPosition, breakingInfo.breakingProgress);
        }
        
        // Show block info
        const char* blockName = "Unknown";
        switch (hitInfo.blockType) {
            case BlockType::STONE: blockName = "Stone"; break;
            case BlockType::DIRT: blockName = "Dirt"; break;
            case BlockType::GRASS: blockName = "Grass"; break;
            case BlockType::WOOD: blockName = "Wood"; break;
            case BlockType::COBBLESTONE: blockName = "Cobblestone"; break;
            case BlockType::SAND: blockName = "Sand"; break;
            // Add more as needed
            default: blockName = "Block"; break;
        }
        
        char blockInfo[128];
        snprintf(blockInfo, sizeof(blockInfo), "Looking at: %s (%.1f, %.1f, %.1f)", 
                blockName, hitInfo.blockPosition.x, hitInfo.blockPosition.y, hitInfo.blockPosition.z);
        DrawText(blockInfo, 10, screenHeight - 50, 16, WHITE);
    }
    
    // Show selected block type
    const char* selectedName = "Unknown";
    switch (selectedBlockType) {
        case BlockType::STONE: selectedName = "Stone"; break;
        case BlockType::DIRT: selectedName = "Dirt"; break;
        case BlockType::GRASS: selectedName = "Grass"; break;
        case BlockType::WOOD: selectedName = "Wood"; break;
        case BlockType::COBBLESTONE: selectedName = "Cobblestone"; break;
        case BlockType::SAND: selectedName = "Sand"; break;
        case BlockType::PLANKS: selectedName = "Planks"; break;
        case BlockType::BRICK: selectedName = "Brick"; break;
        case BlockType::GLASS: selectedName = "Glass"; break;
        default: selectedName = "Block"; break;
    }
    
    char selectedInfo[128];
    snprintf(selectedInfo, sizeof(selectedInfo), "Selected: %s (Keys 1-9 to change)", selectedName);
    DrawText(selectedInfo, 10, screenHeight - 30, 16, YELLOW);
}

void BlockInteraction::DrawBlockOutline(const Vector3& blockPos, Color color) const {
    // Draw wireframe cube around the block - use same coordinates as vertex rendering
    // Vertices span from (x,y,z) to (x+1,y+1,z+1), so center at (x+0.5,y+0.5,z+0.5)
    float x = blockPos.x;
    float y = blockPos.y;
    float z = blockPos.z;
    Vector3 position = {x + 0.5f, y + 0.5f, z + 0.5f};
    Vector3 size = {1.0f, 1.0f, 1.0f};
    
    DrawCubeWires(position, size.x, size.y, size.z, color);
}

void BlockInteraction::DrawBreakingProgress(const Vector3& blockPos, float progress) const {
    if (progress <= 0.0f || progress >= 1.0f) return;
    
    // Draw breaking overlay with transparency
    Color breakColor = {255, 255, 255, (unsigned char)(progress * 128)};
    
    // Slightly larger cube to show breaking effect - use same coordinates as vertex rendering
    // Vertices span from (x,y,z) to (x+1,y+1,z+1), so center at (x+0.5,y+0.5,z+0.5)
    float x = blockPos.x;
    float y = blockPos.y;
    float z = blockPos.z;
    Vector3 position = {x + 0.5f, y + 0.5f, z + 0.5f};
    Vector3 size = Vector3Scale(Vector3One(), BREAKING_ANIMATION_SCALE);
    
    // Draw semi-transparent overlay
    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlScalef(size.x, size.y, size.z);
    
    // Draw semi-transparent overlay for breaking effect
    DrawCube({0, 0, 0}, 1.0f, 1.0f, 1.0f, breakColor);
    
    rlPopMatrix();
    
    // Also draw progress-based wireframe
    Color progressColor = {255, (unsigned char)(255 - progress * 255), 0, 255}; // Red to yellow
    DrawCubeWires(position, size.x, size.y, size.z, progressColor);
}

void BlockInteraction::HandleBlockSelection() {
    // Handle 1-9 keys for block selection
    if (IsKeyPressed(KEY_ONE)) selectedBlockType = BlockType::STONE;
    else if (IsKeyPressed(KEY_TWO)) selectedBlockType = BlockType::DIRT;
    else if (IsKeyPressed(KEY_THREE)) selectedBlockType = BlockType::GRASS;
    else if (IsKeyPressed(KEY_FOUR)) selectedBlockType = BlockType::WOOD;
    else if (IsKeyPressed(KEY_FIVE)) selectedBlockType = BlockType::COBBLESTONE;
    else if (IsKeyPressed(KEY_SIX)) selectedBlockType = BlockType::SAND;
    else if (IsKeyPressed(KEY_SEVEN)) selectedBlockType = BlockType::PLANKS;
    else if (IsKeyPressed(KEY_EIGHT)) selectedBlockType = BlockType::BRICK;
    else if (IsKeyPressed(KEY_NINE)) selectedBlockType = BlockType::GLASS;
}

bool BlockInteraction::IntersectRayAABB(const Vector3& rayOrigin, const Vector3& rayDir, 
                                       const Vector3& boxMin, const Vector3& boxMax, 
                                       float& distance, Vector3& normal) const {
    Vector3 invDir = {
        1.0f / rayDir.x,
        1.0f / rayDir.y,
        1.0f / rayDir.z
    };
    
    // Manual component-wise multiplication since Vector3Multiply might not exist in this version
    Vector3 diff1 = Vector3Subtract(boxMin, rayOrigin);
    Vector3 diff2 = Vector3Subtract(boxMax, rayOrigin);
    Vector3 t1 = {diff1.x * invDir.x, diff1.y * invDir.y, diff1.z * invDir.z};
    Vector3 t2 = {diff2.x * invDir.x, diff2.y * invDir.y, diff2.z * invDir.z};
    
    Vector3 tMin = {
        fminf(t1.x, t2.x),
        fminf(t1.y, t2.y),
        fminf(t1.z, t2.z)
    };
    
    Vector3 tMax = {
        fmaxf(t1.x, t2.x),
        fmaxf(t1.y, t2.y),
        fmaxf(t1.z, t2.z)
    };
    
    float tNear = fmaxf(fmaxf(tMin.x, tMin.y), tMin.z);
    float tFar = fminf(fminf(tMax.x, tMax.y), tMax.z);
    
    if (tNear > tFar || tFar < 0.0f) {
        return false; // No intersection
    }
    
    distance = tNear > 0.0f ? tNear : tFar;
    
    // Calculate normal of hit face
    Vector3 hitPoint = Vector3Add(rayOrigin, Vector3Scale(rayDir, distance));
    Vector3 blockCenter = Vector3Scale(Vector3Add(boxMin, boxMax), 0.5f);
    normal = GetFaceNormal(hitPoint, blockCenter);
    
    return true;
}

bool BlockInteraction::IsValidPlacementPosition(const Vector3& position) const {
    if (!chunkManager) return false;
    
    // Check if the position is not already occupied
    BlockType existingBlock = chunkManager->GetBlock(position.x, position.y, position.z);
    return existingBlock == BlockType::AIR;
}

Vector3 BlockInteraction::GetFaceNormal(const Vector3& hitPoint, const Vector3& blockCenter) const {
    Vector3 diff = Vector3Subtract(hitPoint, blockCenter);
    Vector3 absDiff = {fabsf(diff.x), fabsf(diff.y), fabsf(diff.z)};
    
    // Find the axis with the largest difference
    if (absDiff.x > absDiff.y && absDiff.x > absDiff.z) {
        return {diff.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f};
    } else if (absDiff.y > absDiff.z) {
        return {0.0f, diff.y > 0 ? 1.0f : -1.0f, 0.0f};
    } else {
        return {0.0f, 0.0f, diff.z > 0 ? 1.0f : -1.0f};
    }
}
