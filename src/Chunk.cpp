#include "Chunk.h"
#include "rlgl.h"
#include <algorithm>
#include <cstring>

Chunk::Chunk(ChunkCoord coord) 
    : coord(coord), isGenerated(false), isDirty(false), isLoaded(true), 
      meshDirty(true), visibleFaces(0), greedyMeshDirty(true) {
    // Initialize all blocks to air
    blocks.fill(BlockType::AIR);
}

BlockType Chunk::GetBlock(const BlockPos& pos) const {
    return GetBlock(pos.x, pos.y, pos.z);
}

BlockType Chunk::GetBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return BlockType::AIR; // Out of bounds
    }
    
    int index = x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE;
    return blocks[index];
}

void Chunk::SetBlock(const BlockPos& pos, BlockType blockType) {
    SetBlock(pos.x, pos.y, pos.z, blockType);
}

void Chunk::SetBlock(int x, int y, int z, BlockType blockType) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return; // Out of bounds
    }
    
    int index = x + z * CHUNK_SIZE + y * CHUNK_SIZE * CHUNK_SIZE;
    if (blocks[index] != blockType) {
        blocks[index] = blockType;
        isDirty = true;
        MarkMeshDirty();
    }
}

void Chunk::Fill(BlockType blockType) {
    blocks.fill(blockType);
    isDirty = true;
    MarkMeshDirty();
}

int Chunk::GetHeightAt(int x, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return 0;
    }
    
    // Search from top down for first non-air block
    for (int y = CHUNK_HEIGHT - 1; y >= 0; y--) {
        if (GetBlock(x, y, z) != BlockType::AIR) {
            return y;
        }
    }
    return 0; // All air
}

bool Chunk::ShouldRenderFace(int x, int y, int z, BlockFace face, const Chunk* neighbors[4]) const {
    BlockType currentBlock = GetBlock(x, y, z);
    if (currentBlock == BlockType::AIR) {
        return false; // Don't render air blocks
    }
    
    // Check adjacent block based on face direction
    int adjX = x, adjY = y, adjZ = z;
    bool isEdgeFace = false;
    
    switch (face) {
        case BlockFace::TOP:
            adjY++;
            break;
        case BlockFace::BOTTOM:
            adjY--;
            break;
        case BlockFace::NORTH:
            adjZ--;
            if (adjZ < 0) isEdgeFace = true;
            break;
        case BlockFace::SOUTH:
            adjZ++;
            if (adjZ >= CHUNK_SIZE) isEdgeFace = true;
            break;
        case BlockFace::EAST:
            adjX++;
            if (adjX >= CHUNK_SIZE) isEdgeFace = true;
            break;
        case BlockFace::WEST:
            adjX--;
            if (adjX < 0) isEdgeFace = true;
            break;
        case BlockFace::ALL:
            return true; // Always render if using ALL face
    }
    
    // Handle edge faces - only render if no neighbor or neighbor is transparent
    if (isEdgeFace && neighbors == nullptr) {
        return true; // Render edge faces when no neighbor info available
    }
    
    // Check if adjacent position is within this chunk
    if (adjX >= 0 && adjX < CHUNK_SIZE && adjY >= 0 && adjY < CHUNK_HEIGHT && adjZ >= 0 && adjZ < CHUNK_SIZE) {
        BlockType adjacentBlock = GetBlock(adjX, adjY, adjZ);
        // Render face if adjacent block is air OR if adjacent block is different and current block might be transparent
        return adjacentBlock == BlockType::AIR || adjacentBlock != currentBlock;
    }
    
    // Handle edge cases (chunk boundaries)
    if (adjY < 0) {
        return false; // Don't render bottom faces at world bottom (bedrock level)
    }
    if (adjY >= CHUNK_HEIGHT) {
        return true; // Always render top faces at world top
    }
    
    // For chunk edge faces, check neighboring chunks if available
    if (isEdgeFace && neighbors != nullptr) {
        int neighborIndex = -1;
        int neighborX = x, neighborZ = z;
        
        switch (face) {
            case BlockFace::NORTH: // Going to negative Z
                neighborIndex = 0; // North neighbor
                neighborZ = CHUNK_SIZE - 1; // Map to the far side of north neighbor
                break;
            case BlockFace::SOUTH: // Going to positive Z
                neighborIndex = 1; // South neighbor
                neighborZ = 0; // Map to the near side of south neighbor
                break;
            case BlockFace::EAST: // Going to positive X
                neighborIndex = 2; // East neighbor
                neighborX = 0; // Map to the near side of east neighbor
                break;
            case BlockFace::WEST: // Going to negative X
                neighborIndex = 3; // West neighbor
                neighborX = CHUNK_SIZE - 1; // Map to the far side of west neighbor
                break;
            default:
                break;
        }
        
        if (neighborIndex >= 0 && neighbors[neighborIndex] != nullptr) {
            BlockType neighborBlock = neighbors[neighborIndex]->GetBlock(neighborX, adjY, neighborZ);
            // Render face if neighbor block is air OR different from current block
            return neighborBlock == BlockType::AIR || neighborBlock != currentBlock;
        }
    }
    
    // Default: render face if at chunk boundary and no neighbor info
    return true;
}

