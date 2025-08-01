#include "WorldGenerator.h"
#include <algorithm>
#include <cmath>
#include <iostream>

WorldGenerator::WorldGenerator(int worldSeed) 
    : seed(worldSeed), terrainScale(0.01f), terrainHeight(50.0f), 
      seaLevel(64), dirtDepth(4), caveThreshold(0.6f), rng(worldSeed),
      continentalConfig(4, 0.0025f, 1.0f, 2.0f, 0.5f),
      erosionConfig(4, 0.005f, 1.0f, 2.0f, 0.5f),
      peaksValleysConfig(4, 0.01f, 1.0f, 2.0f, 0.5f),
      densityConfig(3, 0.02f, 1.0f, 2.0f, 0.5f) {
    
    InitializeNoiseGenerators();
    SetupTerrainSplines();
    
    std::cout << "Advanced WorldGenerator initialized with seed: " << worldSeed << std::endl;
}

void WorldGenerator::InitializeNoiseGenerators() {
    // Continental noise - low frequency, controls ocean vs land
    continentalnessNoise = CreateFractalNoise(continentalConfig, 0);
    
    // Erosion noise - medium frequency, controls flatness
    erosionNoise = CreateFractalNoise(erosionConfig, 1000);
    
    // Peaks and valleys noise - higher frequency, adds local variation
    peaksValleysNoise = CreateFractalNoise(peaksValleysConfig, 2000);
    
    // Temperature and humidity for biomes
    temperatureNoise = CreateFractalNoise(NoiseConfig(3, 0.003f, 1.0f), 3000);
    humidityNoise = CreateFractalNoise(NoiseConfig(3, 0.003f, 1.0f), 4000);
    
    // 3D density noise for caves and overhangs
    auto densitySimplex = FastNoise::New<FastNoise::Simplex>();
    auto densityFractal = FastNoise::New<FastNoise::FractalFBm>();
    densityFractal->SetSource(densitySimplex);
    densityFractal->SetOctaveCount(densityConfig.octaves);
    densityFractal->SetLacunarity(densityConfig.lacunarity);
    densityFractal->SetGain(densityConfig.persistence);
    densityNoise = densityFractal;
    
    // Cave noise - for carving caves
    auto caveSimplex = FastNoise::New<FastNoise::Simplex>();
    auto caveFractal = FastNoise::New<FastNoise::FractalRidged>();
    caveFractal->SetSource(caveSimplex);
    caveFractal->SetOctaveCount(3);
    caveNoise = caveFractal;
    
    // Ore noise
    auto cellular = FastNoise::New<FastNoise::CellularValue>();
    cellular->SetJitterModifier(1.0f);
    oreNoise = cellular;
}

FastNoise::SmartNode<> WorldGenerator::CreateFractalNoise(const NoiseConfig& config, int seedOffset) {
    auto simplex = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(config.octaves);
    fractal->SetLacunarity(config.lacunarity);
    fractal->SetGain(config.persistence);
    return fractal;
}

void WorldGenerator::SetupTerrainSplines() {
    // Continental spline - creates oceans, coastlines, and inland areas
    continentalSpline.clear();
    continentalSpline.emplace_back(-1.0f, 30.0f);   // Deep ocean
    continentalSpline.emplace_back(-0.6f, 45.0f);   // Ocean
    continentalSpline.emplace_back(-0.2f, 60.0f);   // Coast
    continentalSpline.emplace_back(0.1f, 70.0f);    // Near coast
    continentalSpline.emplace_back(0.4f, 80.0f);    // Inland
    continentalSpline.emplace_back(0.8f, 100.0f);   // Far inland
    continentalSpline.emplace_back(1.0f, 120.0f);   // Deep inland
    
    // Erosion spline - controls mountainous vs flat terrain
    erosionSpline.clear();
    erosionSpline.emplace_back(-1.0f, 40.0f);    // Highly eroded (flat)
    erosionSpline.emplace_back(-0.5f, 20.0f);    // Moderately eroded
    erosionSpline.emplace_back(0.0f, 0.0f);      // Neutral
    erosionSpline.emplace_back(0.5f, -20.0f);    // Low erosion
    erosionSpline.emplace_back(1.0f, -40.0f);    // No erosion (mountains)
    
    // Peaks and valleys spline - adds local height variation
    peaksValleysSpline.clear();
    peaksValleysSpline.emplace_back(-1.0f, -30.0f);  // Deep valleys
    peaksValleysSpline.emplace_back(-0.5f, -15.0f);  // Valleys
    peaksValleysSpline.emplace_back(0.0f, 0.0f);     // Neutral
    peaksValleysSpline.emplace_back(0.5f, 15.0f);    // Hills
    peaksValleysSpline.emplace_back(1.0f, 30.0f);    // Peaks
}

