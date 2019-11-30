#include "voxeloctree.hpp"

#include <bitset>
#include <future>
#include <glm/gtc/noise.hpp>
#include <iostream>

size_t VoxelOctree::textureIndex(glm::ivec3 i)
{
    return 4 * (i.x + i.y * tex_side_length +
                i.z * tex_side_length * tex_side_length);
}

glm::ivec3 VoxelOctree::nextCell(glm::ivec3 i, int offset)
{
    i.x += offset;

    i.y += i.x / cells_side_length;
    i.x = i.x % cells_side_length;

    i.z += i.y / cells_side_length;
    i.y = i.y % cells_side_length;

    return i;
}

glm::ivec3 VoxelOctree::nextCellNoWrap(glm::ivec3 i, int offset)
{
    i.x += offset;

    i.y += i.x / cells_side_length;

    i.z += i.y / cells_side_length;

    return i;
}

bool VoxelOctree::isVoxel(glm::vec3 pos)
{
    LocCode loc = 1;

    bool result = false;
    glm::vec3 v_pos{ 0 };
    float offset = 0.5f;

    if (glm::any(glm::lessThan(glm::vec3(1), glm::abs(pos))))
    {
        // outside octree
        return false;
    }

    uint8_t last_child_exists = 0;
    for (int i = 0; i < MAX_DEPTH; i++)
    {
        uint8_t child = 0;
		for (int j = 0; j < 3; j++)
		{
			if (pos[j] > v_pos[j])
			{
				child |= 1 << j;
				v_pos[j] += offset;
			}
			else
			{
				v_pos[j] -= offset;
			}
		}

        offset *= 0.5;

        auto iter = nodes.find(loc);
        if (iter != nodes.end())
        {
            uint8_t child_bit = 1 << child;

            last_child_exists = iter->second.child_exits;

            if ((last_child_exists & child_bit) == 0)
            {
                result = false;
                break;
            }
        }
        else if (last_child_exists == 255)
        {
            result = true;
            break;
        }
        else
        {
            // this should not happen
            result = false;
            break;
        }

        loc = (loc << 3) & child;
    }

    return result;
}

glm::vec3 VoxelOctree::calcPos(uint32_t loc_code)
{
    float offset = 0.5f;
    glm::vec3 result{ 0 };
    for (int i = MAX_DEPTH; i >= 0; i--)
    {
        uint32_t curr_loc = loc_code >> (i * 3);

        // can probably be a for-loop
        if (curr_loc & 1)
            result.x += offset;
        else
            result.x -= offset;

        if (curr_loc & 2)
            result.y += offset;
        else
            result.y -= offset;

        if (curr_loc & 4)
            result.z += offset;
        else
            result.z -= offset;

        offset /= 2.f;
    }
    return result;
}

bool VoxelOctree::noise(glm::vec3 pos) { return glm::perlin(pos) > 0.0; }

VoxelOctree::VoxelOctree()
{
    startGeneration();

    createIndirectTexture();

    int size = 50;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            glm::vec3 pos{ 0 };
            pos.x = 2.f * x / float(size) - 1.f;
            pos.y = 2.f * y / float(size) - 1.f;

            if (isVoxel(1.2f * pos))
            {
                std::cout << "11";
            }
            else
            {
                std::cout << "  ";
            }
        }
        std::cout << "\n";
    }

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            glm::vec3 pos{ 0 };
            pos.x = 2.f * x / float(size) - 1.f;
            pos.y = 2.f * y / float(size) - 1.f;

            if (noise(1.2f * pos))
            {
                std::cout << "11";
            }
            else
            {
                std::cout << "  ";
            }
        }
        std::cout << "\n";
    }

    /*
	for (auto node : nodes)
	{
		std::bitset<8> child_exists(node.second.child_exits);
		std::bitset<sizeof(LocCode) * 8> loc(node.first);
		std::cout << loc << ": " << child_exists << "\n";
	}

    for (int z = 0; z < cells_side_length; z++)
    {
        for (int y = 0; y < cells_side_length; y++)
        {
            for (int x = 0; x < cells_side_length; x++)
            {
                std::cout << "(" << x << ", " << y << ", " << z << ")\n";
                for (int i = 0; i < 8; i++)
                {
                    glm::ivec3 offset{ 0 };
                    if (i & 1) offset.x += 1;
                    if (i & 2) offset.y += 1;
                    if (i & 4) offset.z += 1;

                    size_t index =
                        textureIndex(2 * glm::ivec3(x, y, z) + offset);

                    std::cout << "(" << offset.x << ", " << offset.y << ", "
                              << offset.z << "): ";
                    std::cout << (int)indirect_texture[index + 0] << ", ";
                    std::cout << (int)indirect_texture[index + 1] << ", ";
                    std::cout << (int)indirect_texture[index + 2] << ", ";
                    std::cout << (int)indirect_texture[index + 3] << "\n";
                }
                std::cout << "\n";
            }
        }
    }
	*/
}

