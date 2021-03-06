#version 440
#define PI 3.1415926538

layout(binding = 0) uniform sampler2D fbo_tex; 

layout(location = 1) uniform int style;
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
   vec4 outline_color;
   float shininess;
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

// sobel filters
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
vec4 phong();

void main(void)
{   
    // style
    if (style == 0) {
        // cell shading
        fragcolor = celshading();
    } else {
        // paint 
        fragcolor = mix(celshading(), phong(), 0.5);
    }

    // output
    if (pass == 0) {
        // outputs to texture

        // black and white for clear outline
        vec3 lum = vec3(0.299, 0.587, 0.114);
        // bw value, depth, alpha, -
        fragcolor = vec4(dot(fragcolor.rgb, lum), inData.depth, 1.0, 1.0);

    } else if (pass == 1) {
        // outputs to screen

        vec3 fill = outline().rgb;
        vec3 outlines = vec3(1.0) - fill; // inverse of fill
        fragcolor = vec4((outline_color.rgb * outlines) + (fragcolor.rgb * fill), 1.0);
    }

}

// repeated code in brush_fs
vec4 celshading() {
    // Compute Cook-Torrance Lighting
    const float eps = 1e-8; // small value to avoid division by 0

    vec3 nw = normalize(inData.nw); // world-space unit normal vector
    if (mode == 1) {
        // need to recalculate normal (2d particles)
        vec2 p = 2.0*gl_PointCoord.xy - vec2(1.0);
        float mag = dot(p, p); 
        if (mag > 1.0) discard; // discard if outside of radius length
        float z = sqrt(1.0 - mag);
        nw = normalize(vec3(p, z)); 
    }
    vec3 lw = normalize(light_w.xyz - inData.pw.xyz); // world-space unit light vector
    vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);	// world-space unit view vector
    vec3 hw = normalize(lw + vw); // Halfway vector

    // reflect
    vec3 r = normalize(reflect(-lw, nw));
    float nl = dot(nw, lw);

    if (nl < 0) {
        // ambient 
        fragcolor = dark;
    } else {
        // if (specular >=0) : diffuse
        fragcolor = midtone;
    }

    if (pow(dot(r, vw), shininess) > 0.95) {
        fragcolor = highlight;
    }

    return fragcolor;
}

vec4 outline() {
    // add outlines with consistent thickness using Sobel
    mat3 I;

    for (int i=0; i< 3; i++) {
        for (int j=0; j<3; j++) {
            // finding outline from contours (depth)
            I[i][j] = texelFetch(fbo_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).g; 
            // finding outline from colors (BW image)
            // I[i][j] = texelFetch(fbo_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).g; 
        }
    }
    
    // applying convolution filter to get gradient in x and y
    float gx = dot(sx[0], I[0]) + dot(sx[1], I[1]) + dot(sx[2], I[2]); 
    float gy = dot(sy[0], I[0]) + dot(sy[1], I[1]) + dot(sy[2], I[2]);
    // turn edge orientation to flat line (length of gradient)
    float g = sqrt(pow(gx, 2.0) + pow(gy, 2.0)); // with light outline

    // 0.1 threshold (otherwise will get contour)
    if (g > 0.1) {
        g = 1.0;
    } else {
        g = 0.0;
    }

    return vec4(1.0 - vec3(g, g, g), 1.0);
}

// repeated code in brush_fs.glsl
vec4 phong() {
     // Compute per-fragment Phong lighting	

      const float eps = 1e-8; // small value to avoid division by 0
      float d = distance(light_w.xyz, inData.pw.xyz);
      float atten = 1.0/(d*d+eps); // d-squared attenuation

      vec3 nw = normalize(inData.nw); // world-space unit normal vector
      vec3 lw = normalize(light_w.xyz - inData.pw.xyz);	// world-space unit light vector
      vec4 diffuse_term = atten*midtone*max(0.0, dot(nw, lw));

      vec3 vw = normalize(eye_w.xyz - inData.pw.xyz); // world-space unit view vector
      vec3 rw = reflect(-lw, nw); // world-space unit reflection vector

      vec4 specular_term = highlight*pow(max(0.0, dot(rw, vw)), shininess);

      return dark + diffuse_term + specular_term;
}