void WorldGenerator::SetSeed(int newSeed) {
    seed = newSeed;
    rng.seed(newSeed);
    InitializeNoiseGenerators();
}

void WorldGenerator::GenerateChunk(Chunk& chunk) {
    ChunkCoord coord = chunk.GetCoord();
    
    // Generate sophisticated multi-layer terrain
    GenerateTerrainWithDensity(chunk);
    
    // Apply surface decorations
    ApplySurfaceDecorations(chunk);
    
    // Generate caves using 3D noise
    GenerateCaves(chunk);
    
    // Generate ores
    GenerateOres(chunk);
    
    // Generate structures (trees, etc.)
    GenerateStructures(chunk);
    
    chunk.SetGenerated(true);
    chunk.SetDirty(true);
}

float WorldGenerator::GetContinentalness(float worldX, float worldZ) {
    if (!continentalnessNoise) return 0.0f;
    return continentalnessNoise->GenSingle2D(worldX * continentalConfig.frequency, 
                                           worldZ * continentalConfig.frequency, seed);
}

float WorldGenerator::GetErosion(float worldX, float worldZ) {
    if (!erosionNoise) return 0.0f;
    return erosionNoise->GenSingle2D(worldX * erosionConfig.frequency, 
                                   worldZ * erosionConfig.frequency, seed + 1000);
}

float WorldGenerator::GetPeaksValleys(float worldX, float worldZ) {
    if (!peaksValleysNoise) return 0.0f;
    return peaksValleysNoise->GenSingle2D(worldX * peaksValleysConfig.frequency, 
                                        worldZ * peaksValleysConfig.frequency, seed + 2000);
}

int WorldGenerator::GetHeightAt(float worldX, float worldZ) {
    float continentalness = GetContinentalness(worldX, worldZ);
    float erosion = GetErosion(worldX, worldZ);
    float peaksValleys = GetPeaksValleys(worldX, worldZ);
    
    float height = GetTerrainHeight(continentalness, erosion, peaksValleys);
    
    // Clamp to valid range
    return std::max(1, std::min(static_cast<int>(height), CHUNK_HEIGHT - 10));
}

float WorldGenerator::GetTerrainHeight(float continentalness, float erosion, float peaksValleys) {
    // Base height from continentalness (ocean vs land)
    float baseHeight = EvaluateSpline(continentalSpline, continentalness);
    
    // Erosion modifier (flat vs mountainous)
    float erosionModifier = EvaluateSpline(erosionSpline, erosion);
    
    // Local peaks and valleys
    float peaksValleysModifier = EvaluateSpline(peaksValleysSpline, peaksValleys);
    
    // Combine all factors
    float finalHeight = baseHeight + erosionModifier + peaksValleysModifier;
    
    return finalHeight;
}

float WorldGenerator::EvaluateSpline(const std::vector<SplinePoint>& spline, float input) {
    if (spline.empty()) return 0.0f;
    if (spline.size() == 1) return spline[0].output;
    
    // Clamp input to spline range
    input = Clamp(input, spline.front().input, spline.back().input);
    
    // Find the two points to interpolate between
    for (size_t i = 0; i < spline.size() - 1; i++) {
        if (input >= spline[i].input && input <= spline[i + 1].input) {
            float t = (input - spline[i].input) / (spline[i + 1].input - spline[i].input);
            return Lerp(spline[i].output, spline[i + 1].output, SmoothStep(0.0f, 1.0f, t));
        }
    }
    
    return spline.back().output;
}

