#pragma once

#include <cstdint>
#include <unordered_map>

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

    const uint8_t MAX_DEPTH = 9;

    bool recursiveGenerate(uint8_t curr_child, uint32_t loc, uint8_t depth);

    bool checkChildren(const uint32_t total_loc_code, const uint8_t depth);

    Node root;

    std::unordered_map<uint32_t, Node> nodes;
};