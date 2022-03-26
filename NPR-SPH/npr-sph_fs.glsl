#version 440

layout(location = 1) uniform float time;

out vec4 fragcolor; //the output color for this fragment    

uniform vec4 particle_color_rgb = vec4(0.75, 0.5, 0.25, 0.1);

void main(void)
{   
   fragcolor = particle_color_rgb;
}