void WorldGenerator::GenerateTerrainWithDensity(Chunk& chunk) {
    ChunkCoord coord = chunk.GetCoord();
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    // Pre-calculate terrain heights for entire chunk column to avoid redundant calculations
    int surfaceHeights[CHUNK_SIZE][CHUNK_SIZE];
    BiomeType biomes[CHUNK_SIZE][CHUNK_SIZE];
    
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            float worldX = worldOrigin.x + x;
            float worldZ = worldOrigin.z + z;
            surfaceHeights[x][z] = GetHeightAt(worldX, worldZ);
            biomes[x][z] = GetBiome(worldX, worldZ);
        }
    }
    
    // Generate using 3D density field for overhangs and caves (optimized iteration order)
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            float worldX = worldOrigin.x + x;
            float worldZ = worldOrigin.z + z;
            
            int surfaceHeight = surfaceHeights[x][z];
            BiomeType biome = biomes[x][z];
            
            // Early termination: skip columns that are entirely above or below chunk
            int minY = std::max(0, surfaceHeight - 30); // Start slightly below surface
            int maxY = std::min(CHUNK_HEIGHT, surfaceHeight + 20); // End above surface
            
            for (int y = minY; y < maxY; y++) {
                float worldY = y;
                
                // Use 3D density for complex terrain shapes
                float density = GetDensity(worldX, worldY, worldZ);
                
                // Bias density based on distance from surface
                float distanceFromSurface = y - surfaceHeight;
                float surfaceBias = 1.0f - Clamp(std::abs(distanceFromSurface) / 20.0f, 0.0f, 1.0f);
                
                // Stronger bias to ensure terrain generation works properly
                if (y < surfaceHeight) {
                    density += surfaceBias * 1.5f; // Much stronger solid bias below surface
                    // Ensure solid blocks are placed below surface even with negative density
                    if (y <= surfaceHeight - 2) {
                        density = std::max(density, 0.1f); // Force solid for deep terrain
                    }
                } else {
                    density -= surfaceBias * 0.8f; // Stronger air bias above surface
                }
                
                // Determine block type based on density
                if (density > 0.0f) {
                    // Solid terrain
                    BlockType blockType = GetBlockTypeForHeight(y, surfaceHeight, x, z, biome);
                    chunk.SetBlock(x, y, z, blockType);
                } else {
                    // Air or water
                    if (y <= seaLevel) {
                        chunk.SetBlock(x, y, z, BlockType::AIR); // Water would go here
                    } else {
                        chunk.SetBlock(x, y, z, BlockType::AIR);
                    }
                }
            }
            
            // Handle blocks outside optimized range (mostly air/bedrock)
            // Fill bottom area with bedrock
            for (int y = 0; y < minY; y++) {
                if (y == 0) {
                    chunk.SetBlock(x, y, z, BlockType::BEDROCK);
                } else if (y < surfaceHeight - 50) {
                    chunk.SetBlock(x, y, z, BlockType::STONE);
                } else {
                    chunk.SetBlock(x, y, z, BlockType::AIR);
                }
            }
            
            // Fill top area with air
            for (int y = maxY; y < CHUNK_HEIGHT; y++) {
                chunk.SetBlock(x, y, z, BlockType::AIR);
            }
        }
    }
}

float WorldGenerator::GetDensity(float worldX, float worldY, float worldZ) {
    if (!densityNoise) return 0.0f;
    
    return densityNoise->GenSingle3D(worldX * densityConfig.frequency, 
                                   worldY * densityConfig.frequency,
                                   worldZ * densityConfig.frequency, seed + 5000);
}

BiomeType WorldGenerator::GetBiome(float worldX, float worldZ) {
    float temperature = temperatureNoise ? temperatureNoise->GenSingle2D(worldX * 0.003f, worldZ * 0.003f, seed + 3000) : 0.0f;
    float humidity = humidityNoise ? humidityNoise->GenSingle2D(worldX * 0.003f, worldZ * 0.003f, seed + 4000) : 0.0f;
    int height = GetHeightAt(worldX, worldZ);
    
    return DetermineBiome(temperature, humidity, height);
}

