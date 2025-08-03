#ifndef VOXEL_H
#define VOXEL_H

#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <unordered_map>
#include <string>

// Forward declarations
class VoxelWorld;
class TextureManager;

// Voxel types
enum VoxelType {
    VOXEL_AIR = 0,
    VOXEL_GRASS = 1,
    VOXEL_DIRT = 2,
    VOXEL_STONE = 3,
    VOXEL_WOOD = 4,
    VOXEL_COBBLESTONE = 5,
    VOXEL_LEAVES = 6,
    VOXEL_SAND = 7,
    VOXEL_WATER = 8,
    VOXEL_BEDROCK = 13
};

// Face directions for culling
enum FaceDirection {
    FACE_TOP = 0,
    FACE_BOTTOM,
    FACE_FRONT,
    FACE_BACK,
    FACE_RIGHT,
    FACE_LEFT,
    FACE_COUNT
};

// Single voxel structure
struct Voxel {
    VoxelType type;
    bool isActive;
    
    Voxel() : type(VOXEL_AIR), isActive(false) {}
    Voxel(VoxelType t) : type(t), isActive(t != VOXEL_AIR) {}
};

// Material mesh data for multi-material chunks
struct MaterialMesh {
    Mesh mesh;
    Material material;
    std::string textureName;
    bool isGenerated;
    
    MaterialMesh() : mesh({0}), material({0}), isGenerated(false) {}
};

// Voxel chunk for efficient rendering
class VoxelChunk {
public:
    static const int CHUNK_SIZE = 16;
    static const int CHUNK_HEIGHT = 16;
    
private:
    Voxel voxels[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    std::unordered_map<std::string, MaterialMesh> materialMeshes;
    bool meshNeedsUpdate;
    Vector3 chunkPosition;
    bool meshGenerated;
    
public:
    VoxelChunk(Vector3 position);
    ~VoxelChunk();
    
    // Voxel management
    void SetVoxel(int x, int y, int z, VoxelType type);
    Voxel GetVoxel(int x, int y, int z) const;
    bool IsValidPosition(int x, int y, int z) const;
    
    // Mesh generation
    void GenerateMesh(VoxelWorld* world = nullptr, TextureManager* textureManager = nullptr);
    void Draw();
    void MarkForUpdate() { meshNeedsUpdate = true; }
    
    // Utility
    Vector3 GetWorldPosition(int x, int y, int z) const;
    Vector3 GetChunkPosition() const { return chunkPosition; }
    
private:
    bool IsFaceVisible(int x, int y, int z, FaceDirection face, VoxelWorld* world = nullptr) const;
    
    // Greedy meshing structures
    struct FaceMask {
        bool visible;
        VoxelType voxelType;
        std::string textureName;
        
        FaceMask() : visible(false), voxelType(VOXEL_AIR), textureName("") {}
        FaceMask(bool v, VoxelType t, const std::string& tex) : visible(v), voxelType(t), textureName(tex) {}
        
        bool operator==(const FaceMask& other) const {
            return visible == other.visible && voxelType == other.voxelType && textureName == other.textureName;
        }
    };
    
    struct QuadMesh {
        Vector3 startPosition;  // Position of the first voxel in the quad
        int width, height;      // Size in voxel units
        FaceDirection face;
        std::string textureName;
        
        QuadMesh(Vector3 pos, int w, int h, FaceDirection f, const std::string& tex) 
            : startPosition(pos), width(w), height(h), face(f), textureName(tex) {}
    };
    
    void GenerateGreedyMesh(VoxelWorld* world, TextureManager* textureManager);
    int GetMaxLayerForFace(FaceDirection face) const;
    void ExtractFaceMask(FaceDirection face, int layer, VoxelWorld* world, TextureManager* textureManager, 
                        FaceMask mask[CHUNK_SIZE][CHUNK_SIZE]);
    void GreedyMeshFace(FaceDirection face, int layer, FaceMask mask[CHUNK_SIZE][CHUNK_SIZE], 
                       std::vector<QuadMesh>& quads);
    void AddQuadToMesh(std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                      std::vector<Vector2>& texcoords, std::vector<Color>& colors,
                      const QuadMesh& quad) const;
    
    // Face generation methods
    void AddFaceToMesh(std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                       std::vector<Vector2>& texcoords, std::vector<Color>& colors,
                       Vector3 position, FaceDirection face) const;
};

// Voxel world manager
class VoxelWorld {
private:
    std::vector<std::vector<VoxelChunk*>> chunks;
    int worldWidth, worldDepth;
    TextureManager* textureManager;
    
public:
    VoxelWorld(int width, int depth);
    ~VoxelWorld();
    
    // Texture management
    void SetTextureManager(TextureManager* tm) { textureManager = tm; }
    
    // World management
    void SetVoxel(int worldX, int worldY, int worldZ, VoxelType type);
    Voxel GetVoxel(int worldX, int worldY, int worldZ) const;
    
    // Rendering
    void Draw();
    void Update();
    
    // Utility
    VoxelChunk* GetChunk(int chunkX, int chunkZ);
    void WorldToChunkCoords(int worldX, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localZ) const;
    
    // Terrain generation
    void GenerateTestTerrain();
};

#endif // VOXEL_H