void Chunk::UpdateMesh() const {
    if (!meshDirty) {
        return;
    }
    
    vertices.clear();
    uvs.clear();
    indices.clear();
    visibleFaces = 0;
    
    // Pre-allocate vectors to reduce memory allocations (performance optimization)
    vertices.reserve(65536); // Worst case estimate
    uvs.reserve(65536);
    indices.reserve(98304); // 1.5x vertices for triangulation
    
    // Cache world origin calculation
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    // Generate mesh for all non-air blocks (optimized iteration order for cache efficiency)
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType blockType = GetBlock(x, y, z);
                if (blockType != BlockType::AIR) {
                    GenerateBlockMesh(x, y, z, vertices, uvs, indices, worldOrigin);
                }
            }
        }
    }
    
    meshDirty = false;
}

void Chunk::GenerateBlockMesh(int x, int y, int z, std::vector<Vector3>& verts, 
                             std::vector<Vector2>& uvCoords, std::vector<unsigned short>& inds,
                             const Vector3& worldOrigin) const {
    Vector3 blockPos = {worldOrigin.x + x, worldOrigin.y + y, worldOrigin.z + z};
    
    // Define faces to check (static array for better performance)
    static const BlockFace faces[] = {BlockFace::TOP, BlockFace::BOTTOM, BlockFace::NORTH, 
                                     BlockFace::SOUTH, BlockFace::EAST, BlockFace::WEST};
    
    for (int i = 0; i < 6; i++) {
        BlockFace face = faces[i];
        if (ShouldRenderFace(x, y, z, face)) {
            unsigned short baseIndex = static_cast<unsigned short>(verts.size());
            
            // Add vertices for this face with consistent counter-clockwise winding order
            switch (face) {
                case BlockFace::TOP: {
                    // Top face: counter-clockwise when viewed from above
                    verts.push_back({blockPos.x - 0.5f, blockPos.y + 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f});
                    break;
                }
                case BlockFace::BOTTOM: {
                    // Bottom face: counter-clockwise when viewed from below
                    verts.push_back({blockPos.x - 0.5f, blockPos.y - 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y - 0.5f, blockPos.z - 0.5f});
                    break;
                }
                case BlockFace::NORTH: {
                    // North face (negative Z): counter-clockwise when viewed from outside
                    verts.push_back({blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y + 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y - 0.5f, blockPos.z - 0.5f});
                    break;
                }
                case BlockFace::SOUTH: {
                    // South face (positive Z): counter-clockwise when viewed from outside
                    verts.push_back({blockPos.x - 0.5f, blockPos.y - 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.z + 0.5f});
                    break;
                }
                case BlockFace::EAST: {
                    // East face (positive X): counter-clockwise when viewed from outside
                    verts.push_back({blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y + 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x + 0.5f, blockPos.y - 0.5f, blockPos.z - 0.5f});
                    break;
                }
                case BlockFace::WEST: {
                    // West face (negative X): counter-clockwise when viewed from outside
                    verts.push_back({blockPos.x - 0.5f, blockPos.y - 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y + 0.5f, blockPos.z - 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y + 0.5f, blockPos.z + 0.5f});
                    verts.push_back({blockPos.x - 0.5f, blockPos.y - 0.5f, blockPos.z + 0.5f});
                    break;
                }
                default:
                    continue;
            }
            
            // Add UV coordinates with proper orientation for each face
            switch (face) {
                case BlockFace::TOP:
                case BlockFace::BOTTOM:
                    // Standard UV mapping for horizontal faces
                    uvCoords.push_back({0.0f, 0.0f});
                    uvCoords.push_back({1.0f, 0.0f});
                    uvCoords.push_back({1.0f, 1.0f});
                    uvCoords.push_back({0.0f, 1.0f});
                    break;
                case BlockFace::NORTH:
                case BlockFace::SOUTH:
                case BlockFace::EAST:
                case BlockFace::WEST:
                    // Standard UV mapping for vertical faces
                    uvCoords.push_back({0.0f, 1.0f});
                    uvCoords.push_back({1.0f, 1.0f});
                    uvCoords.push_back({1.0f, 0.0f});
                    uvCoords.push_back({0.0f, 0.0f});
                    break;
                default:
                    // Fallback UV coordinates
                    uvCoords.push_back({0.0f, 0.0f});
                    uvCoords.push_back({1.0f, 0.0f});
                    uvCoords.push_back({1.0f, 1.0f});
                    uvCoords.push_back({0.0f, 1.0f});
                    break;
            }
            
            // Add indices for two triangles (quad)
            inds.push_back(baseIndex + 0);
            inds.push_back(baseIndex + 1);
            inds.push_back(baseIndex + 2);
            
            inds.push_back(baseIndex + 0);
            inds.push_back(baseIndex + 2);
            inds.push_back(baseIndex + 3);
            
            visibleFaces++;
        }
    }
}

