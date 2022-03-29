#version 430
#define PI 3.1415926538
layout(location = 1) uniform float time;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 PV;	//camera projection * view matrix
   vec4 eye_w;	//world-space eye position
   vec4 light_w; //world-space light position
};

layout(std140, binding = 1) uniform MaterialUniforms
{
   vec4 ka;	//ambient material color
   vec4 kd;	//diffuse material color
   vec4 ks;	//specular material color
   float shininess; //specular exponent
};

// Constant light colors
vec4 La = vec4(vec3(0.85f), 1.0f); // Ambient light color
vec4 Ld = vec4(vec3(0.5f), 1.0f); // Diffuse light color
vec4 Ls = vec4(1.0f); // Specular light color

in VertexData
{
   vec3 pw; //world-space vertex position
   vec3 nw; //world-space normal vector
} inData; //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

void main(void)
{   
    // Compute Cook-Torrance Lighting
    const float eps = 1e-8; //small value to avoid division by 0

    vec3 nw = normalize(inData.nw); //world-space unit normal vector
    vec3 lw = normalize(light_w.xyz - inData.pw.xyz); //world-space unit light vector
    vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);	//world-space unit view vector
    vec3 hw = normalize(lw + vw); // Halfway vector

    // reflect
    vec3 r = normalize(reflect(lw, nw));
    float specular =dot(nw, lw);
    float diffuse = max(dot(-lw, nw), 0.0);

    float intensity = 0.6 * diffuse + 0.4 * specular;

    // cell shading 
    if (specular >= 0) { 
        // darkest
        fragcolor =ka * 0.5;
    } else if (specular < 0) {
        // midtones
        fragcolor = ka * 0.7;
    } 
    if (dot(r, vw) > 0.95) {
        // highlights
        fragcolor = La * ka * 1.5;
    }

}