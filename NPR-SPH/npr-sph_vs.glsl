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
layout(location = 2) in vec3 for_attrib;
layout(location = 3) in float rho_attrib;
layout(location = 4) in float pres_attrib;
layout(location = 5) in float age_attrib;

//write into the first bound tfo
layout(xfb_buffer = 0) out;

//interleave attrib order was pos, vel, force, rho, pressure, age
//3 floats + 3 floats + 3 floats + 1 float + 1 float + 1 float = 12 floats -> stride = 12*sizeof(float) = 48 bytes

layout(xfb_offset = 0, xfb_stride = 48) out vec3 pos_out; 
layout(xfb_offset = 12, xfb_stride = 48) out vec3 vel_out;
layout(xfb_offset = 24, xfb_stride = 48) out vec3 for_out;
layout(xfb_offset = 36, xfb_stride = 48) out float rho_out;
layout(xfb_offset = 40, xfb_stride = 48) out float pres_out;
layout(xfb_offset = 44, xfb_stride = 48) out float age_out;

vec3 v0(vec3 p); //Basic velocity field
float rand(vec2 co); // pseudorandom number

const float fraction = 0.01f;

void main(void)
{
	//Draw current particles from vbo
	gl_Position = PV*M*vec4(pos_attrib, 1.0f);
	gl_PointSize = 4.0;

	//Compute particle attributes for next frame
	vel_out = v0(pos_attrib);
	pos_out = pos_attrib + vel_out * time * fraction;
	age_out = age_attrib - 1.0;

	//Reinitialize particles as needed
	if(length(pos_attrib) > 100.0f)
	{
		vec2 seed = vec2(float(gl_VertexID), time * fraction); //seed for the random number generator
		//age_out = 500.0 + 200.0*rand(seed);
		
		vel_out *= 0.85f; // Add drag
		age_out = 10.0f; // Reset age

		pos_out = pos_attrib + vel_out * time * fraction;
		pos_out.y = 0.0f;
	}
}

vec3 v0(vec3 p)
{
	return vec3(0.0f, for_attrib.y * time * fraction + p.y, 0.0f);
	//return vec3(sin(-p.y*10.0+time/7.0-10.0), sin(-p.x*10.0+1.2*time+10.0), sin(+7.0*p.x -5.0*p.y + time));
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
