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

const int num_steps = 128;
const float voxel_size = 0.01;

//	<https://www.shadertoy.com/view/4dS3Wd>
//	By Morgan McGuire @morgan3d, http://graphicscodex.com
//
float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
// This one has non-ideal tiling properties that I'm still tuning
float noise(vec3 x) {
	
	const vec3 step = vec3(110, 241, 171);
	vec3 i = floor(x);
	vec3 f = fract(x);
	// For performance, compute the base input to a 1D hash from the integer part of the argument and the 
	// incremental change to the 1D based on the 3D -> 1D wrapping
    float n = dot(i, step);

	vec3 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
               mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

float map(vec3 p)
{
	p = voxel_size * floor(p/voxel_size);

	p *= 0.5;

	float n = 0.0;
	n += 0.5*noise(p);
	n += 0.5*noise(4*p);
	return n;
}

vec3 calcNormal(vec3 p)
{
	p = voxel_size * (floor(p/voxel_size)+0.5);
	vec2 t = vec2(voxel_size, 0);
	return normalize(vec3(
		map(p - t.xyy) - map(p + t.xyy), 
		map(p - t.yxy) - map(p + t.yxy),
		map(p - t.yyx) - map(p + t.yyx)));
}

void main() 
{
	vec3 ray_dir = normalize(in_ray_dir);
	vec3 cam_pos = ubo.cam_pos.xyz;

	vec3 s = sign(ray_dir);

	float t = 0;

	vec3 normal = vec3(0);

	for(int i = 0; i < num_steps; i++) 
	{
		vec3 point = cam_pos + t * ray_dir;
		float n = map(point);
		if(n > 0.3)
		{
			normal = calcNormal(point);
			break;
		}

		vec3 v_pos = voxel_size * (floor(point/voxel_size)+0.5);
		vec3 t_max = (v_pos+0.5*voxel_size*s - point)/ray_dir;

		bvec3 mask = lessThanEqual(t_max, vec3(min(t_max.x, min(t_max.y, t_max.z))));

		if(mask.x)
			t += t_max.x;
		else if(mask.y)
			t += t_max.y;
		else 
			t += t_max.z;
		t += voxel_size*0.01;
	}

	vec3 surf_pos = cam_pos + t * ray_dir;
	vec3 light_pos = vec3(0,0,0);
	vec3 color = vec3(normal);
	//color = mix(color, vec3(0.5), smoothstep(0, num_steps * voxel_size, t));
	vec3 l = normalize(light_pos - surf_pos);
	float a = 1.-clamp(length(light_pos - surf_pos)/10, 0,1);
	float diffuse = max(a*dot(l,normal), 0);

	vec3 half_vec = normalize(l + -ray_dir);
	float specular = pow(clamp(dot(normal, half_vec), 0, 1), 100.0);

	color = vec3(diffuse + specular);

	float max_dist = num_steps * voxel_size;
	color = mix(color, vec3(0), smoothstep(max_dist*0.5, max_dist*0.7, t));

    out_color = vec4(color , 1.0);
}
