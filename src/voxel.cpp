#include "../include/voxel.h"
#include "../include/texture_manager.h"
#include "rlgl.h"
#include <algorithm>
#include <map>

// VoxelChunk Implementation
VoxelChunk::VoxelChunk(Vector3 position) 
    : chunkPosition(position), meshNeedsUpdate(true), meshGenerated(false) {
    
    // Initialize all voxels as air
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                voxels[x][y][z] = Voxel(VOXEL_AIR);
            }
        }
    }
}

VoxelChunk::~VoxelChunk() {
    // Clean up all material meshes
    for (auto& pair : materialMeshes) {
        if (pair.second.isGenerated) {
            UnloadMesh(pair.second.mesh);
        }
        UnloadMaterial(pair.second.material);
    }
    materialMeshes.clear();
}

void VoxelChunk::SetVoxel(int x, int y, int z, VoxelType type) {
    if (IsValidPosition(x, y, z)) {
        voxels[x][y][z] = Voxel(type);
        meshNeedsUpdate = true;
    }
}

Voxel VoxelChunk::GetVoxel(int x, int y, int z) const {
    if (IsValidPosition(x, y, z)) {
        return voxels[x][y][z];
    }
    return Voxel(VOXEL_AIR);
}

bool VoxelChunk::IsValidPosition(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE && 
           y >= 0 && y < CHUNK_HEIGHT && 
           z >= 0 && z < CHUNK_SIZE;
}

Vector3 VoxelChunk::GetWorldPosition(int x, int y, int z) const {
    return Vector3Add(chunkPosition, (Vector3){(float)x, (float)y, (float)z});
}

void VoxelChunk::AddFaceToMesh(std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                               std::vector<Vector2>& texcoords, std::vector<Color>& colors,
                               Vector3 position, FaceDirection face) const {
    
    Vector3 v1, v2, v3, v4;
    Vector3 normal;
    
    // Define vertices and normal for each face (counter-clockwise winding)
    switch (face) {
        case FACE_TOP:
            v1 = Vector3Add(position, (Vector3){-0.5f, 0.5f, -0.5f});
            v2 = Vector3Add(position, (Vector3){-0.5f, 0.5f, 0.5f});
            v3 = Vector3Add(position, (Vector3){0.5f, 0.5f, 0.5f});
            v4 = Vector3Add(position, (Vector3){0.5f, 0.5f, -0.5f});
            normal = (Vector3){0.0f, 1.0f, 0.0f};
            break;
        case FACE_BOTTOM:
            v1 = Vector3Add(position, (Vector3){-0.5f, -0.5f, -0.5f});
            v2 = Vector3Add(position, (Vector3){0.5f, -0.5f, -0.5f});
            v3 = Vector3Add(position, (Vector3){0.5f, -0.5f, 0.5f});
            v4 = Vector3Add(position, (Vector3){-0.5f, -0.5f, 0.5f});
            normal = (Vector3){0.0f, -1.0f, 0.0f};
            break;
        case FACE_FRONT:
            v1 = Vector3Add(position, (Vector3){-0.5f, -0.5f, 0.5f});
            v2 = Vector3Add(position, (Vector3){0.5f, -0.5f, 0.5f});
            v3 = Vector3Add(position, (Vector3){0.5f, 0.5f, 0.5f});
            v4 = Vector3Add(position, (Vector3){-0.5f, 0.5f, 0.5f});
            normal = (Vector3){0.0f, 0.0f, 1.0f};
            break;
        case FACE_BACK:
            v1 = Vector3Add(position, (Vector3){0.5f, -0.5f, -0.5f});
            v2 = Vector3Add(position, (Vector3){-0.5f, -0.5f, -0.5f});
            v3 = Vector3Add(position, (Vector3){-0.5f, 0.5f, -0.5f});
            v4 = Vector3Add(position, (Vector3){0.5f, 0.5f, -0.5f});
            normal = (Vector3){0.0f, 0.0f, -1.0f};
            break;
        case FACE_RIGHT:
            v1 = Vector3Add(position, (Vector3){0.5f, -0.5f, 0.5f});
            v2 = Vector3Add(position, (Vector3){0.5f, -0.5f, -0.5f});
            v3 = Vector3Add(position, (Vector3){0.5f, 0.5f, -0.5f});
            v4 = Vector3Add(position, (Vector3){0.5f, 0.5f, 0.5f});
            normal = (Vector3){1.0f, 0.0f, 0.0f};
            break;
        case FACE_LEFT:
            v1 = Vector3Add(position, (Vector3){-0.5f, -0.5f, -0.5f});
            v2 = Vector3Add(position, (Vector3){-0.5f, -0.5f, 0.5f});
            v3 = Vector3Add(position, (Vector3){-0.5f, 0.5f, 0.5f});
            v4 = Vector3Add(position, (Vector3){-0.5f, 0.5f, -0.5f});
            normal = (Vector3){-1.0f, 0.0f, 0.0f};
            break;
    }
    
    // Get appropriate texture coordinates based on voxel type and face
    Vector2 uv1 = {0.0f, 1.0f};
    Vector2 uv2 = {1.0f, 1.0f};
    Vector2 uv3 = {1.0f, 0.0f};
    Vector2 uv4 = {0.0f, 0.0f};
    
    // Use white color for all vertices (texture will provide the color)
    Color vertexColor = WHITE;
    
    // Add two triangles to form a quad
    // Triangle 1: v1, v2, v3
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v3);
    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);
    texcoords.push_back(uv1);
    texcoords.push_back(uv2);
    texcoords.push_back(uv3);
    colors.push_back(vertexColor);
    colors.push_back(vertexColor);
    colors.push_back(vertexColor);
    
    // Triangle 2: v1, v3, v4
    vertices.push_back(v1);
    vertices.push_back(v3);
    vertices.push_back(v4);
    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);
    texcoords.push_back(uv1);
    texcoords.push_back(uv3);
    texcoords.push_back(uv4);
    colors.push_back(vertexColor);
    colors.push_back(vertexColor);
    colors.push_back(vertexColor);
}

