#pragma once

#include "raylib.h"
#include "raymath.h"
#include "ChunkManager.h"
#include "BlockDictionary.h"
#include <optional>

// Structure to hold raycast hit information
struct BlockHitInfo {
    Vector3 hitPoint;        // World position where ray hit the block
    Vector3 blockPosition;   // Integer world position of the hit block
    Vector3 normal;          // Surface normal of the hit face
    BlockType blockType;     // Type of block that was hit
    bool hit;                // Whether we hit a block
    float distance;          // Distance from ray origin to hit point
    
    BlockHitInfo() : hit(false), distance(0.0f), blockType(BlockType::AIR) {}
};

// Structure to manage block breaking progress
struct BlockBreakingInfo {
    Vector3 blockPosition;   // Position of block being broken
    float breakingProgress;  // 0.0 to 1.0 progress
    float breakStartTime;    // When breaking started
    bool isBreaking;         // Whether currently breaking a block
    
    BlockBreakingInfo() : breakingProgress(0.0f), breakStartTime(0.0f), isBreaking(false) {}
};

class BlockInteraction {
private:
    ChunkManager* chunkManager;
    BlockBreakingInfo breakingInfo;
    float blockReachDistance;
    
    // UI constants
    static constexpr float CROSSHAIR_SIZE = 10.0f;
    static constexpr float BREAKING_ANIMATION_SCALE = 1.02f; // Slightly larger overlay for breaking effect

public:
    BlockInteraction(ChunkManager* cm);
    ~BlockInteraction() = default;
    
    // Initialize block interaction system
    void Initialize();
    
    // Update block interaction (handle mouse input)
    void Update(float deltaTime, const Camera3D& camera);
    
    // Raycast from camera to find block intersection
    BlockHitInfo RaycastBlocks(const Camera3D& camera, float maxDistance) const;
    
    // Handle left mouse click (block breaking)
    void HandleLeftClick(const Camera3D& camera);
    
    // Handle right mouse click (block placing)
    void HandleRightClick(const Camera3D& camera);
    
    // Handle continuous left mouse hold (breaking progress)
    void HandleLeftMouseHold(float deltaTime, const Camera3D& camera);
    
    // Stop breaking current block
    void StopBreaking();
    
    // Get breaking time for a block type
    float GetBlockBreakTime(BlockType blockType) const;
    
    // Draw UI elements (crosshair, block outline, breaking progress)
    void DrawUI(const Camera3D& camera);
    
    // Draw block outline for targeted block
    void DrawBlockOutline(const Vector3& blockPos, Color color = WHITE) const;
    
    // Draw breaking progress overlay
    void DrawBreakingProgress(const Vector3& blockPos, float progress) const;
    
    // Get currently selected block type for placing
    BlockType GetSelectedBlockType() const { return selectedBlockType; }
    
    // Set selected block type
    void SetSelectedBlockType(BlockType blockType) { selectedBlockType = blockType; }
    
    // Handle hotbar/inventory selection
    void HandleBlockSelection();

private:
    BlockType selectedBlockType;
    
    // Helper functions for raycasting
    bool IntersectRayAABB(const Vector3& rayOrigin, const Vector3& rayDir, 
                         const Vector3& boxMin, const Vector3& boxMax, 
                         float& distance, Vector3& normal) const;
    
    // Check if a position is valid for block placement
    bool IsValidPlacementPosition(const Vector3& position) const;
    
    // Get face normal from hit information
    Vector3 GetFaceNormal(const Vector3& hitPoint, const Vector3& blockCenter) const;
};

// Global block interaction instance
extern BlockInteraction* g_blockInteraction;
