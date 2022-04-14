#version 430
#define PI 3.1415926538
layout(binding = 0) uniform sampler2D fbo_tex; 
layout(binding = 1) uniform sampler2D comp_tex; 

layout(location = 1) uniform float time;
layout(location = 2) uniform int pass;
layout(location = 3) uniform int paint_mode;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 P;	//camera projection * view matrix
   mat4 V;
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
   float depth;
   vec2 tex_coord;
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

mat3 xsx = mat3( 
    1.0, 1.0, 1.0, 
    -2.0, -2.0, -2.0, 
   1.0, 1.0, 1.0 
);
mat3 xsy = mat3( 
    1.0, -2.0, 1.0, 
    1.0, -2.0, 1.0, 
    1.0, -2.0, 1.0 
);

vec4 blur(int rad, sampler2D tex);
vec4 outline();
vec4 celshading();
float blur_channel(int rad, sampler2D tex, int channel);
vec4 blur_mask(int rad, sampler2D tex, sampler2D mask, int mask_channel);

// The watercolours tends to dry first in the center
// and accumulate more pigment in the corners
float brush_effect(float dist, float h_avg, float h_var)
{
    float h = max(0.0,1.0-10.0*abs(dist));
    h *= h;
    h *= h;
    return (h_avg+h_var*h) * smoothstep(-0.01, 0.002, dist);
}

// post processing
float edge_efx() {

// add outlines

    // texture function is normalized [0,1] texture coordinates
    vec3 diffuse = texture(fbo_tex, inData.tex_coord.st).rgb;

    mat3 I;
    // have minimum at 3
    for (int i=0; i< 3; i++) {
        for (int j=0; j< 3 ; j++) {
            // unnormalized texture coordinates
            vec3 s = texelFetch(fbo_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).rgb;
            I[i][j] = length(s); 
            I[i+1][j+1] = length(s); 
            I[i+10][j+6] = length(s); 
        }
    }
    
    // applying convolution filter to get gradient in x and y
    float gx = dot(sx[0], I[0]) + dot(sx[1], I[1]) + dot(sx[2], I[2]); 
    float gy = dot(sy[0], I[0]) + dot(sy[1], I[1]) + dot(sy[2], I[2]);
    // turn edge orientation to flat line (length of gradient)
    float g = sqrt(pow(gx, 2.0) + pow(gy, 2.0)); // gradient change level 

    // black outline 
    float thickened = clamp(g* 10.0, 0.0, 1.0); // will either have 0 or 1
//    if (thickened > 0) {
//    thickened = 1.0;
//    }

    return thickened;
}

void main(void)
{   
    
    fragcolor = celshading();

    if (paint_mode == 0) {
        // cell shading 
    } else {
        // the watercolor
    
        if (pass == 1) {
            // out to texture:
            // using r channel for outline, g for alpha 
            fragcolor = vec4(edge_efx(), 1.0, inData.depth, 0.0); // no fourth ? texture only RGB            
        }

        else if (pass == 2) {
            // blur is between 0 and 1 (darken edges)
            //To convert form the depth of the depth buffer to the original Z-coordinate
            // get line thickness (0-1)
            // Transform the variable gl_FragCoord.z to normalized devices coordinates in the range [-1, 1]
            float ndc = gl_FragCoord.z * 2.0 - 1.0; 
            float near = 0.1; 
            float far  = 100.0; 
            // perspective projection 
            float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));	
            float line_width =  1 - (1/linearDepth) * 2.5; // 5.0 to push back 

            // added distance fade effect
            // remap to 0 - 0.1
            line_width = clamp(line_width, 0, 0.1);
     
            // smudge the cell shading (blend)
            fragcolor = blur_mask(15,fbo_tex, comp_tex, 1);
            // make edges abit darker (where the colors settle close to the edge)
            // 0.3 is the edge darkening value
            fragcolor -= (0.3*blur_channel(40, comp_tex, 0));
            // make things far away lighter
            fragcolor += line_width;
            // add outlines
            fragcolor *= outline();

            // need to add noise
       }

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

vec4 blur_mask(int rad, sampler2D tex, sampler2D mask, int mask_channel)
{      
   // blur intensity 
   int hw = rad;
   float n=0.0;
   vec4 blur = vec4(0.0);
   for(int i=-hw; i<=hw; i++)
   {
      for(int j=-hw; j<=hw; j++)
      {
         ivec2 uv = ivec2(gl_FragCoord)+ivec2(i,j);
         if (texelFetch(mask, uv, 0)[mask_channel] > 0) {
             blur += texelFetch(tex, uv, 0);
             // if outside edge make it white 
             n+=1.0;
         }
         
      }
   }
   blur = blur/n;
   return blur;
}

vec4 blur(int rad, sampler2D tex)
{      
   // blur intensity 
   int hw = rad;
   float n=0.0;
   vec4 blur = vec4(0.0);
   for(int i=-hw; i<=hw; i++)
   {
      for(int j=-hw; j<=hw; j++)
      {
         blur += texelFetch(tex, ivec2(gl_FragCoord)+ivec2(i,j), 0);
         n+=1.0;
      }
   }
   blur = blur/n;
   return blur;
}

float blur_channel(int rad, sampler2D tex, int channel)
{      
   // blur intensity 
   int hw = rad;
   float n=0.0;
   float blur = 0.0;
   for(int i=-hw; i<=hw; i++)
   {
      for(int j=-hw; j<=hw; j++)
      {
         blur += (texelFetch(tex, ivec2(gl_FragCoord)+ivec2(i,j), 0)[channel] * 3);
         // contour still not thick, line thickness is not equal 
         n+=1.0;
      }
   }
   blur = blur/n;
   return blur;
}


vec4 outline() {
    // add outlines that are thicker closer to the viewer

    // texture function is normalized [0,1] texture coordinates
    mat3 I;
    // depth value between 0 (closest) and 1 (far) -> want to emphasize the difference
    // get line thickness (0-1)
    // https://stackoverflow.com/questions/11277501/how-to-recover-view-space-position-given-view-space-depth-value-and-ndc-xy/46118945#46118945
    float ndc = gl_FragCoord.z * 2.0 - 1.0; // 0 to 1 
    float remap = 0 + (1 - pow(ndc, 2) - 0) * (10 - 0) / (1 - 0); // remap inverse from 0-1 to 0-10 

    // have minimum at 3, *5 for higher range
    for (int i=0; i<3; i++) {
        for (int j=0; j<3; j++) {
            // unnormalized texture coordinates
            //vec3 s = texelFetch(fbo_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).rgb;
            //I[i][j] = length(s); 
            // BUT EDGE OF TEXTURE MAKES IT DETECT THE EDGE!! 
            I[i][j] = texelFetch(comp_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).b; // depth would be the same > we don't want to look at the color because we want the objects edge
           
        }
    }
    
    // applying convolution filter to get gradient in x and y
    float gx = dot(sx[0], I[0]) + dot(sx[1], I[1]) + dot(sx[2], I[2]); 
    float gy = dot(sy[0], I[0]) + dot(sy[1], I[1]) + dot(sy[2], I[2]);
    // turn edge orientation to flat line (length of gradient)
    float g = sqrt(pow(gx, 2.0) + pow(gy, 2.0)); // with light outline
    return 1.0 - vec4(g, g, g, 1.0);
}
