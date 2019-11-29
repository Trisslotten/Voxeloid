#include "sparsevoxeloctree.hpp"

#include <future>
#include <glm/gtc/noise.hpp>
#include <iostream>

glm::vec3 SparseVoxelOctree::calcPos(uint32_t loc_code)
{
    float offset = 1.f;
    glm::vec3 result{ 0 };
    for (int i = MAX_DEPTH; i >= 0; i--)
    {
        uint32_t curr_loc = loc_code >> (i * 3);

        // can probably be a for-loop
        if (curr_loc & 1U)
            result.x += offset;
        else
            result.x -= offset;

        if (curr_loc & 2U)
            result.y += offset;
        else
            result.y -= offset;

        if (curr_loc & 4U)
            result.z += offset;
        else
            result.z -= offset;

        offset /= 2.f;
    }
    return result;
}

SparseVoxelOctree::SparseVoxelOctree()
{
    //
    startGeneration(1, 0);

    long size = (sizeof(Node) + sizeof(LocCode)) * nodes.size();

	long nv = 0;
	long nc = 0;
	for (int i = 0; i < 8; i++)
	{
		nv += num_voxels[i];
		nc += num_checked[i];
	}

	std::cout << "num checked:  " << nc << "\n\n";

    std::cout << "num voxels: " << nv << "\n";
    std::cout << "num nodes:  " << nodes.size() << "\n";
    std::cout << "num bits:   " << 8 * size << "\n";
    std::cout << "bits/Voxel: " << 8.0 * size / double(nv) << "\n\n";

    std::cout << size << " Bytes\n";
    std::cout << size / 1024U << " KB\n";
    std::cout << size / 1048576U << " MB\n";
    std::cout << size / 1073741824U << " GB\n";
}

uint8_t SparseVoxelOctree::recursiveGenerate(uint8_t curr_child,
                                             LocCode loc_code,
                                             uint8_t depth,
                                             uint8_t map_index)
{
    LocCode curr_loc_code = curr_child;
    LocCode total_loc_code = (loc_code << 3) | curr_loc_code;

    if (depth >= MAX_DEPTH)
    {
		num_checked[map_index]++;

        glm::vec3 pos = calcPos(loc_code);

        float val = -0.5; //glm::simplex(pos);
        if (val > 0.0)
        {
            gen_maps[map_index][total_loc_code] = { 0 };
            num_voxels[map_index]++;
            return 1;
        }
        else
        {
            return 2;
        }
    }
    else
    {
        return checkChildren(total_loc_code, depth, map_index);
    }
}

uint8_t SparseVoxelOctree::checkChildren(const LocCode total_loc_code,
                                         const uint8_t depth,
                                         uint8_t map_index)
{
    uint8_t child_exits = 0;
    bool some_has_voxel = false;
    bool some_has_empty = false;

    for (int i = 0; i < 8; i++)
    {
        uint8_t child_bit = 1 << i;

        uint8_t result =
            recursiveGenerate(i, total_loc_code, depth + 1, map_index);
        if (result & 1)
        {
            child_exits |= child_bit;
            some_has_voxel = true;
        }
        if (result & 2)
        {
            some_has_empty = true;
        }
    }

    if (child_exits == 255U && some_has_voxel && !some_has_empty)
    {
        // if all children are voxels and no empty space exists below
        // remove redundant children
        for (int i = 0; i < 8; i++)
        {
            LocCode child_loc = (total_loc_code << 3) | i;
            gen_maps[map_index].erase(child_loc);
        }
        gen_maps[map_index][total_loc_code] = { child_exits };
        return 1;
    }
    else if (child_exits != 0)
    {
        gen_maps[map_index][total_loc_code] = { child_exits };
        return 1 | 2;
    }
    else
    {
        return 2;
    }
}

void SparseVoxelOctree::startGeneration(const LocCode total_loc_code,
                                        const uint8_t depth)
{
    uint8_t child_exits = 0;
    bool some_has_voxel = false;
    bool some_has_empty = false;

    std::vector<std::future<uint8_t>> futures;

    for (int i = 0; i < 8; i++)
    {
        futures.push_back(std::async(&SparseVoxelOctree::recursiveGenerate,
                                     this,
                                     i,
                                     total_loc_code,
                                     depth + 1,
                                     i));
    }

    for (int i = 0; i < 8; i++)
    {
        futures[i].wait();
        auto result = futures[i].get();
        uint8_t child_bit = 1 << i;
        if (result & 1)
        {
            child_exits |= child_bit;
            some_has_voxel = true;
        }
        if (result & 2)
        {
            some_has_empty = true;
        }

        nodes.insert(gen_maps[i].begin(), gen_maps[i].end());
        gen_maps[i].clear();
    }

    if (child_exits == 255U && some_has_voxel && !some_has_empty)
    {
        // if all children are voxels and no empty space exists below
        // remove redundant children
        for (int i = 0; i < 8; i++)
        {
            LocCode child_loc = (total_loc_code << 3) | i;
            nodes.erase(child_loc);
        }
        nodes[total_loc_code] = { child_exits };
    }
    else if (child_exits != 0)
    {
        nodes[total_loc_code] = { child_exits };
    }
}
