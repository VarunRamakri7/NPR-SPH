#version 430
#define PI 3.1415926538
layout(binding = 0) uniform sampler2D fbo_tex; 

layout(location = 1) uniform float time;
layout(location = 2) uniform int pass;

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
   vec2 tex_coord;
   vec3 pw; //world-space vertex position
   vec3 nw; //world-space normal vector
} inData; //block is named 'inData'


out vec4 fragcolor; //the output color for this fragment    

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

vec4 blur();
vec4 outline();
vec4 celshading();


void main(void)
{   
    fragcolor = celshading();
    if (pass == 1) {
        // get line thickness (0-1)
        float ndc = gl_FragCoord.z * 2.0 - 1.0; 
        float near = 0.1; 
        float far  = 100.0; 
        float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));	
        float line_width =  1 - 1/pow(linearDepth,2) * 5.0; // 5.0 to push back 

        // added distance fade effect
        // remap to 0 - 0.3
        line_width = clamp(line_width/2, 0, 0.3);

        // colored outline: fragcolor*outline()
        fragcolor = outline() * blur() + line_width;
    }
}

vec4 celshading() {
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
    
    return fragcolor;

}


vec4 blur()
{      
   // blur intensity 
   int hw = 20;
   float n=0.0;
   vec4 blur = vec4(0.0);
   for(int i=-hw; i<=hw; i++)
   {
      for(int j=-hw; j<=hw; j++)
      {
         blur += texelFetch(fbo_tex, ivec2(gl_FragCoord)+ivec2(i,j), 0);
         n+=1.0;
      }
   }
   blur = blur/n;
   return blur;
}


vec4 outline() {
    // add outlines

    // texture function is normalized [0,1] texture coordinates
    vec3 diffuse = texture(fbo_tex, inData.tex_coord.st).rgb;
    mat3 I;
    // converting image to greyscale
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            // unnormalized texture coordinates
            vec3 s = texelFetch(fbo_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).rgb;
            I[i][j] = length(s); 
        }
    }

    // applying convolution filter to get gradient in x and y
    float gx = dot(sx[0], I[0]) + dot(sx[1], I[1]) + dot(sx[2], I[2]); 
    float gy = dot(sy[0], I[0]) + dot(sy[1], I[1]) + dot(sy[2], I[2]);
    // turn edge orientation to flat line (length of gradient)
    float g = sqrt(pow(gx, 2.0) + pow(gy, 2.0)); // with light outline

    // black outline 
    float thickened = clamp(g* 10.0, 0.0, 1.0); // will either have 0 or 1
    return 1 - vec4(vec3(thickened), 0.0);
}
