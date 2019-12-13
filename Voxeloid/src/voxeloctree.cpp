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
    uint8_t my_loc = 0;
    for (int i = 0; i < MAX_DEPTH; i++)
    {
        uint8_t child_loc = 0;
        for (int j = 0; j < 3; j++)
        {
            if (pos[j] > v_pos[j])
            {
                child_loc |= 1 << j;
                v_pos[j] += offset;
            }
            else
            {
                v_pos[j] -= offset;
            }
        }
        offset *= 0.5f;

        auto iter = nodes.find(loc);
        if (iter != nodes.end())
        {
            uint8_t child_bit = 1 << child_loc;

            uint8_t child_exists = iter->second.child_exits;
            if ((child_exists & child_bit) == 0)
            {
                result = false;
                break;
            }

            last_child_exists = child_exists;
            my_loc = child_loc;
        }
        else if (last_child_exists & (1 << my_loc))
        {
            result = true;
            break;
        }
        else
        {
            result = false;
            break;
        }

        loc = (loc << 3) | child_loc;
    }

    return result;
}

bool VoxelOctree::isVoxel2(glm::vec3 pos)
{
    if (glm::any(glm::lessThan(glm::vec3(1), glm::abs(pos))))
    {
        // outside octree
        return false;
    }
    float pos_offset = 0.5f;
    glm::vec3 center{ 0 };
    glm::ivec3 current_cell{ 0 };

    for (int i = 0; i <= MAX_DEPTH; i++)
    {
        glm::vec3 dir = pos - center;
        glm::ivec3 offset{ 0 };
        for (int j = 0; j < 3; j++)
            if (dir[j] > 0) offset[j] += 1;

        size_t index = textureIndex(2 * current_cell + offset);
        uint8_t node_info = indirect_texture[index + 3];

        switch (node_info)
        {
        case INDIRECT_LEAF: return true;
        case INDIRECT_EMPTY: return false;
        }
        // is INDIRECT_NODE, keep going

		glm::vec3 sgn = glm::lessThan(glm::vec3(0), dir);
        center += pos_offset * (sgn*2.f - 1.f);
        pos_offset *= 0.5f;
        for (int j = 0; j < 3; j++)
            current_cell[j] = indirect_texture[index + j];
    }

    return false;
}