bool VoxelChunk::IsFaceVisible(int x, int y, int z, FaceDirection face, VoxelWorld* world) const {
    if (!voxels[x][y][z].isActive) return false;
    
    int nx = x, ny = y, nz = z;
    
    // Get neighbor coordinates based on face direction
    switch (face) {
        case FACE_TOP: ny++; break;
        case FACE_BOTTOM: ny--; break;
        case FACE_FRONT: nz++; break;
        case FACE_BACK: nz--; break;
        case FACE_RIGHT: nx++; break;
        case FACE_LEFT: nx--; break;
    }
    
    // Check if neighbor is within this chunk
    if (IsValidPosition(nx, ny, nz)) {
        // Neighbor is in same chunk - check directly
        return !voxels[nx][ny][nz].isActive;
    }
    
    // Neighbor is outside chunk bounds - check neighboring chunk if world is provided
    if (world != nullptr) {
        // Convert local coordinates to world coordinates
        Vector3 worldPos = GetWorldPosition(x, y, z);
        int worldX = (int)worldPos.x;
        int worldY = (int)worldPos.y;
        int worldZ = (int)worldPos.z;
        
        // Calculate neighbor world coordinates
        int neighborWorldX = worldX, neighborWorldY = worldY, neighborWorldZ = worldZ;
        switch (face) {
            case FACE_TOP: neighborWorldY++; break;
            case FACE_BOTTOM: neighborWorldY--; break;
            case FACE_FRONT: neighborWorldZ++; break;
            case FACE_BACK: neighborWorldZ--; break;
            case FACE_RIGHT: neighborWorldX++; break;
            case FACE_LEFT: neighborWorldX--; break;
        }
        
        // Check if neighbor voxel exists in world
        Voxel neighborVoxel = world->GetVoxel(neighborWorldX, neighborWorldY, neighborWorldZ);
        return !neighborVoxel.isActive;
    }
    
    // If no world provided and neighbor is outside chunk, render the face
    return true;
}

void VoxelChunk::GenerateMesh(VoxelWorld* world, TextureManager* textureManager) {
    if (!meshNeedsUpdate) return;
    
    // Use greedy meshing for optimization
    GenerateGreedyMesh(world, textureManager);
    
    meshNeedsUpdate = false;
    meshGenerated = true;
}

void VoxelChunk::Draw() {
    if (meshGenerated) {
        for (const auto& pair : materialMeshes) {
            const MaterialMesh& matMesh = pair.second;
            if (matMesh.isGenerated && matMesh.mesh.vertexCount > 0) {
                DrawMesh(matMesh.mesh, matMesh.material, MatrixIdentity());
            }
        }
    }
}

// VoxelWorld Implementation
VoxelWorld::VoxelWorld(int width, int depth) : worldWidth(width), worldDepth(depth), textureManager(nullptr) {
    chunks.resize(worldWidth);
    for (int x = 0; x < worldWidth; x++) {
        chunks[x].resize(worldDepth);
        for (int z = 0; z < worldDepth; z++) {
            Vector3 chunkPos = {
                (float)(x * VoxelChunk::CHUNK_SIZE),
                0.0f,
                (float)(z * VoxelChunk::CHUNK_SIZE)
            };
            chunks[x][z] = new VoxelChunk(chunkPos);
        }
    }
}