void Chunk::RenderOptimized(TextureManager& textureManager, const Chunk* neighbors[4]) const {
    if (!isLoaded) {
        return;
    }
    
    // Generate greedy mesh if dirty
    if (greedyMeshDirty) {
        GenerateGreedyMesh(neighbors);
    }
    
    // Render optimized quads
    for (const auto& quad : optimizedQuads) {
        RenderQuad(quad, textureManager);
    }
}

void Chunk::GenerateGreedyMesh(const Chunk* neighbors[4]) const {
    if (!greedyMeshDirty) {
        return;
    }
    
    optimizedQuads.clear();
    
    // For now, implement naive greedy meshing - we'll optimize further later
    // Generate quads for each face direction
    BlockFace faces[] = {BlockFace::TOP, BlockFace::BOTTOM, BlockFace::NORTH, 
                        BlockFace::SOUTH, BlockFace::EAST, BlockFace::WEST};
    
    for (BlockFace face : faces) {
        GenerateQuadsForFace(face, neighbors);
    }
    
    greedyMeshDirty = false;
}

void Chunk::GenerateQuadsForFace(BlockFace face, const Chunk* neighbors[4]) const {
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    // Create a 2D mask for this face direction
    bool processed[CHUNK_SIZE][CHUNK_SIZE]; // For horizontal faces, use X and Z
    bool processedVert[CHUNK_SIZE][CHUNK_HEIGHT]; // For vertical faces, use X/Z and Y
    
    // Initialize the appropriate mask
    if (face == BlockFace::TOP || face == BlockFace::BOTTOM) {
        for (int i = 0; i < CHUNK_SIZE; i++) {
            for (int j = 0; j < CHUNK_SIZE; j++) {
                processed[i][j] = false;
            }
        }
    } else {
        for (int i = 0; i < CHUNK_SIZE; i++) {
            for (int j = 0; j < CHUNK_HEIGHT; j++) {
                processedVert[i][j] = false;
            }
        }
    }
    
    // Handle horizontal faces (TOP/BOTTOM) differently from vertical faces
    if (face == BlockFace::TOP || face == BlockFace::BOTTOM) {
        // For horizontal faces, iterate through X and Z
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                if (processed[x][z]) continue;
                
                // Find the appropriate Y level for this face
                int y = -1;
                if (face == BlockFace::TOP) {
                    // For top faces, find the highest block at this x,z position
                    for (int testY = CHUNK_HEIGHT - 1; testY >= 0; testY--) {
                        if (GetBlock(x, testY, z) != BlockType::AIR && 
                            ShouldRenderFace(x, testY, z, face, neighbors)) {
                            y = testY;
                            break;
                        }
                    }
                } else { // BOTTOM face
                    // For bottom faces, find the lowest block at this x,z position
                    for (int testY = 0; testY < CHUNK_HEIGHT; testY++) {
                        if (GetBlock(x, testY, z) != BlockType::AIR && 
                            ShouldRenderFace(x, testY, z, face, neighbors)) {
                            y = testY;
                            break;
                        }
                    }
                }
                
                if (y == -1) {
                    processed[x][z] = true;
                    continue; // No block found at this x,z
                }
                
                BlockType blockType = GetBlock(x, y, z);
                if (blockType == BlockType::AIR) {
                    processed[x][z] = true;
                    continue;
                }
                
                // Start a new quad - expand in X direction first
                int width = 1;
                while (x + width < CHUNK_SIZE && !processed[x + width][z]) {
                    // Check if we can extend the quad
                    int testY = -1;
                    if (face == BlockFace::TOP) {
                        for (int checkY = CHUNK_HEIGHT - 1; checkY >= 0; checkY--) {
                            if (GetBlock(x + width, checkY, z) != BlockType::AIR && 
                                ShouldRenderFace(x + width, checkY, z, face, neighbors)) {
                                testY = checkY;
                                break;
                            }
                        }
                    } else {
                        for (int checkY = 0; checkY < CHUNK_HEIGHT; checkY++) {
                            if (GetBlock(x + width, checkY, z) != BlockType::AIR && 
                                ShouldRenderFace(x + width, checkY, z, face, neighbors)) {
                                testY = checkY;
                                break;
                            }
                        }
                    }
                    
                    if (testY == y && GetBlock(x + width, y, z) == blockType) {
                        width++;
                    } else {
                        break;
                    }
                }
                
                // Now try to expand in Z direction
                int depth = 1;
                bool canExpandZ = true;
                while (z + depth < CHUNK_SIZE && canExpandZ) {
                    // Check the entire width for this Z level
                    for (int checkX = x; checkX < x + width; checkX++) {
                        if (processed[checkX][z + depth]) {
                            canExpandZ = false;
                            break;
                        }
                        
                        int testY = -1;
                        if (face == BlockFace::TOP) {
                            for (int checkY = CHUNK_HEIGHT - 1; checkY >= 0; checkY--) {
                                if (GetBlock(checkX, checkY, z + depth) != BlockType::AIR && 
                                    ShouldRenderFace(checkX, checkY, z + depth, face, neighbors)) {
                                    testY = checkY;
                                    break;
                                }
                            }
                        } else {
                            for (int checkY = 0; checkY < CHUNK_HEIGHT; checkY++) {
                                if (GetBlock(checkX, checkY, z + depth) != BlockType::AIR && 
                                    ShouldRenderFace(checkX, checkY, z + depth, face, neighbors)) {
                                    testY = checkY;
                                    break;
                                }
                            }
                        }
                        
                        if (testY != y || GetBlock(checkX, y, z + depth) != blockType) {
                            canExpandZ = false;
                            break;
                        }
                    }
                    
                    if (canExpandZ) {
                        depth++;
                    }
                }
                
                // Create the quad - position should be at the center of the merged area
                Vector3 quadPos = {
                    worldOrigin.x + x + width * 0.5f - 0.5f,  // Center in X
                    worldOrigin.y + y, 
                    worldOrigin.z + z + depth * 0.5f - 0.5f   // Center in Z
                };
                
                Vector3 quadSize = {static_cast<float>(width), 1.0f, static_cast<float>(depth)};
                optimizedQuads.emplace_back(quadPos, quadSize, blockType, face);
                
                // Mark processed area
                for (int w = 0; w < width; w++) {
                    for (int d = 0; d < depth; d++) {
                        processed[x + w][z + d] = true;
                    }
                }
            }
        }
    } else {
        // Handle vertical faces (NORTH, SOUTH, EAST, WEST) - existing logic
        for (int i = 0; i < CHUNK_SIZE; i++) {
            for (int j = 0; j < CHUNK_HEIGHT; j++) {
                if (processedVert[i][j]) continue;
                
                // Convert 2D coordinates to 3D based on face direction
                int x, y, z;
                switch (face) {
                    case BlockFace::NORTH:
                        x = i; y = j; z = 0;
                        break;
                    case BlockFace::SOUTH:
                        x = i; y = j; z = CHUNK_SIZE - 1;
                        break;
                    case BlockFace::EAST:
                        x = CHUNK_SIZE - 1; y = j; z = i;
                        break;
                    case BlockFace::WEST:
                        x = 0; y = j; z = i;
                        break;
                    default:
                        continue;
                }
                
                if (y >= CHUNK_HEIGHT) continue;
                
                BlockType blockType = GetBlock(x, y, z);
                if (blockType == BlockType::AIR || !ShouldRenderFace(x, y, z, face, neighbors)) {
                    processedVert[i][j] = true;
                    continue;
                }
                
                // Start a new quad - expand horizontally first
                int width = 1;
                while (i + width < CHUNK_SIZE && !processedVert[i + width][j]) {
                    int testX, testY, testZ;
                    switch (face) {
                        case BlockFace::NORTH:
                        case BlockFace::SOUTH:
                            testX = i + width; testY = j; testZ = z;
                            break;
                        case BlockFace::EAST:
                        case BlockFace::WEST:
                            testX = x; testY = j; testZ = i + width;
                            break;
                        default:
                            testX = testY = testZ = 0;
                            break;
                    }
                    
                    if (GetBlock(testX, testY, testZ) == blockType && 
                        ShouldRenderFace(testX, testY, testZ, face, neighbors)) {
                        width++;
                    } else {
                        break;
                    }
                }
                
                // Create the quad - position should be centered properly for merged quads
                Vector3 quadPos;
                Vector3 quadSize = {1.0f, 1.0f, 1.0f};
                
                // Adjust position and size based on face direction
                switch (face) {
                    case BlockFace::NORTH:
                    case BlockFace::SOUTH:
                        quadPos = {
                            worldOrigin.x + i + width * 0.5f - 0.5f,  // Center in X
                            worldOrigin.y + j, 
                            worldOrigin.z + z
                        };
                        quadSize.x = static_cast<float>(width);
                        break;
                    case BlockFace::EAST:
                    case BlockFace::WEST:
                        quadPos = {
                            worldOrigin.x + x,
                            worldOrigin.y + j, 
                            worldOrigin.z + i + width * 0.5f - 0.5f   // Center in Z
                        };
                        quadSize.z = static_cast<float>(width);
                        break;
                    default:
                        quadPos = {
                            worldOrigin.x + x,
                            worldOrigin.y + y, 
                            worldOrigin.z + z
                        };
                        break;
                }
                
                optimizedQuads.emplace_back(quadPos, quadSize, blockType, face);
                
                // Mark processed area
                for (int w = 0; w < width; w++) {
                    processedVert[i + w][j] = true;
                }
            }
        }
    }
}