glm::vec3 VoxelOctree::calcPos(uint32_t loc_code)
{
    float offset = 0.5f;
    glm::vec3 result{ 0 };
    for (int i = MAX_DEPTH - 1; i >= 0; i--)
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

bool VoxelOctree::noise(glm::vec3 pos) { return glm::perlin(2.f * pos, glm::vec3(4.f)) > 0.3; }

VoxelOctree::VoxelOctree()
{
    startGeneration();

    createIndirectTexture();
}

void VoxelOctree::createIndirectTexture()
{
    // TODO: some leaf nodes dont exist in nodes but need to exist in indirect texture
    // therefore 2*
    size_t side_len = glm::ceil(2 * glm::pow(double(nodes.size()), 1.0 / 3.0));
    std::cout << side_len << "\n";

    indirect_texture.resize(side_len * side_len * side_len * 8 * 4,
                            INDIRECT_EMPTY);
    cells_side_length = side_len;
    tex_side_length = side_len * 2;

    next_free_cell = nextCell(next_free_cell);
    recursiveCreateIndirect(glm::ivec3(0), 1, 0);
}

void VoxelOctree::recursiveCreateIndirect(glm::ivec3 my_cell, LocCode my_loc,
                                          uint8_t depth)
{
    glm::ivec3 child_cells_start = next_free_cell;

    auto curr_iter = nodes.find(my_loc);
    if (curr_iter == nodes.end() ||
        (curr_iter->second.child_exits == 255 && depth >= MAX_DEPTH))
    {
        // this is leaf node

        for (int i = 0; i < 8; i++)
        {
            glm::ivec3 offset{ 0 };
            if (i & 1) offset.x += 1;
            if (i & 2) offset.y += 1;
            if (i & 4) offset.z += 1;
            size_t index = textureIndex(2 * my_cell + offset);

            indirect_texture[index + 0] = 255;
            indirect_texture[index + 1] = 0;
            indirect_texture[index + 2] = 0;
            indirect_texture[index + 3] = INDIRECT_LEAF;
        }
        return;
    }
	if (depth > MAX_DEPTH)
	{
		return;
	}

    Node node = curr_iter->second;

    for (int i = 0; i < 8; i++)
    {
        glm::ivec3 offset{ 0 };
        if (i & 1) offset.x += 1;
        if (i & 2) offset.y += 1;
        if (i & 4) offset.z += 1;
        size_t index = textureIndex(2 * my_cell + offset);

        uint8_t child_bit = 1 << i;
        if (child_bit & node.child_exits)
        {
            // populate current cell with info about children

            glm::ivec3 indirect = next_free_cell;

            indirect_texture[index + 0] = indirect.x;
            indirect_texture[index + 1] = indirect.y;
            indirect_texture[index + 2] = indirect.z;
            indirect_texture[index + 3] = INDIRECT_NODE;

            // "reserve" the current next_free_cell for this child
            next_free_cell = nextCell(next_free_cell);
        }
        else
        {
            indirect_texture[index + 0] = 0;
            indirect_texture[index + 1] = 0;
            indirect_texture[index + 2] = 255;
            indirect_texture[index + 3] = INDIRECT_EMPTY;
        }
    }

    glm::ivec3 child_cell = child_cells_start;
    for (int i = 0; i < 8; i++)
    {
        uint8_t child_bit = 1 << i;
        if (child_bit & node.child_exits)
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

        glm::vec3 pos = calcPos(total_loc_code);

        if (noise(pos))
        {
            gen_maps[map_index][total_loc_code] = { 255 };
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
    uint8_t child_has_empty = 0;

    for (int i = 0; i < 8; i++)
    {
        uint8_t child_bit = 1 << i;

        uint8_t result =
            recursiveGenerate(i, total_loc_code, depth + 1, map_index);
        if (result & 1)
        {
            child_exits |= child_bit;
        }
        if (result & 2)
        {
            child_has_empty |= child_bit;
        }
    }

    if (child_exits)
    {
        // remove redundant children
        for (int i = 0; i < 8; i++)
        {
            // TODO: idea: create second function that creates nodes to avoid erasing already created
            uint8_t child_bit = 1 << i;
            bool has_voxel = child_exits & child_bit;
            bool has_empty = child_has_empty & child_bit;
            if (has_voxel && !has_empty)
            {
                LocCode child_loc = (total_loc_code << 3) | i;
                gen_maps[map_index].erase(child_loc);
            }
        }
        gen_maps[map_index][total_loc_code] = { child_exits };
    }

    uint8_t result = 0;
    if (child_exits) result |= 1;
    if (child_has_empty) result |= 2;
    return result;
}

void VoxelOctree::startGeneration()
{
    LocCode total_loc_code = 1;
    uint8_t depth = 0;

    uint8_t child_exits = 0;
    uint8_t child_has_empty = 0;

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
        }
        if (result & 2)
        {
            child_has_empty |= child_bit;
        }

        nodes.insert(gen_maps[i].begin(), gen_maps[i].end());
        gen_maps[i].clear();
    }

    if (child_exits)
    {
        // remove redundant children
        for (int i = 0; i < 8; i++)
        {
            // TODO: idea: create second function that creates nodes to avoid erasing already created
            uint8_t child_bit = 1 << i;
            bool has_voxel = child_exits & child_bit;
            bool has_empty = child_has_empty & child_bit;
            if (has_voxel && !has_empty)
            {
                LocCode child_loc = (total_loc_code << 3) | i;
                nodes.erase(child_loc);
            }
        }
        nodes[total_loc_code] = { child_exits };
    }
}