VoxelWorld::~VoxelWorld() {
    for (int x = 0; x < worldWidth; x++) {
        for (int z = 0; z < worldDepth; z++) {
            delete chunks[x][z];
        }
    }
}

void VoxelWorld::WorldToChunkCoords(int worldX, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localZ) const {
    chunkX = worldX / VoxelChunk::CHUNK_SIZE;
    chunkZ = worldZ / VoxelChunk::CHUNK_SIZE;
    localX = worldX % VoxelChunk::CHUNK_SIZE;
    localZ = worldZ % VoxelChunk::CHUNK_SIZE;
    
    // Handle negative coordinates
    if (worldX < 0) {
        chunkX--;
        localX += VoxelChunk::CHUNK_SIZE;
    }
    if (worldZ < 0) {
        chunkZ--;
        localZ += VoxelChunk::CHUNK_SIZE;
    }
}

VoxelChunk* VoxelWorld::GetChunk(int chunkX, int chunkZ) {
    if (chunkX >= 0 && chunkX < worldWidth && chunkZ >= 0 && chunkZ < worldDepth) {
        return chunks[chunkX][chunkZ];
    }
    return nullptr;
}

void VoxelWorld::SetVoxel(int worldX, int worldY, int worldZ, VoxelType type) {
    int chunkX, chunkZ, localX, localZ;
    WorldToChunkCoords(worldX, worldZ, chunkX, chunkZ, localX, localZ);
    
    VoxelChunk* chunk = GetChunk(chunkX, chunkZ);
    if (chunk) {
        chunk->SetVoxel(localX, worldY, localZ, type);
    }
}

Voxel VoxelWorld::GetVoxel(int worldX, int worldY, int worldZ) const {
    int chunkX, chunkZ, localX, localZ;
    WorldToChunkCoords(worldX, worldZ, chunkX, chunkZ, localX, localZ);
    
    if (chunkX >= 0 && chunkX < worldWidth && chunkZ >= 0 && chunkZ < worldDepth) {
        return chunks[chunkX][chunkZ]->GetVoxel(localX, worldY, localZ);
    }
    return Voxel(VOXEL_AIR);
}

void VoxelWorld::Update() {
    // Generate meshes for chunks that need updates
    for (int x = 0; x < worldWidth; x++) {
        for (int z = 0; z < worldDepth; z++) {
            chunks[x][z]->GenerateMesh(this, textureManager);
        }
    }
}

void VoxelWorld::Draw() {
    for (int x = 0; x < worldWidth; x++) {
        for (int z = 0; z < worldDepth; z++) {
            chunks[x][z]->Draw();
        }
    }
}

void VoxelWorld::GenerateTestTerrain() {
    // Generate a simple test terrain
    for (int x = 0; x < worldWidth * VoxelChunk::CHUNK_SIZE; x++) {
        for (int z = 0; z < worldDepth * VoxelChunk::CHUNK_SIZE; z++) {
            // Simple height map
            int height = 4 + (int)(sin(x * 0.1f) * cos(z * 0.1f) * 3.0f);
            
            for (int y = 0; y < height && y < VoxelChunk::CHUNK_HEIGHT; y++) {
                VoxelType type = VOXEL_STONE;
                if (y == height - 1) type = VOXEL_GRASS;
                else if (y >= height - 3) type = VOXEL_DIRT;
                
                SetVoxel(x, y, z, type);
            }
        }
    }
}

