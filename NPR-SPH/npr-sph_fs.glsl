#version 440

layout(location = 1) uniform float time;

out vec4 fragcolor; //the output color for this fragment    

uniform vec4 particle_color_rgb = vec4(0.2f, 0.3f, 0.8f, 1.0f);

void main(void)
{   
   fragcolor = particle_color_rgb;
}