void Chunk::RenderQuad(const QuadMesh& quad, TextureManager& textureManager) const {
    // Get appropriate texture for this block type and face
    Texture2D texture = textureManager.GetBlockTexture(quad.blockType, quad.face);
    
    if (texture.id == 0) {
        // Fallback to colored cube if no texture
        Color blockColor = MAGENTA;
        switch (quad.blockType) {
            case BlockType::DIRT: blockColor = BROWN; break;
            case BlockType::GRASS: blockColor = GREEN; break;
            case BlockType::STONE: blockColor = GRAY; break;
            case BlockType::WOOD: blockColor = MAROON; break;
            case BlockType::COBBLESTONE: blockColor = DARKGRAY; break;
            default: blockColor = MAGENTA; break;
        }
        DrawCube(quad.position, quad.size.x, quad.size.y, quad.size.z, blockColor);
        return;
    }
    
    // Render textured quad based on face direction
    float halfX = quad.size.x * 0.5f;
    float halfY = quad.size.y * 0.5f;
    float halfZ = quad.size.z * 0.5f;
    
    // Calculate UV coordinates based on quad size to prevent stretching
    // UV coordinates should tile the texture based on the size of the merged quad
    float uMax, vMax;
    
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    
    switch (quad.face) {
        case BlockFace::TOP:
            rlNormal3f(0.0f, 1.0f, 0.0f);
            // TOP face - UV tiling based on quad size to prevent stretching
            uMax = quad.size.x; // Tile in X direction
            vMax = quad.size.z; // Tile in Z direction
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(quad.position.x - halfX, quad.position.y + halfY, quad.position.z - halfZ);  // Vertex 0
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(quad.position.x + halfX, quad.position.y + halfY, quad.position.z - halfZ);  // Vertex 1
            rlTexCoord2f(uMax, vMax); rlVertex3f(quad.position.x + halfX, quad.position.y + halfY, quad.position.z + halfZ);  // Vertex 2
            rlTexCoord2f(0.0f, vMax); rlVertex3f(quad.position.x - halfX, quad.position.y + halfY, quad.position.z + halfZ);  // Vertex 3
            break;
            
        case BlockFace::BOTTOM:
            rlNormal3f(0.0f, -1.0f, 0.0f);
            // BOTTOM face - UV tiling based on quad size to prevent stretching
            uMax = quad.size.x; // Tile in X direction  
            vMax = quad.size.z; // Tile in Z direction
            rlTexCoord2f(0.0f, vMax); rlVertex3f(quad.position.x - halfX, quad.position.y - halfY, quad.position.z + halfZ);  // Vertex 0
            rlTexCoord2f(uMax, vMax); rlVertex3f(quad.position.x + halfX, quad.position.y - halfY, quad.position.z + halfZ);  // Vertex 1
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(quad.position.x + halfX, quad.position.y - halfY, quad.position.z - halfZ);  // Vertex 2
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(quad.position.x - halfX, quad.position.y - halfY, quad.position.z - halfZ);  // Vertex 3
            break;
            
        case BlockFace::NORTH:
            rlNormal3f(0.0f, 0.0f, -1.0f);
            // NORTH face - UV tiling based on quad size to prevent stretching
            uMax = quad.size.x; // Tile in X direction
            vMax = quad.size.y; // Tile in Y direction
            rlTexCoord2f(0.0f, vMax); rlVertex3f(quad.position.x + halfX, quad.position.y - halfY, quad.position.z - halfZ);  // Vertex 0
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(quad.position.x + halfX, quad.position.y + halfY, quad.position.z - halfZ);  // Vertex 1  
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(quad.position.x - halfX, quad.position.y + halfY, quad.position.z - halfZ);  // Vertex 2
            rlTexCoord2f(uMax, vMax); rlVertex3f(quad.position.x - halfX, quad.position.y - halfY, quad.position.z - halfZ);  // Vertex 3
            break;
            
        case BlockFace::SOUTH:
            rlNormal3f(0.0f, 0.0f, 1.0f);
            // SOUTH face - UV tiling based on quad size to prevent stretching
            uMax = quad.size.x; // Tile in X direction
            vMax = quad.size.y; // Tile in Y direction
            rlTexCoord2f(uMax, vMax); rlVertex3f(quad.position.x - halfX, quad.position.y - halfY, quad.position.z + halfZ);  // Vertex 0
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(quad.position.x - halfX, quad.position.y + halfY, quad.position.z + halfZ);  // Vertex 1
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(quad.position.x + halfX, quad.position.y + halfY, quad.position.z + halfZ);  // Vertex 2
            rlTexCoord2f(0.0f, vMax); rlVertex3f(quad.position.x + halfX, quad.position.y - halfY, quad.position.z + halfZ);  // Vertex 3
            break;
            
        case BlockFace::EAST:
            rlNormal3f(1.0f, 0.0f, 0.0f);
            // EAST face - UV tiling based on quad size to prevent stretching
            uMax = quad.size.z; // Tile in Z direction
            vMax = quad.size.y; // Tile in Y direction
            rlTexCoord2f(0.0f, vMax); rlVertex3f(quad.position.x + halfX, quad.position.y - halfY, quad.position.z + halfZ);  // Vertex 0
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(quad.position.x + halfX, quad.position.y + halfY, quad.position.z + halfZ);  // Vertex 1
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(quad.position.x + halfX, quad.position.y + halfY, quad.position.z - halfZ);  // Vertex 2
            rlTexCoord2f(uMax, vMax); rlVertex3f(quad.position.x + halfX, quad.position.y - halfY, quad.position.z - halfZ);  // Vertex 3
            break;
            
        case BlockFace::WEST:
            rlNormal3f(-1.0f, 0.0f, 0.0f);
            // WEST face - UV tiling based on quad size to prevent stretching
            uMax = quad.size.z; // Tile in Z direction
            vMax = quad.size.y; // Tile in Y direction
            rlTexCoord2f(uMax, vMax); rlVertex3f(quad.position.x - halfX, quad.position.y - halfY, quad.position.z - halfZ);  // Vertex 0
            rlTexCoord2f(uMax, 0.0f); rlVertex3f(quad.position.x - halfX, quad.position.y + halfY, quad.position.z - halfZ);  // Vertex 1
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(quad.position.x - halfX, quad.position.y + halfY, quad.position.z + halfZ);  // Vertex 2
            rlTexCoord2f(0.0f, vMax); rlVertex3f(quad.position.x - halfX, quad.position.y - halfY, quad.position.z + halfZ);  // Vertex 3
            break;
            
        default:
            break;
    }
    
    rlEnd();
    rlSetTexture(0);
}

