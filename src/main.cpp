#include "raylib.h"
#include "rlgl.h"
#include "FastNoise.h"
#include "TextureManager.h"
#include "PhysicsPlayer.h"
#include "ChunkManager.h"
#include "OptimizedChunkSystem.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <iostream>

// Constants
constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;
constexpr int GRID_SIZE = 10;
constexpr float CUBE_SIZE = 1.0f;
constexpr float MOVE_SPEED = 5.0f;

// Function Declarations
void Render(GameState gameState, ChunkManager& chunkManager, OptimizedChunkManager& optimizedChunkManager);

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "3Djunk");
    SetExitKey(KEY_NULL); // Prevent ESC from closing window


    // Initialize texture system
    if (!g_textureManager.Initialize()) {
        std::cout << "Failed to initialize texture manager!" << std::endl;
        CloseWindow();
        return -1;
    }
    
    // Print texture loading status
    std::cout << "Texture Manager initialized successfully!" << std::endl;
    std::cout << "The game will now attempt to load textures from assets/textures/blocks/" << std::endl;
    std::cout << "If textures don't exist, colored blocks will be used as fallback." << std::endl;

    // Initialize chunk manager with 6 worker threads for faster loading
    ChunkManager chunkManager("world/", 8, 6); // 8 chunk render distance, 6 worker threads
    if (!chunkManager.Initialize(12345)) {
        std::cout << "Failed to initialize chunk manager!" << std::endl;
        g_textureManager.Cleanup();
        CloseWindow();
        return -1;
    }
    
    std::cout << "Chunk Manager initialized successfully!" << std::endl;
    
    // Initialize Optimized Chunk System with SVO and Marching Cubes
    std::cout << "Initializing Optimized Chunk System with SVO and Marching Cubes..." << std::endl;
    OptimizedChunkManager optimizedChunkManager;
    
    // Configure performance settings for your hardware
    OptimizedChunkManager::PerformanceSettings perfSettings;
    perfSettings.maxSmoothTerrainDistance = 96.0f;  // Use marching cubes within 96 blocks
    perfSettings.adaptiveQuality = true;            // Automatically adjust quality based on performance
    perfSettings.useHybridRendering = true;         // Mix smooth and block rendering intelligently
    perfSettings.targetFrameTime = 16.6f;           // Target 60 FPS
    optimizedChunkManager.SetPerformanceSettings(perfSettings);
    
    std::cout << "Optimized Chunk System initialized!" << std::endl;
    std::cout << "- Sparse Voxel Octrees: Enabled (memory efficient)" << std::endl;
    std::cout << "- Marching Cubes: Enabled (smooth terrain within " << perfSettings.maxSmoothTerrainDistance << " blocks)" << std::endl;
    std::cout << "- Adaptive Quality: " << (perfSettings.adaptiveQuality ? "Enabled" : "Disabled") << std::endl;
    
    // Pre-generate spawn chunks before player initialization
    Vector3 spawnPos = {0.0f, 0.0f, 0.0f}; // Default spawn position
    chunkManager.PreGenerateSpawnChunks(spawnPos, 2); // Generate 5x5 chunk area around spawn
    
    // Also load optimized chunks around spawn
    optimizedChunkManager.LoadChunksAroundPlayer(spawnPos, 3);

    DisableCursor(); 

    InitPhysicsPlayer(&chunkManager);

    GameState gameState = GAMEPLAY;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        if (IsKeyPressed(KEY_ESCAPE)) {
            switch (gameState) {
                case GAMEPLAY:
                    gameState = PAUSED;
                    EnableCursor();
                    break;
                case PAUSED:
                    gameState = GAMEPLAY;
                    DisableCursor();
                    break;
            }
        }

        UpdatePhysicsPlayer(gameState, deltaTime, &chunkManager);
        
        // Update chunk manager with player position and velocity for predictive loading
        if (gameState == GAMEPLAY) {
            chunkManager.UpdatePlayerPosition(g_player.position, g_player.velocity);
            optimizedChunkManager.UpdatePlayerPosition(g_player.position, g_player.velocity);
        }
        
        // Update the optimized chunk system
        optimizedChunkManager.Update(deltaTime, g_player.position);
        
        Render(gameState, chunkManager, optimizedChunkManager);
    }

    // Cleanup
    chunkManager.Shutdown();
    g_textureManager.Cleanup();
    CloseWindow();
    return 0;
}

