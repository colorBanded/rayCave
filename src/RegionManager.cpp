#include "RegionManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
#else
    #include <unistd.h>
#endif

RegionManager::RegionManager(const std::string& worldPath) : worldPath(worldPath) {
    // Ensure world path ends with a slash
    if (!worldPath.empty() && worldPath.back() != '/' && worldPath.back() != '\\') {
        this->worldPath += "/";
    }
}

std::string RegionManager::GetRegionFilePath(RegionCoord regionCoord) const {
    return worldPath + "region/" + regionCoord.ToString();
}

bool RegionManager::RegionFileExists(RegionCoord regionCoord) const {
    std::string filePath = GetRegionFilePath(regionCoord);
    struct stat st;
    return stat(filePath.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

void RegionManager::CreateDirectoryIfNeeded(const std::string& filePath) {
    size_t pos = filePath.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string directory = filePath.substr(0, pos);
        
        // Create directory recursively
        struct stat st;
        if (stat(directory.c_str(), &st) != 0) {
            // Directory doesn't exist, create it
            #ifdef _WIN32
                _mkdir(directory.c_str());
            #else
                // Create parent directories first (simple recursive approach)
                size_t parentPos = directory.find_last_of("/\\");
                if (parentPos != std::string::npos) {
                    std::string parent = directory.substr(0, parentPos);
                    CreateDirectoryIfNeeded(parent + "/dummy");
                }
                mkdir(directory.c_str(), 0755);
            #endif
        }
    }
}

RegionHeader* RegionManager::GetRegionHeader(RegionCoord regionCoord) {
    std::lock_guard<std::mutex> lock(regionMutex);
    
    // Check cache first
    auto it = headerCache.find(regionCoord);
    if (it != headerCache.end()) {
        return it->second.get();
    }
    
    // Load from file
    auto header = std::make_unique<RegionHeader>();
    if (LoadRegionHeader(regionCoord, *header)) {
        RegionHeader* headerPtr = header.get();
        headerCache[regionCoord] = std::move(header);
        return headerPtr;
    }
    
    // Create new header if file doesn't exist
    if (!RegionFileExists(regionCoord)) {
        RegionHeader* headerPtr = header.get();
        headerCache[regionCoord] = std::move(header);
        return headerPtr;
    }
    
    return nullptr;
}

bool RegionManager::LoadRegionHeader(RegionCoord regionCoord, RegionHeader& header) {
    std::string filePath = GetRegionFilePath(regionCoord);
    
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read header
    file.read(reinterpret_cast<char*>(&header), sizeof(RegionHeader));
    file.close();
    
    // Validate magic number
    if (header.magic != RegionHeader::MAGIC) {
        std::cout << "Invalid region file magic number: " << filePath << std::endl;
        return false;
    }
    
    // Check version compatibility
    if (header.version > RegionHeader::VERSION) {
        std::cout << "Region file version too new: " << filePath << " (version " << header.version << ")" << std::endl;
        return false;
    }
    
    return true;
}

bool RegionManager::SaveRegionHeader(RegionCoord regionCoord, const RegionHeader& header) {
    std::string filePath = GetRegionFilePath(regionCoord);
    
    CreateDirectoryIfNeeded(filePath);
    
    std::fstream file(filePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        // File doesn't exist, create it
        file.open(filePath, std::ios::binary | std::ios::out);
        if (!file.is_open()) {
            std::cout << "Failed to create region file: " << filePath << std::endl;
            return false;
        }
    }
    
    // Write header at the beginning of file
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&header), sizeof(RegionHeader));
    file.close();
    
    return true;
}

bool RegionManager::SaveChunk(const Chunk& chunk) {
    ChunkCoord chunkCoord = chunk.GetCoord();
    RegionCoord regionCoord = RegionCoord::FromChunkCoord(chunkCoord);
    
    int localX, localZ;
    RegionCoord::GetLocalChunkCoord(chunkCoord, localX, localZ);
    
    RegionHeader* header = GetRegionHeader(regionCoord);
    if (!header) {
        std::cout << "Failed to get region header for chunk (" << chunkCoord.x << ", " << chunkCoord.z << ")" << std::endl;
        return false;
    }
    
    // Serialize chunk data
    std::vector<unsigned char> chunkData = chunk.Serialize();
    if (chunkData.empty()) {
        std::cout << "Failed to serialize chunk (" << chunkCoord.x << ", " << chunkCoord.z << ")" << std::endl;
        return false;
    }
    
    std::string filePath = GetRegionFilePath(regionCoord);
    CreateDirectoryIfNeeded(filePath);
    
    // Open file for reading and writing
    std::fstream file(filePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        // File doesn't exist, create it
        file.open(filePath, std::ios::binary | std::ios::out);
        if (!file.is_open()) {
            std::cout << "Failed to open region file: " << filePath << std::endl;
            return false;
        }
        
        // Write initial header
        file.write(reinterpret_cast<const char*>(header), sizeof(RegionHeader));
    }
    
    int chunkIndex = header->GetChunkIndex(localX, localZ);
    
    // Find a suitable position to write the chunk
    // For simplicity, we'll append to the end of the file
    file.seekg(0, std::ios::end);
    uint32_t chunkOffset = static_cast<uint32_t>(file.tellg());
    
    // Write chunk data
    file.write(reinterpret_cast<const char*>(chunkData.data()), chunkData.size());
    
    // Update header
    header->chunkOffsets[chunkIndex] = chunkOffset;
    header->chunkSizes[chunkIndex] = static_cast<uint32_t>(chunkData.size());
    header->lastModified[chunkIndex] = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    
    // Write updated header
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(header), sizeof(RegionHeader));
    file.close();
    
    return true;
}