void Chunk::RenderSingleBlock(const Vector3& blockPos, BlockType blockType, TextureManager& textureManager) const {
    // Get texture for this block type
    Texture2D texture = textureManager.GetBlockTexture(blockType, BlockFace::ALL);
    
    if (texture.id == 0) {
        // Fallback to colored cube if no texture
        Color blockColor = MAGENTA;
        switch (blockType) {
            case BlockType::DIRT: blockColor = BROWN; break;
            case BlockType::GRASS: blockColor = GREEN; break;
            case BlockType::STONE: blockColor = GRAY; break;
            case BlockType::WOOD: blockColor = MAROON; break;
            case BlockType::COBBLESTONE: blockColor = DARKGRAY; break;
            default: blockColor = MAGENTA; break;
        }
        DrawCube(blockPos, 1.0f, 1.0f, 1.0f, blockColor);
        return;
    }
    
    // Render a simple textured cube
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    
    float x = blockPos.x;
    float y = blockPos.y;
    float z = blockPos.z;
    
    // Top face
    rlNormal3f(0.0f, 1.0f, 0.0f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + 0.5f, y + 0.5f, z + 0.5f);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - 0.5f, y + 0.5f, z + 0.5f);
    
    // Bottom face
    rlNormal3f(0.0f, -1.0f, 0.0f);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - 0.5f, y - 0.5f, z + 0.5f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + 0.5f, y - 0.5f, z + 0.5f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
    
    // North face
    rlNormal3f(0.0f, 0.0f, -1.0f);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
    
    // South face
    rlNormal3f(0.0f, 0.0f, 1.0f);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - 0.5f, y - 0.5f, z + 0.5f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - 0.5f, y + 0.5f, z + 0.5f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + 0.5f, y + 0.5f, z + 0.5f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + 0.5f, y - 0.5f, z + 0.5f);
    
    // East face
    rlNormal3f(1.0f, 0.0f, 0.0f);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x + 0.5f, y - 0.5f, z + 0.5f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + 0.5f, y + 0.5f, z + 0.5f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
    
    // West face
    rlNormal3f(-1.0f, 0.0f, 0.0f);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x - 0.5f, y + 0.5f, z + 0.5f);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - 0.5f, y - 0.5f, z + 0.5f);
    
    rlEnd();
    rlSetTexture(0);
}

