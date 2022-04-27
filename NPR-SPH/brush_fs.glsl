#version 440
#define PI 3.1415926538

layout(binding = 0) uniform sampler2D fbo_tex; 

layout(location = 3) uniform int mode;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 P; //camera projection * view matrix
   mat4 V;
   vec4 eye_w; //world-space eye position
   vec4 light_w; //world-space light position
};

layout(std140, binding = 3) uniform MaterialUniforms
{
   vec4 dark; // ambient material color
   vec4 midtone; // diffuse material color
   vec4 highlight; // specular material color
   vec4 outline_color;
   float shininess;
   float brush_scale;
};

in VertexData
{
   vec3 pw; //world-space vertex position
   vec3 nw; //world-space normal vector
   float depth;
   vec2 tex_coord;
   float color;
} inData; //block is named 'inData'

out vec4 fragcolor; //the output color for this fragment    

// sobel filter
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
vec4 phong();

void main(void)
{   
    // mixing celshading and phong to get blended look
    fragcolor = mix(celshading(), phong(), 0.5);

    // random color variation from noise function
    // 0.02 to lower the range, adding subtle variation
    float noise = (fract(sin(dot(inData.tex_coord, vec2(12.9898,78.233))) * 43758.5453)*0.02);
    fragcolor += noise;

    vec3 fill = outline().rgb;
    vec3 outlines = vec3(1.0) - fill; // inverse of fill
    // lighter outline and colors at the back
    vec3 line_color = desaturate(outline_color.rgb, clamp(pow(length(vec3(eye_w) - inData.pw)/15.0, 4), 0.0, 0.8)) * outlines;
    vec3 desat_fill = desaturate(fragcolor.rgb, clamp(pow(length(vec3(eye_w) - inData.pw)/15.0, 4), 0.0, 0.8)) * fill;
    fragcolor = vec4(line_color + desat_fill, 1.0);

    // keeping strokes inside the object
    fragcolor.a = texelFetch(fbo_tex, ivec2(gl_FragCoord), 0).b * inData.color;
    // if the depth is different then its a different object and can discard
    if (abs(inData.depth - texelFetch(fbo_tex, ivec2(gl_FragCoord), 0).g) > 0.01) {
        discard;
    }
}

vec3 desaturate(vec3 color, float amount)
{
    vec3 gray = vec3(dot(vec3(1.0), color));
    if (color == vec3(0.0,0.0,0.0)) {
        // if black, grey is 0.7
        gray = vec3(0.7);
    }
    return vec3(mix(color, gray, amount));
}

// repeated code in toon_fs
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
    // add outlines with varying thickness (thicker when close to the viewer)
    mat3 I;

    // get thickness
    float d = 1.0 - clamp(pow(length(vec3(eye_w) - inData.pw)/4, 1), 0.0, 1.0); 
    int linethickness = 5 + int(floor(d * 10));

    for (int i=0; i< linethickness; i++) {
        for (int j=0; j<linethickness; j++) {
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

    // threshold (otherwise will get contour)
    if (g > 0.1) {
        g = 1.0;
    } else {
        g = 0.0;
    }

    return vec4(1.0 - vec3(g, g, g), 1.0);
}

// repeated code in toon_fs.glsl
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