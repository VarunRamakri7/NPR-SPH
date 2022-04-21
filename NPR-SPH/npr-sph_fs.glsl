#version 440

layout(location = 1) uniform float time;

in vec4 particle_pos;

out vec4 frag_color;

void main ()
{
	// Make circular particles
    float r = length(gl_PointCoord - vec2(0.35f));
    if (r >= 0.35) discard;

    vec3 normal = normalize(vec3(1.0f - pow(gl_FragCoord.x, 2) - pow(gl_FragCoord.y, 2))); // Get normal of particle

    frag_color = vec4(particle_pos.xyz, 1.0f);

    //frag_color = vec4(0.2f, 0.3f, 0.8f, 1.0f);
}