// Greedy Meshing Implementation
void VoxelChunk::GenerateGreedyMesh(VoxelWorld* world, TextureManager* textureManager) {
    // Clean up existing meshes
    for (auto& pair : materialMeshes) {
        if (pair.second.isGenerated) {
            UnloadMesh(pair.second.mesh);
        }
        UnloadMaterial(pair.second.material);
    }
    materialMeshes.clear();
    
    // Map to collect quads for each material
    std::unordered_map<std::string, std::vector<QuadMesh>> materialQuads;
    
    // Process each face direction and each layer
    for (int face = 0; face < FACE_COUNT; face++) {
        FaceDirection faceDir = (FaceDirection)face;
        
        // For each face direction, process all layers
        int maxLayer = GetMaxLayerForFace(faceDir);
        for (int layer = 0; layer < maxLayer; layer++) {
            FaceMask mask[CHUNK_SIZE][CHUNK_SIZE];
            
            // Extract face mask for this direction and layer
            ExtractFaceMask(faceDir, layer, world, textureManager, mask);
            
            // Generate quads using greedy meshing
            std::vector<QuadMesh> quads;
            GreedyMeshFace(faceDir, layer, mask, quads);
            
            // Group quads by texture
            for (const auto& quad : quads) {
                materialQuads[quad.textureName].push_back(quad);
            }
        }
    }
    
    // Convert quads to meshes for each material
    for (const auto& pair : materialQuads) {
        const std::string& textureName = pair.first;
        const std::vector<QuadMesh>& quads = pair.second;
        
        if (quads.empty()) continue;
        
        std::vector<Vector3> vertices;
        std::vector<Vector3> normals;
        std::vector<Vector2> texcoords;
        std::vector<Color> colors;
        
        // Convert each quad to mesh data using original face logic
        for (const auto& quad : quads) {
            AddQuadToMesh(vertices, normals, texcoords, colors, quad);
        }
        
        // Create material mesh
        MaterialMesh& matMesh = materialMeshes[textureName];
        matMesh.textureName = textureName;
        
        // Initialize mesh
        matMesh.mesh.vertexCount = vertices.size();
        matMesh.mesh.triangleCount = vertices.size() / 3;
        
        // Allocate mesh data
        matMesh.mesh.vertices = (float*)RL_MALLOC(vertices.size() * 3 * sizeof(float));
        matMesh.mesh.normals = (float*)RL_MALLOC(vertices.size() * 3 * sizeof(float));
        matMesh.mesh.texcoords = (float*)RL_MALLOC(vertices.size() * 2 * sizeof(float));
        matMesh.mesh.colors = (unsigned char*)RL_MALLOC(vertices.size() * 4 * sizeof(unsigned char));
        
        // Copy data to mesh
        for (size_t i = 0; i < vertices.size(); i++) {
            matMesh.mesh.vertices[i * 3] = vertices[i].x;
            matMesh.mesh.vertices[i * 3 + 1] = vertices[i].y;
            matMesh.mesh.vertices[i * 3 + 2] = vertices[i].z;
            
            matMesh.mesh.normals[i * 3] = normals[i].x;
            matMesh.mesh.normals[i * 3 + 1] = normals[i].y;
            matMesh.mesh.normals[i * 3 + 2] = normals[i].z;
            
            matMesh.mesh.texcoords[i * 2] = texcoords[i].x;
            matMesh.mesh.texcoords[i * 2 + 1] = texcoords[i].y;
            
            matMesh.mesh.colors[i * 4] = colors[i].r;
            matMesh.mesh.colors[i * 4 + 1] = colors[i].g;
            matMesh.mesh.colors[i * 4 + 2] = colors[i].b;
            matMesh.mesh.colors[i * 4 + 3] = colors[i].a;
        }
        
        // Upload mesh to GPU
        UploadMesh(&matMesh.mesh, false);
        matMesh.isGenerated = true;
        
        // Set up material with appropriate texture
        matMesh.material = LoadMaterialDefault();
        if (textureManager && textureManager->HasTexture(textureName)) {
            matMesh.material.maps[MATERIAL_MAP_DIFFUSE].texture = textureManager->GetTexture(textureName);
        }
    }
}

int VoxelChunk::GetMaxLayerForFace(FaceDirection face) const {
    switch (face) {
        case FACE_TOP:
        case FACE_BOTTOM:
            return CHUNK_HEIGHT;  // Y layers
        case FACE_FRONT:
        case FACE_BACK:
            return CHUNK_SIZE;    // Z layers
        case FACE_RIGHT:
        case FACE_LEFT:
            return CHUNK_SIZE;    // X layers
        default:
            return 1;
    }
}

void VoxelChunk::ExtractFaceMask(FaceDirection face, int layer, VoxelWorld* world, TextureManager* textureManager, 
                                 FaceMask mask[CHUNK_SIZE][CHUNK_SIZE]) {
    // Initialize mask
    for (int i = 0; i < CHUNK_SIZE; i++) {
        for (int j = 0; j < CHUNK_SIZE; j++) {
            mask[i][j] = FaceMask();
        }
    }
    
    // Extract face information for the specific layer
    for (int u = 0; u < CHUNK_SIZE; u++) {
        for (int v = 0; v < CHUNK_SIZE; v++) {
            // Convert u,v coordinates to x,y,z based on face direction and layer
            int x, y, z;
            
            switch (face) {
                case FACE_TOP:
                    x = u; y = layer; z = v;
                    break;
                case FACE_BOTTOM:
                    x = u; y = layer; z = v;
                    break;
                case FACE_FRONT:
                    x = u; y = v; z = layer;
                    break;
                case FACE_BACK:
                    x = u; y = v; z = layer;
                    break;
                case FACE_RIGHT:
                    x = layer; y = v; z = u;
                    break;
                case FACE_LEFT:
                    x = layer; y = v; z = u;
                    break;
                default:
                    continue;
            }
            
            // Check if we should render this face
            if (IsValidPosition(x, y, z) && IsFaceVisible(x, y, z, face, world)) {
                VoxelType voxelType = voxels[x][y][z].type;
                std::string textureName = textureManager ? 
                    textureManager->GetTextureNameForVoxel(voxelType, face) : "stone";
                
                mask[u][v] = FaceMask(true, voxelType, textureName);
            }
        }
    }
}