void Chunk::Render(TextureManager& textureManager) const {
    // Use simple per-block rendering instead of optimized greedy meshing
    // This ensures correct texturing and positioning
    if (!isLoaded) {
        return;
    }
    
    Vector3 worldOrigin = coord.GetWorldOrigin();
    
    // Render each block individually - simple and reliable
    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockType blockType = GetBlock(x, y, z);
                if (blockType != BlockType::AIR) {
                    Vector3 blockPos = {worldOrigin.x + x, worldOrigin.y + y, worldOrigin.z + z};
                    RenderSingleBlock(blockPos, blockType, textureManager);
                }
            }
        }
    }
}

std::vector<unsigned char> Chunk::Serialize() const {
    std::vector<unsigned char> data;
    
    // Write header
    data.push_back('C'); // Chunk identifier
    data.push_back('H');
    data.push_back('K');
    data.push_back(1);   // Version
    
    // Write chunk coordinates
    data.resize(data.size() + sizeof(int) * 2);
    std::memcpy(data.data() + data.size() - sizeof(int) * 2, &coord.x, sizeof(int));
    std::memcpy(data.data() + data.size() - sizeof(int), &coord.z, sizeof(int));
    
    // Write block data (simple approach: 1 byte per block)
    data.reserve(data.size() + BLOCKS_PER_CHUNK);
    for (const auto& block : blocks) {
        data.push_back(static_cast<unsigned char>(block));
    }
    
    return data;
}

bool Chunk::Deserialize(const std::vector<unsigned char>& data) {
    if (data.size() < 8) { // Minimum header size
        return false;
    }
    
    // Check header
    if (data[0] != 'C' || data[1] != 'H' || data[2] != 'K' || data[3] != 1) {
        return false;
    }
    
    // Read coordinates
    int fileX, fileZ;
    std::memcpy(&fileX, data.data() + 4, sizeof(int));
    std::memcpy(&fileZ, data.data() + 8, sizeof(int));
    
    if (fileX != coord.x || fileZ != coord.z) {
        return false; // Coordinate mismatch
    }
    
    // Read block data
    if (data.size() < 12 + BLOCKS_PER_CHUNK) {
        return false;
    }
    
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++) {
        blocks[i] = static_cast<BlockType>(data[12 + i]);
    }
    
    isGenerated = true;
    isDirty = false;
    MarkMeshDirty();
    
    return true;
}
