#version 440
#define PI 3.1415926538

layout(binding = 0) uniform sampler2D bw_tex; 

layout(location = 2) uniform int pass;
layout(location = 3) uniform int mode;
layout(location = 6) uniform float scale;


layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 P;	//camera projection * view matrix
   mat4 V;
   vec4 eye_w;	//world-space eye position
   vec4 light_w; //world-space light position
};

layout(std140, binding = 3 ) uniform MaterialUniforms
{
   vec4 dark;	//ambient material color
   vec4 midtone;	//diffuse material color
   vec4 highlight;	//specular material color
};

in VertexData
{
   vec3 pw; //world-space vertex position
   vec3 nw; //world-space normal vector
   float depth;
   vec2 tex_coord;
} inData; //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

// Constant light colors
vec4 La = vec4(vec3(0.85f), 1.0f); // Ambient light color
vec4 Ld = vec4(vec3(0.5f), 1.0f); // Diffuse light color
vec4 Ls = vec4(1.0f); // Specular light color

mat3 sx = mat3( 
    1.0, 2.0, 1.0, 
    0.0, 0.0, 0.0, 
   -1.0, -2.0, -1.0 
);
mat3 sy = mat3( 
    1.0, 0.0, -1.0, 
    2.0, 0.0, -2.0, 
    1.0, 0.0, -1.0 
);

vec4 outline();
vec4 celshading();
vec3 desaturate(vec3 color, float amount);

void main(void)
{   
    if (mode == 1) {
        // simulate
        // make circle
        float rad = 0.35;
        float r = length(gl_PointCoord - vec2(rad));
        if (r >= rad) {
            discard;
        }
    }
    
    fragcolor = celshading();

    if (pass == 0) {
        // black and white for clear outline
        vec3 lum = vec3(0.299, 0.587, 0.114);
        // bw value, depth, alpha, -
        fragcolor = vec4(dot(fragcolor.rgb, lum), inData.depth, 1.0, fragcolor.a);
    } else if (pass == 1) {
        fragcolor.rgb = desaturate(fragcolor.rgb, clamp(pow(length(vec3(eye_w) - inData.pw)/4, 4), 0, 0.4));
        // outline 
        fragcolor *= outline();
    }
   
}

vec4 celshading() {
    // Compute Cook-Torrance Lighting
    const float eps = 1e-8; //small value to avoid division by 0

    vec3 nw = normalize(inData.nw); //world-space unit normal vector
    if (mode == 1) {
        // need to recalc normal
        vec2 p = 2.0*gl_PointCoord.xy - vec2(1.0);
        float z = sqrt(1.0 - dot(p, p));
        nw = normalize(vec3(p, z)); // normalize or no? 
    }
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
        // highlights
        fragcolor = La * highlight * 1.5;
        
    } else if (specular < 0) {
        // midtones
        fragcolor = midtone * 0.7;
    } 
    if (dot(r, vw) > 0.95) {
        // darkest
        fragcolor = dark * 0.5;
    }
    
    return fragcolor;

}

vec4 outline() {
    // add outlines (consistent thicknesS)
    mat3 I;

    for (int i=0; i< 3; i++) {
        for (int j=0; j<3; j++) {
            I[i][j] = texelFetch(bw_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).g; 
           
        }
    }
    
    // applying convolution filter to get gradient in x and y
    float gx = dot(sx[0], I[0]) + dot(sx[1], I[1]) + dot(sx[2], I[2]); 
    float gy = dot(sy[0], I[0]) + dot(sy[1], I[1]) + dot(sy[2], I[2]);
    // turn edge orientation to flat line (length of gradient)
    float g = sqrt(pow(gx, 2.0) + pow(gy, 2.0)); // with light outline

    // 0.1 threshold (otherwise will get contour)
    // MAKE THIS THRESHOLD A UNIFORM?
    if (g > 0.1) {
        g = 1.0;
    } else {
        g = 0.0;
    }

    return vec4(1.0 - vec3(g, g, g), 1.0);
}

vec3 desaturate(vec3 color, float amount)
{
    vec3 gray = vec3(dot(vec3(1.0), color));
    return vec3(mix(color, gray, amount));
}