void VoxelOctree::createIndirectTexture()
{
    size_t side_len = glm::ceil(glm::pow(double(nodes.size()), 1.0 / 3.0));
    std::cout << side_len << "\n";

    indirect_texture.resize(side_len * side_len * side_len * 8 * 4, 99);
    cells_side_length = side_len;
    tex_side_length = side_len * 2;

    recursiveCreateIndirect(glm::ivec3(0), 1, 0);
}

void VoxelOctree::recursiveCreateIndirect(glm::ivec3 my_cell, LocCode my_loc,
                                          uint8_t depth)
{
    glm::ivec3 child_cells_start = nextCell(next_free_cell);
    uint8_t child_exits = 0;

    // node exists because we checked in depth-1
    Node node = nodes[my_loc];

    for (int i = 0; i < 8; i++)
    {
        LocCode child_loc = (my_loc << 3) | i;
        auto iter = nodes.find(child_loc);

        if (node.child_exits == 255 && iter == nodes.end())
        {
            // node is leaf containing voxels
            size_t index = textureIndex(2 * my_cell);
            indirect_texture[index + 0] = 111;
            indirect_texture[index + 1] = 111;
            indirect_texture[index + 2] = 111;
            indirect_texture[index + 3] = 111;

            // TODO: maybe fill rest of cell
            break;
        }
        glm::ivec3 offset{ 0 };
        if (i & 1) offset.x += 1;
        if (i & 2) offset.y += 1;
        if (i & 4) offset.z += 1;

        // populate current cell with info about children
        size_t index = textureIndex(2 * my_cell + offset);

        if (iter != nodes.end())
        {
            glm::ivec3 indirect = nextCell(next_free_cell, i + 1);

            indirect_texture[index + 0] = indirect.x;
            indirect_texture[index + 1] = indirect.y;
            indirect_texture[index + 2] = indirect.z;
            indirect_texture[index + 3] = 2;

            // "reserve" the current next_free_cell for this child
            next_free_cell = nextCell(next_free_cell);

            child_exits |= 1 << i;
        }
        else
        {
            indirect_texture[index + 0] = 0;
            indirect_texture[index + 1] = 0;
            indirect_texture[index + 2] = 0;
            indirect_texture[index + 3] = 0;
        }
    }

    glm::ivec3 child_cell = child_cells_start;
    for (int i = 0; i < 8; i++)
    {
        uint8_t child_bit = 1 << i;
        if (child_bit & child_exits)
        {
            LocCode child_loc = (my_loc << 3) | i;
            recursiveCreateIndirect(child_cell, child_loc, depth + 1);

            child_cell = nextCell(child_cell);
        }
    }
}

void VoxelOctree::printInfo()
{
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

uint8_t VoxelOctree::recursiveGenerate(uint8_t curr_child, LocCode loc_code,
                                       uint8_t depth, uint8_t map_index)
{
    LocCode curr_loc_code = curr_child;
    LocCode total_loc_code = (loc_code << 3) | curr_loc_code;

    if (depth >= MAX_DEPTH)
    {
        num_checked[map_index]++;

        glm::vec3 pos = calcPos(loc_code);

        if (noise(pos))
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

uint8_t VoxelOctree::checkChildren(const LocCode total_loc_code,
                                   const uint8_t depth, uint8_t map_index)
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
            // TODO: idea: create second function that creates nodes to avoid erasing already created
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

void VoxelOctree::startGeneration()
{
    LocCode total_loc_code = 1;
    uint8_t depth = 0;

    uint8_t child_exits = 0;
    bool some_has_voxel = false;
    bool some_has_empty = false;

    std::vector<std::future<uint8_t>> futures;

    for (int i = 0; i < 8; i++)
    {
        futures.push_back(std::async(&VoxelOctree::recursiveGenerate, this, i,
                                     total_loc_code, depth + 1, i));
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
