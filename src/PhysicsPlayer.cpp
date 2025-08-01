#include "PhysicsPlayer.h"
#include "BlockPhysics.h"
#include "ChunkManager.h"
#include <cmath>
#include <iostream>

// Constants
constexpr float CAMERA_FOVY = 45.0f;
constexpr float MOUSE_SENSITIVITY = 0.003f;

// Authentic Minecraft physics constants (converted from ticks to seconds)
// Minecraft runs at 20 TPS (ticks per second), so 1 tick = 0.05 seconds
constexpr float GRAVITY = 32.0f;                   // 0.08 blocks/tick² * 400 (conversion factor) = 32.0 blocks/s²
constexpr float GROUND_SPEED = 4.317f;             // Normal walking speed (blocks/s) - exact Minecraft value
constexpr float AIR_SPEED = 2.0f;                  // Reduced air control speed (blocks/s)
constexpr float JUMP_VELOCITY = 8.4f;              // 0.42 blocks/tick * 20 TPS = 8.4 blocks/s
constexpr float VERTICAL_DRAG = 0.4f;              // 0.98^20 ≈ 0.67, inverted for exponential decay
constexpr float GROUND_FRICTION = 26.0f;           // 0.546^20 ≈ very small, needs high friction value
constexpr float AIR_FRICTION = 3.6f;               // 0.91^20 ≈ 0.12, moderate air control

// Player camera settings
constexpr float PLAYER_EYE_HEIGHT = 1.6f;      // Eye height for camera (blocks)

// Movement multipliers (authentic Minecraft values)
// Walking: 4.317 blocks/s, Sprinting: 5.612 blocks/s, Sneaking: ~1.295 blocks/s
constexpr float SPRINT_MULTIPLIER = 1.3004f;       // 5.612 / 4.317 = exact sprint multiplier
constexpr float SNEAK_MULTIPLIER = 0.3f;           // 1.295 / 4.317 ≈ 0.3

// Global physics player instance
PhysicsPlayer g_player;

void InitPhysicsPlayer(ChunkManager* chunkManager) {
    // Default spawn position
    float spawnX = 0.0f;
    float spawnZ = 0.0f;
    float spawnY = 70.0f; // Default height in case world generator isn't available
    
    // If we have a chunk manager, get the actual surface height from loaded chunks
    if (chunkManager) {
        int surfaceHeight = chunkManager->GetActualSurfaceHeight(spawnX, spawnZ);
        spawnY = static_cast<float>(surfaceHeight) + 2.0f; // Spawn 2 blocks above surface
        std::cout << "Player spawning at actual surface height: " << surfaceHeight << " (Y: " << spawnY << ")" << std::endl;
    } else {
        std::cout << "Warning: No chunk manager available, using default spawn height: " << spawnY << std::endl;
    }
    
    g_player.position = { spawnX, spawnY, spawnZ };
    g_player.velocity = { 0.0f, 0.0f, 0.0f };  // No initial velocity
    g_player.yaw = 0.0f;    // Face forward (towards negative Z)
    g_player.pitch = 0.0f;  // Look straight ahead
    g_player.onGround = false;
    g_player.sprinting = false;
    g_player.sneaking = false;
    g_player.jumpCooldown = 0;
    
    // Initialize physics system with chunk manager
    InitializeBlockPhysics(chunkManager);
}

Vector3 CalculateInputDirection() {
    Vector3 inputDir = { 0.0f, 0.0f, 0.0f };
    
    // Calculate forward and right vectors based on yaw (fixing potential direction issues)
    Vector3 forward = { sinf(g_player.yaw), 0.0f, cosf(g_player.yaw) };
    Vector3 right = { cosf(g_player.yaw), 0.0f, -sinf(g_player.yaw) }; // Fixed right vector
    
    // WASD input
    if (IsKeyDown(KEY_W)) {
        inputDir.x += forward.x;
        inputDir.z += forward.z;
    }
    if (IsKeyDown(KEY_S)) {
        inputDir.x -= forward.x;
        inputDir.z -= forward.z;
    }
    if (IsKeyDown(KEY_A)) {
        inputDir.x += right.x;  // Move left (use positive right vector)
        inputDir.z += right.z;
    }
    if (IsKeyDown(KEY_D)) {
        inputDir.x -= right.x;  // Move right (use negative right vector)
        inputDir.z -= right.z;
    }
    
    // Normalize input direction (optimized to avoid sqrt when possible)
    float lengthSq = inputDir.x * inputDir.x + inputDir.z * inputDir.z;
    if (lengthSq > 0.0f) {
        float invLength = 1.0f / sqrtf(lengthSq);
        inputDir.x *= invLength;
        inputDir.z *= invLength;
    }
    
    return inputDir;
}

