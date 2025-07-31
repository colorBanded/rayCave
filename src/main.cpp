#include "raylib.h"
#include "rlgl.h"
#include "FastNoise.h"
#include <vector>
#include <unordered_map>
#include <cmath>

// Game state enumeration
enum GameState {
    GAMEPLAY,
    PAUSED
};

// Constants
constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;
constexpr int GRID_SIZE = 10;
constexpr float CUBE_SIZE = 1.0f;
constexpr float CAMERA_FOVY = 45.0f;

// Function Declarations
void InitCamera(Camera3D &camera);
void Update();
void Render(const Camera3D &camera, GameState gameState);

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "3Djunk");
    SetTargetFPS(60);

    DisableCursor(); // Lock cursor at startup

    Camera3D camera = { 0 };
    InitCamera(camera);

    GameState gameState = GAMEPLAY;

    while (!WindowShouldClose()) {
        // Handle ESC toggle between pause and gameplay
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

        if (gameState == GAMEPLAY) {
            UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        }

        Update();
        Render(camera, gameState);
    }

    CloseWindow();
    return 0;
}

void InitCamera(Camera3D &camera) {
    camera.position = { 4.0f, 4.0f, 4.0f };
    camera.target = { 0.0f, 0.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = CAMERA_FOVY;
    camera.projection = CAMERA_PERSPECTIVE;
}

void Update() {
    // Game logic (e.g., input, world updates) goes here
}

void Render(const Camera3D &camera, GameState gameState) {
    BeginDrawing();
    ClearBackground(BLANK);

    BeginMode3D(camera);

    // Draw central voxel
    DrawCube({ 0, 0, 0 }, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, GREEN);
    DrawCubeWires({ 0, 0, 0 }, CUBE_SIZE, CUBE_SIZE, CUBE_SIZE, DARKGREEN);

    // Grid for reference
    DrawGrid(GRID_SIZE, 1.0f);

    EndMode3D();

    // Always draw HUD
    DrawFPS(10, 10);
    DrawText("Simple Voxel (WASD + Mouse)", 10, 30, 20, DARKGRAY);

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
