#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>

typedef uint64_t LocCode;

class VoxelOctree
{
  public:
    VoxelOctree();

    void printInfo();

  private:
    // https://geidav.wordpress.com/2014/08/18/advanced-octrees-2-node-representations/
    struct Node
    {
        uint8_t child_exits = 0;
        //uint32_t loc_code;
    };

    const uint8_t MAX_DEPTH = 6;

    //  first bit (1) = some grandchild has voxel
    // second bit (2) = some grandchild has empty space
    uint8_t recursiveGenerate(uint8_t curr_child, LocCode loc, uint8_t depth,
                              uint8_t map_index);
    uint8_t checkChildren(const LocCode total_loc_code, const uint8_t depth,
                          uint8_t map_index);
    void startGeneration();

    glm::vec3 calcPos(uint32_t loc_code);

    void createIndirectTexture();
    void recursiveCreateIndirect(glm::ivec3 my_cell, LocCode parent_loc,
                                 uint8_t depth);

    size_t textureIndex(glm::ivec3 cell);
    glm::ivec3 nextCell(glm::ivec3 i, int offset = 1);
    glm::ivec3 nextCellNoWrap(glm::ivec3 i, int offset = 1);

	bool isVoxel(glm::vec3 pos);

	bool noise(glm::vec3 pos);

    Node root;
    std::unordered_map<LocCode, Node> gen_maps[8];

    std::unordered_map<LocCode, Node> nodes;

    std::vector<uint8_t> indirect_texture;
    size_t tex_side_length = 0;
	size_t cells_side_length = 0;

    glm::ivec3 next_free_cell{ 0 };

    long num_voxels[8];
    long num_checked[8];
};