void ApplyHorizontalMovement(float deltaTime) {
    Vector3 inputDir = CalculateInputDirection();
    
    // Calculate target speed based on input and state
    float targetSpeed = 0.0f;
    if (inputDir.x != 0 || inputDir.z != 0) {
        targetSpeed = g_player.onGround ? GROUND_SPEED : AIR_SPEED;
        
        // Apply movement modifiers
        if (g_player.sprinting) targetSpeed *= SPRINT_MULTIPLIER;
        else if (g_player.sneaking) targetSpeed *= SNEAK_MULTIPLIER;
    }
    
    // Calculate target velocity
    Vector3 targetVelocity = {
        inputDir.x * targetSpeed,
        g_player.velocity.y, // Don't touch Y velocity here
        inputDir.z * targetSpeed
    };
    
    // Apply friction/acceleration toward target velocity (authentic Minecraft physics)
    // Ground friction = 0.91 * slipperiness (0.6 for normal blocks)
    // Air friction = 0.91 per tick
    float friction = g_player.onGround ? GROUND_FRICTION : AIR_FRICTION;
    float lerpFactor = 1.0f - expf(-friction * deltaTime);
    
    g_player.velocity.x = g_player.velocity.x + (targetVelocity.x - g_player.velocity.x) * lerpFactor;
    g_player.velocity.z = g_player.velocity.z + (targetVelocity.z - g_player.velocity.z) * lerpFactor;
}

void ApplyVerticalMovement(float deltaTime) {
    // Handle jumping - check if space is being held when landing
    if (IsKeyDown(KEY_SPACE) && g_player.onGround) {
        g_player.velocity.y = JUMP_VELOCITY;
        g_player.onGround = false;
    }
    
    // gravity and drag when not on ground 
    if (!g_player.onGround) {
        //  gravity: vy = (vy - 0.08) * 0.98 per tick
        // Converted to per-second: first gravity, then drag
        g_player.velocity.y -= GRAVITY * deltaTime;
        
        // vertical drag (0.98 per tick/per second)
        g_player.velocity.y *= (1.0f - VERTICAL_DRAG * deltaTime);
        
        // terminal(montage) velocity
        if (g_player.velocity.y < -50.0f) {
            g_player.velocity.y = -50.0f;
        }
    }
}

void UpdatePhysicsPlayer(GameState gameState, float deltaTime, ChunkManager* chunkManager) {
    if (gameState != GAMEPLAY) return;
    
    // Handle sprinting and sneaking
    g_player.sprinting = IsKeyDown(KEY_LEFT_SHIFT);
    g_player.sneaking = IsKeyDown(KEY_LEFT_CONTROL);
    
    // Mouse look
    Vector2 mouseDelta = GetMouseDelta();
    g_player.yaw -= mouseDelta.x * MOUSE_SENSITIVITY;
    g_player.pitch -= mouseDelta.y * MOUSE_SENSITIVITY;
    
    // Clamp pitch to prevent over-rotation
    if (g_player.pitch > 1.5f) g_player.pitch = 1.5f;
    if (g_player.pitch < -1.5f) g_player.pitch = -1.5f;
    
    // Apply simple block physics
    ApplyHorizontalMovement(deltaTime);
    ApplyVerticalMovement(deltaTime);
    
    // Handle block collisions (this updates position based on velocity)
    HandleBlockCollisions(chunkManager);
}


Camera3D GetRaylibCamera() {
    Camera3D camera = { 0 };
    
    // Set camera position at eye level
    camera.position = {
        g_player.position.x,
        g_player.position.y + PLAYER_EYE_HEIGHT,
        g_player.position.z
    };
    
    // Calculate target based on yaw and pitch
    camera.target = {
        camera.position.x + sinf(g_player.yaw) * cosf(g_player.pitch),
        camera.position.y + sinf(g_player.pitch),
        camera.position.z + cosf(g_player.yaw) * cosf(g_player.pitch)
    };
    
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = CAMERA_FOVY;
    camera.projection = CAMERA_PERSPECTIVE;
    
    return camera;
}
