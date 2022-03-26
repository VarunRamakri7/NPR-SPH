#version 440            
layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
};

layout(location = 0) in vec3 pos_attrib;
layout(location = 1) in vec3 vel_attrib;
layout(location = 2) in float age_attrib;

//write into the first bound tfo
layout(xfb_buffer = 0) out;

//interleave attrib order was pos, vel, age
//3 floats + 3 floats + 1 float = 7 floats -> stride = 7*sizeof(float) = 28 bytes

//pos offset = 0 bytes
layout(xfb_offset = 0, xfb_stride = 28) out vec3 pos_out; 

//vel offset = 3*sizeof(float) = 12 bytes
layout(xfb_offset = 12, xfb_stride = 28) out vec3 vel_out; 

//age offset = 6*sizeof(float) = 24 bytes
layout(xfb_offset = 24, xfb_stride = 28) out float age_out;

//Basic velocity field
vec3 v0(vec3 p);

//pseudorandom number
float rand(vec2 co);

void main(void)
{
	//Draw current particles from vbo
	gl_Position = PV*M*vec4(pos_attrib, 1.0);
	gl_PointSize = 6.0;

	//Compute particle attributes for next frame
	vel_out = v0(pos_attrib);
	pos_out = pos_attrib + 0.003*vel_out.xyz;
	age_out = age_attrib - 1.0;

	//Reinitialize particles as needed
	if(age_out <= 0.0 || length(pos_out) > 2.0f)
	{
		vec2 seed = vec2(float(gl_VertexID), time/10.0); //seed for the random number generator
		age_out = 500.0 + 200.0*rand(seed);
		//Pseudorandom position
		pos_out = 0.5*vec3(rand(seed.xx), rand(seed.yy), 0.1*rand(seed.xy));
	}
}

vec3 v0(vec3 p)
{
	return vec3(sin(-p.y*10.0+time/7.0-10.0), sin(-p.x*10.0+1.2*time+10.0), sin(+7.0*p.x -5.0*p.y + time));
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