BiomeType WorldGenerator::DetermineBiome(float temperature, float humidity, int height) {
    // Ocean biome for low areas
    if (height < seaLevel - 5) {
        return BiomeType::OCEAN;
    }
    
    // Mountain biomes for high elevations
    if (height > seaLevel + 60) {
        if (temperature < -0.3f) {
            return BiomeType::FROZEN_PEAKS;
        } else {
            return BiomeType::MOUNTAINS;
        }
    }
    
    // Temperature-based biomes
    if (temperature < -0.5f) {
        return BiomeType::FROZEN_PEAKS;
    } else if (temperature > 0.5f) {
        if (humidity < -0.3f) {
            return BiomeType::DESERT;
        } else {
            return BiomeType::PLAINS;
        }
    } else {
        // Moderate temperature
        if (humidity > 0.3f) {
            return BiomeType::SWAMP;
        } else if (humidity > -0.2f) {
            return BiomeType::FOREST;
        } else {
            return BiomeType::HILLS;
        }
    }
}

BlockType WorldGenerator::GetBlockTypeForHeight(int height, int surfaceHeight, int x, int z, BiomeType biome) {
    int depthFromSurface = surfaceHeight - height;
    
    if (depthFromSurface < 0) {
        return BlockType::AIR; // Above surface
    } else if (depthFromSurface == 0) {
        return GetSurfaceBlock(biome, height);
    } else if (depthFromSurface < dirtDepth) {
        return GetSubsurfaceBlock(biome, depthFromSurface);
    } else {
        return BlockType::STONE;
    }
}

BlockType WorldGenerator::GetSurfaceBlock(BiomeType biome, int height) {
    switch (biome) {
        case BiomeType::OCEAN:
            return height <= seaLevel ? BlockType::DIRT : BlockType::GRASS;
        case BiomeType::DESERT:
            return BlockType::DIRT; // Would be sand in full implementation
        case BiomeType::FROZEN_PEAKS:
            return BlockType::STONE; // Would be snow in full implementation
        case BiomeType::SWAMP:
            return BlockType::DIRT;
        default:
            return BlockType::GRASS;
    }
}

BlockType WorldGenerator::GetSubsurfaceBlock(BiomeType biome, int depth) {
    switch (biome) {
        case BiomeType::DESERT:
            return BlockType::DIRT; // Would be sand/sandstone
        default:
            return BlockType::DIRT;
    }
}

void WorldGenerator::ApplySurfaceDecorations(Chunk& chunk) {
    ChunkCoord coord = chunk.GetCoord();
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            float worldX = worldOrigin.x + x;
            float worldZ = worldOrigin.z + z;
            
            // Find surface height
            int surfaceHeight = -1;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                if (chunk.GetBlock(x, y, z) != BlockType::AIR) {
                    surfaceHeight = y;
                    break;
                }
            }
            
            if (surfaceHeight >= 0 && surfaceHeight < CHUNK_HEIGHT - 1) {
                BiomeType biome = GetBiome(worldX, worldZ);
                PlaceVegetation(chunk, x, z, surfaceHeight, biome);
            }
        }
    }
}

void WorldGenerator::GenerateCaves(Chunk& chunk) {
    ChunkCoord coord = chunk.GetCoord();
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 1; y < 80; y++) { // Caves only below y=80
                float worldX = worldOrigin.x + x;
                float worldY = y;
                float worldZ = worldOrigin.z + z;
                
                if (ShouldGenerateCave(worldX, worldY, worldZ)) {
                    // Only carve out solid blocks
                    if (chunk.GetBlock(x, y, z) != BlockType::AIR) {
                        chunk.SetBlock(x, y, z, BlockType::AIR);
                    }
                }
            }
        }
    }
}