void Render(GameState gameState, ChunkManager& chunkManager, OptimizedChunkManager& optimizedChunkManager) {
    BeginDrawing();
    ClearBackground(BLANK);

    Camera3D camera = GetRaylibCamera();
    
    // Temporarily disable backface culling to debug rendering issues
    rlDisableBackfaceCulling();
    
    BeginMode3D(camera);

    // Render traditional chunks around the player
    chunkManager.RenderChunks(g_player.position, g_textureManager);
    
    // Render the optimized SVO/Marching Cubes chunks
    optimizedChunkManager.RenderChunks(g_player.position, g_textureManager);

    // Keep the sample blocks for testing textures (at a higher Y level to avoid interference)
    Vector3 positions[] = {
        { 20, 80, 0 },    // Grass
        { 22, 80, 0 },    // Dirt
        { 18, 80, 0 },    // Stone
        { 20, 80, 2 },    // Wood
        { 20, 80, -2 }    // Cobblestone
    };
    
    BlockType blockTypes[] = {
        BlockType::GRASS,
        BlockType::DIRT,
        BlockType::STONE,
        BlockType::WOOD,
        BlockType::COBBLESTONE
    };
    
    for (int i = 0; i < 5; i++) {
        if (blockTypes[i] == BlockType::GRASS) {
            g_textureManager.DrawMultiFaceCube(positions[i], CUBE_SIZE, blockTypes[i]);
        } else {
            g_textureManager.DrawTexturedCube(positions[i], CUBE_SIZE, blockTypes[i]);
        }
        
        DrawCubeWires(positions[i], CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, BLACK);
    }

    // Grid for reference (smaller for better performance)
    DrawGrid(20, 1.0f);

    EndMode3D();
    
    // Re-enable backface culling for UI elements
    rlEnableBackfaceCulling();

    // Always draw HUD
    DrawFPS(10, 10);
    DrawText(":3voxel - mrewmaxxing", 10, 30, 18, DARKGRAY);
    DrawText("Features: chunks or something idfk", 10, 50, 16, DARKGRAY);
    
    // Chunk statistics
    char chunkStats[256];
    snprintf(chunkStats, sizeof(chunkStats), "Traditional Chunks: %d | Optimized Chunks: %zu", 
            chunkManager.GetLoadedChunkCount(),
            optimizedChunkManager.GetLoadedChunkCount());
    DrawText(chunkStats, 10, 70, 16, DARKGRAY);
    
    // Optimized system statistics
    char optimizedStats[256];
    snprintf(optimizedStats, sizeof(optimizedStats), "SVO Memory: %.1f MB | Total Vertices: %zu | Triangles: %zu", 
            optimizedChunkManager.GetTotalMemoryUsage() / (1024.0f * 1024.0f),
            optimizedChunkManager.GetTotalVertexCount(),
            optimizedChunkManager.GetTotalTriangleCount());
    DrawText(optimizedStats, 10, 90, 16, DARKGRAY);
    
    // Show which textures are available
    if (g_textureManager.IsInitialized()) {
        DrawText("Textures: LOADED", 10, 110, 16, GREEN);
    } else {
        DrawText("Textures: FAILED", 10, 110, 16, RED);
    }
    
    // Movement status
    char statusText[256];
    snprintf(statusText, sizeof(statusText), "Ground: %s | Sprint: %s | Vel: %.3f,%.3f,%.3f", 
            g_player.onGround ? "YES" : "NO",
            g_player.sprinting ? "YES" : "NO",
            g_player.velocity.x, g_player.velocity.y, g_player.velocity.z);
    DrawText(statusText, 10, 130, 16, DARKGRAY);
    
    // Player position and chunk info
    ChunkCoord playerChunk = ChunkCoord::FromWorldPos(g_player.position.x, g_player.position.z);
    char posText[256];
    snprintf(posText, sizeof(posText), "Pos: %.1f,%.1f,%.1f | Chunk: %d,%d", 
            g_player.position.x, g_player.position.y, g_player.position.z,
            playerChunk.x, playerChunk.z);
    DrawText(posText, 10, 150, 16, DARKGRAY);
    
    // Direction indicator
    const char* direction = "Unknown";
    float normalizedYaw = fmodf(g_player.yaw, 2.0f * PI);
    if (normalizedYaw < 0) normalizedYaw += 2.0f * PI;
    
    // Convert yaw to degrees for easier calculation
    float yawDegrees = normalizedYaw * 180.0f / PI;
    
    // Determine cardinal direction (with 45-degree ranges)
    if (yawDegrees >= 315.0f || yawDegrees < 45.0f) {
        direction = "North (N)";
    } else if (yawDegrees >= 45.0f && yawDegrees < 135.0f) {
        direction = "East (E)";
    } else if (yawDegrees >= 135.0f && yawDegrees < 225.0f) {
        direction = "South (S)";
    } else if (yawDegrees >= 225.0f && yawDegrees < 315.0f) {
        direction = "West (W)";
    }
    
    char directionText[128];
    snprintf(directionText, sizeof(directionText), "Facing: %s (%.1f°)", direction, yawDegrees);
    DrawText(directionText, 10, 170, 16, LIGHTGRAY);
    
    // Controls
    DrawText("Controls: SPACE=Jump, L-SHIFT=Sprint, L-CTRL=Sneak", 10, 190, 16, DARKGRAY);
    DrawText("Advanced: SVO + Marching Cubes terrain system active", 10, 210, 16, GREEN);

    // Draw visual compass in top-right corner
    float compassX = SCREEN_WIDTH - 120.0f;
    float compassY = 40.0f;
    float compassRadius = 35.0f;
    
    // Draw compass background circle
    DrawCircle(compassX, compassY, compassRadius, Fade(BLACK, 0.3f));
    DrawCircleLines(compassX, compassY, compassRadius, WHITE);
    
    // Draw cardinal direction markers
    DrawText("N", compassX - 5, compassY - compassRadius - 15, 14, WHITE);
    DrawText("S", compassX - 5, compassY + compassRadius + 5, 14, WHITE);
    DrawText("E", compassX + compassRadius + 5, compassY - 7, 14, WHITE);
    DrawText("W", compassX - compassRadius - 15, compassY - 7, 14, WHITE);
    
    // Draw direction arrow pointing where player is facing
    float arrowX = compassX + sinf(g_player.yaw) * (compassRadius - 5);
    float arrowY = compassY - cosf(g_player.yaw) * (compassRadius - 5);
    DrawCircle(arrowX, arrowY, 4, RED);
    
    // Draw arrow line from center to direction point
    DrawLine(compassX, compassY, arrowX, arrowY, RED);

    // Draw pause overlay if paused
    if (gameState == PAUSED) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));

        const char* pausedText = "PAUSED";
        int fontSize = 60;
        int textWidth = MeasureText(pausedText, fontSize);
        DrawText(pausedText, (SCREEN_WIDTH - textWidth) / 2, SCREEN_HEIGHT / 2 - fontSize / 2, fontSize, RAYWHITE);
    }

    EndDrawing();
}
