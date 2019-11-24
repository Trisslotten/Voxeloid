#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec3 out_ray_dir;

layout(binding = 0) uniform UniformBufferObject {
    vec4 cam_pos;
	vec4 cam_dir;
} ubo;


vec2 positions[3] = vec2[]
(
    vec2(-1., -1.),
    vec2(3.0, -1.0),
    vec2(-1.0, 3.0)
);

void main() 
{
	vec3 cam_pos = ubo.cam_pos.xyz;
	vec3 cam_dir = ubo.cam_dir.xyz;
	vec3 up = vec3(0,1,0);
	vec3 side = normalize(cross(up, cam_dir));
	vec3 cam_up = normalize(cross(side, cam_dir));

	float aspect = 16./9.;
	vec2 pos = positions[gl_VertexIndex];
	out_ray_dir = cam_dir + aspect*pos.x*side + pos.y*cam_up;

	out_uv = pos*0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}