void WorldGenerator::GenerateOres(Chunk& chunk) {
    ChunkCoord coord = chunk.GetCoord();
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 1; y < 64; y++) { // Ores only in lower levels
                float worldX = worldOrigin.x + x;
                float worldY = y;
                float worldZ = worldOrigin.z + z;
                
                BlockType oreType;
                if (ShouldGenerateOre(worldX, worldY, worldZ, oreType)) {
                    // Only replace stone blocks
                    if (chunk.GetBlock(x, y, z) == BlockType::STONE) {
                        chunk.SetBlock(x, y, z, oreType);
                    }
                }
            }
        }
    }
}

void WorldGenerator::GenerateStructures(Chunk& chunk) {
    ChunkCoord coord = chunk.GetCoord();
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    // Simple tree generation
    for (int x = 2; x < CHUNK_SIZE - 2; x += 4) {
        for (int z = 2; z < CHUNK_SIZE - 2; z += 4) {
            float worldX = worldOrigin.x + x;
            float worldZ = worldOrigin.z + z;
            
            // Find surface height
            int surfaceHeight = -1;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
                if (chunk.GetBlock(x, y, z) != BlockType::AIR) {
                    surfaceHeight = y;
                    break;
                }
            }
            
            if (surfaceHeight >= seaLevel && surfaceHeight < CHUNK_HEIGHT - 10) {
                BiomeType biome = GetBiome(worldX, worldZ);
                
                // Random chance for tree placement
                std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                if (dist(rng) < 0.1f) { // 10% chance
                    PlaceTree(chunk, x, z, surfaceHeight, biome);
                }
            }
        }
    }
}

bool WorldGenerator::ShouldGenerateCave(float x, float y, float z) {
    if (!caveNoise) return false;
    
    float noise1 = caveNoise->GenSingle3D(x * 0.02f, y * 0.02f, z * 0.02f, seed + 6000);
    float noise2 = caveNoise->GenSingle3D(x * 0.01f, y * 0.03f, z * 0.01f, seed + 7000);
    
    return (noise1 > caveThreshold && noise2 > caveThreshold * 0.8f);
}

bool WorldGenerator::ShouldGenerateOre(float x, float y, float z, BlockType& oreType) {
    if (!oreNoise) return false;
    
    float noise = oreNoise->GenSingle3D(x * 0.1f, y * 0.1f, z * 0.1f, seed + 8000);
    
    if (noise > 0.7f) {
        oreType = BlockType::COBBLESTONE; // Placeholder for coal ore
        return true;
    }
    
    return false;
}

void WorldGenerator::PlaceTree(Chunk& chunk, int x, int z, int groundHeight, BiomeType biome) {
    if (biome == BiomeType::DESERT || biome == BiomeType::FROZEN_PEAKS || biome == BiomeType::OCEAN) {
        return; // No trees in these biomes
    }
    
    int treeHeight = 4 + (rng() % 3); // 4-6 blocks tall
    
    // Place trunk
    for (int y = 1; y <= treeHeight; y++) {
        if (groundHeight + y < CHUNK_HEIGHT) {
            chunk.SetBlock(x, groundHeight + y, z, BlockType::WOOD);
        }
    }
}

void WorldGenerator::PlaceVegetation(Chunk& chunk, int x, int z, int groundHeight, BiomeType biome) {
    // Simple grass placement logic could go here
    // For now, just ensure grass blocks have proper surface
    if (chunk.GetBlock(x, groundHeight, z) == BlockType::DIRT && biome != BiomeType::DESERT) {
        chunk.SetBlock(x, groundHeight, z, BlockType::GRASS);
    }
}

// Keep the old simple terrain method for compatibility
void WorldGenerator::GenerateTerrain(Chunk& chunk) {
    GenerateTerrainWithDensity(chunk);
}

// Utility functions
float WorldGenerator::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float WorldGenerator::SmoothStep(float edge0, float edge1, float x) {
    float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float WorldGenerator::Clamp(float value, float min, float max) {
    return std::max(min, std::min(value, max));
}

void WorldGenerator::ConfigureNoise(const NoiseConfig& continental, const NoiseConfig& erosion, 
                                   const NoiseConfig& peaksValleys, const NoiseConfig& density) {
    continentalConfig = continental;
    erosionConfig = erosion;
    peaksValleysConfig = peaksValleys;
    densityConfig = density;
    InitializeNoiseGenerators();
}
