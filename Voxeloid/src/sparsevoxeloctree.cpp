#include "sparsevoxeloctree.hpp"

#include <glm/gtc/noise.hpp>
#include <iostream>

glm::vec3 calcPos(uint32_t loc_code, uint8_t depth)
{
    float size = 1.f;
    glm::vec3 result{ 0 };
    for (int i = depth - 1; i >= 0; i--)
    {
        uint32_t curr_loc = loc_code >> (i * 3);

        // can probably be a for-loop
        if (curr_loc & 1U)
            result.x += size;
        else
            result.x -= size;

        if (curr_loc & 2U)
            result.y += size;
        else
            result.y -= size;

        if (curr_loc & 4U)
            result.z += size;
        else
            result.z -= size;

        size /= 2.f;
    }
	
	return result;
}

SparseVoxelOctree::SparseVoxelOctree()
{
    //
    checkChildren(1, 0);

	size_t size = nodes.size();
    std::cout << size << " Bytes\n";
	std::cout << size/1024U << " KB\n";
	std::cout << size/1048576U << " MB\n";
	std::cout << size/1073741824U<< " GB\n";
	std::cout<< "map: " << sizeof(nodes) << " Bytes\n";
}

// https://stackoverflow.com/questions/994593/how-to-do-an-integer-log2-in-c
uint32_t ilog2(uint32_t n)
{
	uint32_t targetlevel = 0;
	while (n >>= 1) ++targetlevel;
	return targetlevel;
}

bool SparseVoxelOctree::recursiveGenerate(uint8_t curr_child,
                                          uint32_t loc_code,
                                          uint8_t depth)
{
    uint32_t curr_loc_code = ilog2(curr_child & -curr_child) + 1;
    uint32_t total_loc_code = (loc_code << 3) | curr_loc_code;

    if (depth >= MAX_DEPTH)
    {
        glm::vec3 pos = calcPos(loc_code, depth);

        float val = glm::perlin(pos);
        if (val > 0.5)
        {
            nodes[total_loc_code] = { 0 };
			return true;
        }
		else
		{
			return false;
		}
    }
    else
    {
        return checkChildren(total_loc_code, depth);
    }
}

bool SparseVoxelOctree::checkChildren(const uint32_t total_loc_code,
                                      const uint8_t depth)
{
    Node to_add;
    to_add.child_exits = 0;
    //to_add.loc_code = (loc_code << 3) | curr_loc_code;

    for (int i = 0; i < 8; i++)
    {
        uint8_t child_bit = 1 << i;
        if (recursiveGenerate(child_bit, total_loc_code, depth + 1))
        {
            to_add.child_exits |= child_bit;
        }
    }
    if (to_add.child_exits != 0)
    {
        nodes[total_loc_code] = to_add;
        return true;
    }
    else
    {
        return false;
    }
}