void VoxelChunk::GreedyMeshFace(FaceDirection face, int layer, FaceMask mask[CHUNK_SIZE][CHUNK_SIZE], 
                                std::vector<QuadMesh>& quads) {
    // Create a copy of the mask to mark processed areas
    bool processed[CHUNK_SIZE][CHUNK_SIZE] = {false};
    
    for (int u = 0; u < CHUNK_SIZE; u++) {
        for (int v = 0; v < CHUNK_SIZE; v++) {
            if (processed[u][v] || !mask[u][v].visible) continue;
            
            // Find the width of the quad (extend in u direction)
            int width = 1;
            while (u + width < CHUNK_SIZE && 
                   !processed[u + width][v] && 
                   mask[u + width][v].visible &&
                   mask[u + width][v] == mask[u][v]) {
                width++;
            }
            
            // Find the height of the quad (extend in v direction)
            int height = 1;
            bool canExtend = true;
            while (v + height < CHUNK_SIZE && canExtend) {
                // Check if the entire row can be extended
                for (int i = 0; i < width; i++) {
                    if (processed[u + i][v + height] || 
                        !mask[u + i][v + height].visible ||
                        !(mask[u + i][v + height] == mask[u][v])) {
                        canExtend = false;
                        break;
                    }
                }
                if (canExtend) height++;
            }
            
            // Mark the quad area as processed
            for (int i = 0; i < width; i++) {
                for (int j = 0; j < height; j++) {
                    processed[u + i][v + j] = true;
                }
            }
            
            // Create the quad with the position of the first voxel
            Vector3 startPosition;
            
            // Convert u,v coordinates and layer back to world position
            switch (face) {
                case FACE_TOP:
                    startPosition = GetWorldPosition(u, layer, v);
                    break;
                case FACE_BOTTOM:
                    startPosition = GetWorldPosition(u, layer, v);
                    break;
                case FACE_FRONT:
                    startPosition = GetWorldPosition(u, v, layer);
                    break;
                case FACE_BACK:
                    startPosition = GetWorldPosition(u, v, layer);
                    break;
                case FACE_RIGHT:
                    startPosition = GetWorldPosition(layer, v, u);
                    break;
                case FACE_LEFT:
                    startPosition = GetWorldPosition(layer, v, u);
                    break;
            }
            
            quads.emplace_back(startPosition, width, height, face, mask[u][v].textureName);
        }
    }
}

void VoxelChunk::AddQuadToMesh(std::vector<Vector3>& vertices, std::vector<Vector3>& normals, 
                               std::vector<Vector2>& texcoords, std::vector<Color>& colors,
                               const QuadMesh& quad) const {
    // Instead of creating custom vertex logic, use the original AddFaceToMesh for each voxel position
    // This preserves the exact same rotation and positioning as the working single-face system
    
    for (int w = 0; w < quad.width; w++) {
        for (int h = 0; h < quad.height; h++) {
            Vector3 voxelPosition = quad.startPosition;
            
            // Calculate the position offset for this voxel in the quad
            switch (quad.face) {
                case FACE_TOP:
                case FACE_BOTTOM:
                    // For top/bottom faces: width extends in X, height extends in Z
                    voxelPosition.x += w;
                    voxelPosition.z += h;
                    break;
                case FACE_FRONT:
                case FACE_BACK:
                    // For front/back faces: width extends in X, height extends in Y
                    voxelPosition.x += w;
                    voxelPosition.y += h;
                    break;
                case FACE_RIGHT:
                case FACE_LEFT:
                    // For left/right faces: width extends in Z, height extends in Y
                    voxelPosition.z += w;
                    voxelPosition.y += h;
                    break;
            }
            
            // Use the original AddFaceToMesh function - this keeps rotation/positioning identical
            AddFaceToMesh(vertices, normals, texcoords, colors, voxelPosition, quad.face);
        }
    }
}

