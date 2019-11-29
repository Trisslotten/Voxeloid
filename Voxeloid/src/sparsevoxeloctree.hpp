#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>

typedef uint64_t LocCode;

class SparseVoxelOctree
{
  public:
    SparseVoxelOctree();

  private:
    // https://geidav.wordpress.com/2014/08/18/advanced-octrees-2-node-representations/
    struct Node
    {
        uint8_t child_exits = 0;
        //uint32_t loc_code;
    };

    const uint8_t MAX_DEPTH = 10;

    //  first bit (1) = some grandchild has voxel
    // second bit (2) = some grandchild has empty space
    uint8_t recursiveGenerate(uint8_t curr_child, LocCode loc, uint8_t depth, uint8_t map_index);
    uint8_t checkChildren(const LocCode total_loc_code, const uint8_t depth, uint8_t map_index);
	void startGeneration(const LocCode total_loc_code, const uint8_t depth);

    glm::vec3 calcPos(uint32_t loc_code);

    Node root;
    std::unordered_map<LocCode, Node> gen_maps[8];

	std::unordered_map<LocCode, Node> nodes;

    long num_voxels[8];
	long num_checked[8];
};