bool RegionManager::LoadChunk(Chunk& chunk) {
    ChunkCoord chunkCoord = chunk.GetCoord();
    RegionCoord regionCoord = RegionCoord::FromChunkCoord(chunkCoord);
    
    int localX, localZ;
    RegionCoord::GetLocalChunkCoord(chunkCoord, localX, localZ);
    
    RegionHeader* header = GetRegionHeader(regionCoord);
    if (!header) {
        return false;
    }
    
    int chunkIndex = header->GetChunkIndex(localX, localZ);
    
    // Check if chunk exists in this region
    if (header->chunkOffsets[chunkIndex] == 0 || header->chunkSizes[chunkIndex] == 0) {
        return false; // Chunk doesn't exist
    }
    
    std::string filePath = GetRegionFilePath(regionCoord);
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Seek to chunk data
    file.seekg(header->chunkOffsets[chunkIndex]);
    
    // Read chunk data
    std::vector<unsigned char> chunkData(header->chunkSizes[chunkIndex]);
    file.read(reinterpret_cast<char*>(chunkData.data()), header->chunkSizes[chunkIndex]);
    file.close();
    
    // Deserialize chunk
    return chunk.Deserialize(chunkData);
}

bool RegionManager::ChunkExists(ChunkCoord chunkCoord) {
    RegionCoord regionCoord = RegionCoord::FromChunkCoord(chunkCoord);
    
    if (!RegionFileExists(regionCoord)) {
        return false;
    }
    
    int localX, localZ;
    RegionCoord::GetLocalChunkCoord(chunkCoord, localX, localZ);
    
    RegionHeader* header = GetRegionHeader(regionCoord);
    if (!header) {
        return false;
    }
    
    int chunkIndex = header->GetChunkIndex(localX, localZ);
    return header->chunkOffsets[chunkIndex] != 0 && header->chunkSizes[chunkIndex] != 0;
}

bool RegionManager::DeleteChunk(ChunkCoord chunkCoord) {
    RegionCoord regionCoord = RegionCoord::FromChunkCoord(chunkCoord);
    
    int localX, localZ;
    RegionCoord::GetLocalChunkCoord(chunkCoord, localX, localZ);
    
    RegionHeader* header = GetRegionHeader(regionCoord);
    if (!header) {
        return false;
    }
    
    int chunkIndex = header->GetChunkIndex(localX, localZ);
    
    // Mark chunk as deleted
    header->chunkOffsets[chunkIndex] = 0;
    header->chunkSizes[chunkIndex] = 0;
    header->lastModified[chunkIndex] = 0;
    
    // Save updated header
    return SaveRegionHeader(regionCoord, *header);
}

void RegionManager::CompactRegion(RegionCoord regionCoord) {
    // This is a complex operation that would defragment the region file
    // For now, we'll leave it as a placeholder
    // In a full implementation, this would:
    // 1. Read all valid chunks from the region
    // 2. Rewrite the region file with chunks packed together
    // 3. Update all offsets in the header
    std::cout << "Region compaction not yet implemented for " << regionCoord.ToString() << std::endl;
}

size_t RegionManager::GetRegionFileSize(RegionCoord regionCoord) {
    std::string filePath = GetRegionFilePath(regionCoord);
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0) {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

int RegionManager::GetChunkCountInRegion(RegionCoord regionCoord) {
    RegionHeader* header = GetRegionHeader(regionCoord);
    if (!header) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < REGION_SIZE * REGION_SIZE; i++) {
        if (header->chunkOffsets[i] != 0 && header->chunkSizes[i] != 0) {
            count++;
        }
    }
    return count;
}

void RegionManager::ClearCache() {
    std::lock_guard<std::mutex> lock(regionMutex);
    headerCache.clear();
}
