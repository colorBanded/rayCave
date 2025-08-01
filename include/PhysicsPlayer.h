#pragma once

#include "raylib.h"

// Game state enumeration
enum GameState {
    GAMEPLAY,
    PAUSED
};

// Physics-based player state
struct PhysicsPlayer {
    Vector3 position;
    Vector3 velocity;    // Current velocity in m/tick
    float yaw;           // Horizontal rotation (left/right)
    float pitch;         // Vertical rotation (up/down)
    bool onGround;       // Whether player is on ground
    bool sprinting;      // Whether player is sprinting
    bool sneaking;       // Whether player is sneaking
    int jumpCooldown;    // Jump cooldown in ticks
};

// Global physics player instance
extern PhysicsPlayer g_player;

// Function Declarations
void InitPhysicsPlayer(class ChunkManager* chunkManager = nullptr);
void UpdatePhysicsPlayer(GameState gameState, float deltaTime, class ChunkManager* chunkManager = nullptr);
Vector3 CalculateInputDirection();
void ApplyHorizontalMovement(float deltaTime);
void ApplyVerticalMovement(float deltaTime);
void HandleGroundCollision();
Camera3D GetRaylibCamera();
float DeltaTimeToTicks(float deltaTime);
