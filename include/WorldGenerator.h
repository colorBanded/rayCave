#pragma once

#include "Chunk.h"
#include "FastNoise.h"
#include <random>
#include <vector>

// Spline point for terrain shaping
struct SplinePoint {
    float input;
    float output;
    float derivative;
    
    SplinePoint(float in, float out, float deriv = 0.0f) : input(in), output(out), derivative(deriv) {}
};

// Terrain noise configuration
struct NoiseConfig {
    int octaves;
    float frequency;
    float amplitude;
    float lacunarity;
    float persistence;
    
    NoiseConfig(int oct = 4, float freq = 0.01f, float amp = 1.0f, float lac = 2.0f, float pers = 0.5f)
        : octaves(oct), frequency(freq), amplitude(amp), lacunarity(lac), persistence(pers) {}
};

// Biome types for terrain variation
enum class BiomeType {
    OCEAN,
    PLAINS, 
    HILLS,
    MOUNTAINS,
    DESERT,
    FOREST,
    SWAMP,
    FROZEN_PEAKS
};

class WorldGenerator {
private:
    // Multi-layer noise system (Minecraft-inspired)
    FastNoise::SmartNode<> continentalnessNoise;  // Controls ocean vs land
    FastNoise::SmartNode<> erosionNoise;          // Controls flatness vs mountains  
    FastNoise::SmartNode<> peaksValleysNoise;     // Controls peaks and valleys
    FastNoise::SmartNode<> temperatureNoise;      // For biome selection
    FastNoise::SmartNode<> humidityNoise;         // For biome selection
    
    // 3D noise for caves and overhangs
    FastNoise::SmartNode<> densityNoise;          // 3D density field
    FastNoise::SmartNode<> caveNoise;
    FastNoise::SmartNode<> oreNoise;
    
    // Terrain shaping splines (like Minecraft)
    std::vector<SplinePoint> continentalSpline;
    std::vector<SplinePoint> erosionSpline;
    std::vector<SplinePoint> peaksValleysSpline;
    
    // Generation parameters
    NoiseConfig continentalConfig;
    NoiseConfig erosionConfig;
    NoiseConfig peaksValleysConfig;
    NoiseConfig densityConfig;
    
    float terrainScale;
    float terrainHeight;
    int seaLevel;
    int dirtDepth;
    float caveThreshold;
    int seed;
    
    std::mt19937 rng;

public:
    WorldGenerator(int worldSeed = 12345);
    ~WorldGenerator() = default;
    
    // Generate a chunk's terrain
    void GenerateChunk(Chunk& chunk);
    
    // Get height at world coordinates
    int GetHeightAt(float worldX, float worldZ);
    
    // Get noise values for debugging
    float GetContinentalness(float worldX, float worldZ);
    float GetErosion(float worldX, float worldZ);
    float GetPeaksValleys(float worldX, float worldZ);
    BiomeType GetBiome(float worldX, float worldZ);
    
    // Generate specific features
    void GenerateTerrain(Chunk& chunk);
    void GenerateTerrainWithDensity(Chunk& chunk);  // 3D noise terrain
    void GenerateCaves(Chunk& chunk);
    void GenerateOres(Chunk& chunk);
    void GenerateStructures(Chunk& chunk);
    void ApplySurfaceDecorations(Chunk& chunk);
    
    // Configuration
    void SetSeed(int newSeed);
    void SetTerrainScale(float scale) { terrainScale = scale; }
    void SetTerrainHeight(float height) { terrainHeight = height; }
    void SetSeaLevel(int level) { seaLevel = level; }
    void SetCaveThreshold(float threshold) { caveThreshold = threshold; }
    
    // Advanced configuration
    void ConfigureNoise(const NoiseConfig& continental, const NoiseConfig& erosion, 
                       const NoiseConfig& peaksValleys, const NoiseConfig& density);
    void SetupTerrainSplines();
    
    // Get generation parameters
    int GetSeed() const { return seed; }
    float GetTerrainScale() const { return terrainScale; }
    float GetTerrainHeight() const { return terrainHeight; }
    int GetSeaLevel() const { return seaLevel; }
    
private:
    // Noise generation helpers
    void InitializeNoiseGenerators();
    FastNoise::SmartNode<> CreateFractalNoise(const NoiseConfig& config, int seedOffset = 0);
    
    // Terrain shaping
    float EvaluateSpline(const std::vector<SplinePoint>& spline, float input);
    float GetTerrainHeight(float continentalness, float erosion, float peaksValleys);
    float GetDensity(float worldX, float worldY, float worldZ);
    
    // Block placement helpers
    BlockType GetBlockTypeForHeight(int height, int surfaceHeight, int x, int z, BiomeType biome);
    BlockType GetSurfaceBlock(BiomeType biome, int height);
    BlockType GetSubsurfaceBlock(BiomeType biome, int depth);
    
    // Feature generation
    bool ShouldGenerateCave(float x, float y, float z);
    bool ShouldGenerateOre(float x, float y, float z, BlockType& oreType);
    void PlaceTree(Chunk& chunk, int x, int z, int groundHeight, BiomeType biome);
    void PlaceVegetation(Chunk& chunk, int x, int z, int groundHeight, BiomeType biome);
    
    // Biome determination
    BiomeType DetermineBiome(float temperature, float humidity, int height);
    
    // Math utilities
    float Lerp(float a, float b, float t);
    float SmoothStep(float edge0, float edge1, float x);
    float Clamp(float value, float min, float max);
};
