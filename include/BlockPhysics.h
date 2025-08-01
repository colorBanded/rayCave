#pragma once

#include "raylib.h"

// Forward declaration
class ChunkManager;

// Player collision box (authentic Minecraft dimensions)
constexpr float PLAYER_WIDTH = 0.6f;           // Player width (blocks) - authentic Minecraft
constexpr float PLAYER_HEIGHT = 1.8f;          // Player height (blocks) - authentic Minecraft

// Block world functions - now connected to real world data
bool IsBlockSolid(int x, int y, int z, ChunkManager* chunkManager);
bool CheckCollision(Vector3 position, ChunkManager* chunkManager);
void HandleBlockCollisions(ChunkManager* chunkManager);

// Initialize physics system with chunk manager
void InitializeBlockPhysics(ChunkManager* chunkManager);
