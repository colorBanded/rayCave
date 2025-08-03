#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "../include/voxel.h"
#include "../include/texture_manager.h"

int main() {
    // Initialize window
    InitWindow(800, 600, "rayCave - 3D World");
    
    // Define the camera to look into our 3d world
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 10.0f, 8.0f, 10.0f };    // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };        // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };            // Camera up vector (rotation towards target)
    camera.fovy = 60.0f;                                   // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                // Camera projection type
    
    // Game state
    bool isPaused = false;
    int selectedHotbarSlot = 0; // Currently selected hotbar slot (0-8)
    
    // Initialize texture manager
    TextureManager textureManager;
    
    // Create voxel world (4x4 chunks)
    VoxelWorld world(4, 4);
    world.SetTextureManager(&textureManager);
    world.GenerateTestTerrain();
    
    // Lock cursor initially
    DisableCursor();
    
    // Disable ESC key for closing window (we'll handle it manually)
    SetExitKey(KEY_NULL);
    
    // Set target FPS
    SetTargetFPS(60);
    
    // Main game loop
    while (!WindowShouldClose()) {
        // Handle pause toggle
        if (IsKeyPressed(KEY_ESCAPE)) {
            isPaused = !isPaused;
            if (isPaused) {
                EnableCursor();  // Show cursor when paused
            } else {
                DisableCursor(); // Hide cursor when playing
            }
        }
        
        // Update camera only when not paused
        if (!isPaused) {
            UpdateCamera(&camera, CAMERA_FREE);
            
            // Handle hotbar selection (1-9 keys)
            if (IsKeyPressed(KEY_ONE)) selectedHotbarSlot = 0;
            else if (IsKeyPressed(KEY_TWO)) selectedHotbarSlot = 1;
            else if (IsKeyPressed(KEY_THREE)) selectedHotbarSlot = 2;
            else if (IsKeyPressed(KEY_FOUR)) selectedHotbarSlot = 3;
            else if (IsKeyPressed(KEY_FIVE)) selectedHotbarSlot = 4;
            else if (IsKeyPressed(KEY_SIX)) selectedHotbarSlot = 5;
            else if (IsKeyPressed(KEY_SEVEN)) selectedHotbarSlot = 6;
            else if (IsKeyPressed(KEY_EIGHT)) selectedHotbarSlot = 7;
            else if (IsKeyPressed(KEY_NINE)) selectedHotbarSlot = 8;
            
            // Handle mouse wheel for hotbar selection
            float wheelMove = GetMouseWheelMove();
            if (wheelMove != 0) {
                selectedHotbarSlot -= (int)wheelMove;
                if (selectedHotbarSlot < 0) selectedHotbarSlot = 8;
                if (selectedHotbarSlot > 8) selectedHotbarSlot = 0;
            }
        }
        
        // Update voxel world
        world.Update();
        
        // Begin drawing
        BeginDrawing();
        ClearBackground(BLANK);
        
        // Begin 3D mode
        BeginMode3D(camera);
        
        // Draw voxel world
        world.Draw();
        
        // Draw a grid for reference
        DrawGrid(20, 1.0f);
        
        // End 3D mode
        EndMode3D();
        
        // Draw pause overlay if paused
        if (isPaused) {
            // Draw semi-transparent overlay
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.4f));
            
            // Draw pause message
            const char* pauseText = "PAUSED";
            int textWidth = MeasureText(pauseText, 60);
            DrawText(pauseText, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() / 2 - 30, 60, WHITE);
            
            const char* resumeText = "Press ESC to resume";
            int resumeWidth = MeasureText(resumeText, 20);
            DrawText(resumeText, (GetScreenWidth() - resumeWidth) / 2, GetScreenHeight() / 2 + 40, 20, LIGHTGRAY);
        } else {
            // Calculate facing direction for compass
            Vector3 forward = Vector3Subtract(camera.target, camera.position);
            forward = Vector3Normalize(forward);
            
            // Convert to compass direction
            float angle = atan2f(forward.x, forward.z) * RAD2DEG;
            if (angle < 0) angle += 360.0f;
            
            const char* direction = "?";
            if (angle >= 337.5f || angle < 22.5f) direction = "North";
            else if (angle >= 22.5f && angle < 67.5f) direction = "Northeast";
            else if (angle >= 67.5f && angle < 112.5f) direction = "East";
            else if (angle >= 112.5f && angle < 157.5f) direction = "Southeast";
            else if (angle >= 157.5f && angle < 202.5f) direction = "South";
            else if (angle >= 202.5f && angle < 247.5f) direction = "Southwest";
            else if (angle >= 247.5f && angle < 292.5f) direction = "West";
            else if (angle >= 292.5f && angle < 337.5f) direction = "Northwest";
            
            // Get current FPS
            int fps = GetFPS();
            
            // HUD Background panels
            int hudPadding = 8;
            int hudMargin = 10;
            
            // Top-left info panel
            const char* titleText = "rayCave";
            const char* versionText = "v1.0.0 - 3D Voxel World";
            const char* controlsText1 = "ESC - Pause";
            const char* controlsText2 = "Mouse - Look Around";
            const char* controlsText3 = "WASD - Move";
            
            int titleWidth = MeasureText(titleText, 28);
            int versionWidth = MeasureText(versionText, 16);
            int maxControlWidth = MeasureText(controlsText2, 12);
            int panelWidth = (titleWidth > versionWidth ? titleWidth : versionWidth) + hudPadding * 2;
            if (maxControlWidth + hudPadding * 2 > panelWidth) panelWidth = maxControlWidth + hudPadding * 2;
            int panelHeight = 28 + 16 + 12 * 3 + hudPadding * 2 + 20; // Extra spacing
            
            // Draw semi-transparent background for info panel
            DrawRectangle(hudMargin, hudMargin, panelWidth, panelHeight, Fade(BLACK, 0.7f));
            DrawRectangleLines(hudMargin, hudMargin, panelWidth, panelHeight, Fade(WHITE, 0.3f));
            
            // Draw title and info
            DrawText(titleText, hudMargin + hudPadding, hudMargin + hudPadding, 28, RAYWHITE);
            DrawText(versionText, hudMargin + hudPadding, hudMargin + hudPadding + 32, 16, LIGHTGRAY);
            DrawText(controlsText1, hudMargin + hudPadding, hudMargin + hudPadding + 52, 12, GRAY);
            DrawText(controlsText2, hudMargin + hudPadding, hudMargin + hudPadding + 66, 12, GRAY);
            DrawText(controlsText3, hudMargin + hudPadding, hudMargin + hudPadding + 80, 12, GRAY);
            
            // Top-right compass panel
            const char* compassTitle = "Navigation";
            const char* directionText = TextFormat("Direction: %s", direction);
            const char* angleText = TextFormat("Bearing: %.1f°", angle);
            const char* coordsText = TextFormat("Position: %.1f, %.1f, %.1f", 
                                              camera.position.x, camera.position.y, camera.position.z);
            
            int compassTitleWidth = MeasureText(compassTitle, 20);
            int directionWidth = MeasureText(directionText, 14);
            int angleWidth = MeasureText(angleText, 12);
            int coordsWidth = MeasureText(coordsText, 12);
            
            int compassPanelWidth = compassTitleWidth;
            if (directionWidth > compassPanelWidth) compassPanelWidth = directionWidth;
            if (angleWidth > compassPanelWidth) compassPanelWidth = angleWidth;
            if (coordsWidth > compassPanelWidth) compassPanelWidth = coordsWidth;
            compassPanelWidth += hudPadding * 2;
            
            int compassPanelHeight = 20 + 14 + 12 * 2 + hudPadding * 2 + 15; // Extra spacing
            int compassX = GetScreenWidth() - compassPanelWidth - hudMargin;
            
            // Draw compass panel background
            DrawRectangle(compassX, hudMargin, compassPanelWidth, compassPanelHeight, Fade(BLACK, 0.7f));
            DrawRectangleLines(compassX, hudMargin, compassPanelWidth, compassPanelHeight, Fade(WHITE, 0.3f));
            
            // Draw compass info
            DrawText(compassTitle, compassX + hudPadding, hudMargin + hudPadding, 20, RAYWHITE);
            DrawText(directionText, compassX + hudPadding, hudMargin + hudPadding + 25, 14, SKYBLUE);
            DrawText(angleText, compassX + hudPadding, hudMargin + hudPadding + 42, 12, LIGHTGRAY);
            DrawText(coordsText, compassX + hudPadding, hudMargin + hudPadding + 56, 12, LIGHTGRAY);
            
            // Bottom-right performance panel
            const char* perfTitle = "Performance";
            const char* fpsText = TextFormat("FPS: %d", fps);
            const char* targetText = "Target: 60 FPS";
            
            int perfTitleWidth = MeasureText(perfTitle, 16);
            int fpsWidth = MeasureText(fpsText, 14);
            int targetWidth = MeasureText(targetText, 12);
            
            int perfPanelWidth = perfTitleWidth;
            if (fpsWidth > perfPanelWidth) perfPanelWidth = fpsWidth;
            if (targetWidth > perfPanelWidth) perfPanelWidth = targetWidth;
            perfPanelWidth += hudPadding * 2;
            
            int perfPanelHeight = 16 + 14 + 12 + hudPadding * 2 + 10;
            int perfX = GetScreenWidth() - perfPanelWidth - hudMargin;
            int perfY = GetScreenHeight() - perfPanelHeight - hudMargin;
            
            // Draw performance panel background
            DrawRectangle(perfX, perfY, perfPanelWidth, perfPanelHeight, Fade(BLACK, 0.7f));
            DrawRectangleLines(perfX, perfY, perfPanelWidth, perfPanelHeight, Fade(WHITE, 0.3f));
            
            // Draw performance info with color coding for FPS
            DrawText(perfTitle, perfX + hudPadding, perfY + hudPadding, 16, RAYWHITE);
            Color fpsColor = (fps >= 55) ? GREEN : (fps >= 30) ? YELLOW : RED;
            DrawText(fpsText, perfX + hudPadding, perfY + hudPadding + 20, 14, fpsColor);
            DrawText(targetText, perfX + hudPadding, perfY + hudPadding + 36, 12, LIGHTGRAY);
            
            // Crosshair in center of screen
            int centerX = GetScreenWidth() / 2;
            int centerY = GetScreenHeight() / 2;
            int crosshairSize = 10;
            int crosshairThickness = 2;
            
            // Draw crosshair with outline for visibility
            DrawRectangle(centerX - crosshairSize/2 - 1, centerY - crosshairThickness/2 - 1, 
                         crosshairSize + 2, crosshairThickness + 2, BLACK);
            DrawRectangle(centerX - crosshairThickness/2 - 1, centerY - crosshairSize/2 - 1, 
                         crosshairThickness + 2, crosshairSize + 2, BLACK);
            
            DrawRectangle(centerX - crosshairSize/2, centerY - crosshairThickness/2, 
                         crosshairSize, crosshairThickness, WHITE);
            DrawRectangle(centerX - crosshairThickness/2, centerY - crosshairSize/2, 
                         crosshairThickness, crosshairSize, WHITE);
            
            // Draw hotbar at bottom center
            int hotbarY = GetScreenHeight() - 80; // 80 pixels from bottom
            textureManager.DrawHotbar(centerX, hotbarY, selectedHotbarSlot);
        }
        
        EndDrawing();
    }
    
    // Close window and unload resources
    CloseWindow();
    
    return 0;
}