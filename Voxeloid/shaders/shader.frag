#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec3 in_ray_dir;

layout(binding = 0) uniform UniformBufferObject {
    vec4 cam_pos;
	vec4 cam_dir;
} ubo;

layout(binding = 1) uniform sampler3D tex_indirect;

// https://gist.github.com/DomNomNom/46bb1ce47f68d255fd5d
vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) / rayDir;
    vec3 tMax = (boxMax - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
}
vec3 voxel(vec3 lro, vec3 rd, vec3 ird, float size)
{
    size *= 0.5;
    vec3 hit = -(sign(rd)*(lro-size)-size)*ird;   
    return hit;
}

const int INDIRECT_LEAF = 255;
const int INDIRECT_EMPTY = 0;
const int INDIRECT_NODE = 127;

void main() 
{
	vec3 ray_dir = normalize(in_ray_dir);
	vec3 ray_ori = ubo.cam_pos.xyz;
	vec3 s = sign(ray_dir);

	vec3 start = ray_ori;

	vec3 color = vec3(0);
	const int MAX_DEPTH = 5;
	const float MIN_VOXEL_SIZE = 1.0/pow(2,MAX_DEPTH); 
	float tot_len = 0.0;

	float voxel_size = 0.5;

	bool exitoctree = false;
	int depth = 0;
	ivec3 current_cell = ivec3(0);
	ivec3 cells_stack[MAX_DEPTH];
	vec3 centers_stack[MAX_DEPTH];
	vec3 center = vec3(0.5);

	for (int i = 0; i < 256; i++) 
	{
		if (exitoctree)
		{
			depth--;

			current_cell = cells_stack[depth];
			center = centers_stack[depth];

			voxel_size *= 2.0;

			float vsize2 = voxel_size * 2.0;
			vec3 vpos2 = vsize2 * (floor(ray_ori/vsize2)+0.5);
			vec3 new_vpos2 = vsize2 * (floor((ray_ori+vsize2)/vsize2)+0.5);
			exitoctree = vpos2 != new_vpos2 && (depth > 0);
		} else {
			// check current voxel at f_ray_ori
			vec3 local_ori = mod(ray_ori, 1.0);
			ivec3 offset = ivec3(lessThan(center, local_ori));
			vec4 res = texelFetch(tex_indirect, 2*current_cell + offset, 0);
			ivec4 node_info = ivec4(round(res * 255.0));
				
			if (node_info.w == INDIRECT_NODE && depth < MAX_DEPTH)
			{
				//color = vec3(0,0,1);
				centers_stack[depth] = center;
				cells_stack[depth] = current_cell;

				depth++;
				voxel_size *= 0.5;

				current_cell = node_info.xyz;
				center += voxel_size*vec3(offset*2-1);
			} else if (node_info.w == INDIRECT_LEAF){
				color = vec3(1,0,0) * smoothstep(4,0,length(start-ray_ori));
				break;
			} else {
				vec3 vpos = voxel_size * (floor(ray_ori/voxel_size)+0.5);
				vec3 hit = (vpos + 0.5*voxel_size*s - ray_ori)/ray_dir;

				bvec3 mask = lessThan(hit, min(hit.yzx, hit.zxy));
				float t = 0;// = dot(hit, mask);
				if(mask.x)
					t += hit.x;
				else if(mask.y)
					t += hit.y;
				else 
					t += hit.z;


				vec3 new_ray_ori = ray_ori + (t+0.00001*MIN_VOXEL_SIZE) * ray_dir;

				float vsize2 = voxel_size * 2.0;
				vec3 vpos2 = vsize2 * (floor(ray_ori/vsize2)+0.5);
				vec3 new_vpos2 = vsize2 * (floor(new_ray_ori/vsize2)+0.5);

				exitoctree = vpos2 != new_vpos2 && (depth > 0);

				ray_ori = new_ray_ori;

				/*
				if(any(lessThan(ray_ori, vec3(-1))) || any(lessThan(vec3(1), ray_ori)))
				{
					color = vec3(0);
					break;
				}
				*/
			}
		}
	}

    out_color = vec4(color, 1.0);
}
