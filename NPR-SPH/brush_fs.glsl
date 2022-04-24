#version 440
#define PI 3.1415926538

layout(binding = 0) uniform sampler2D fbo_tex; 

layout(location = 6) uniform float scale;
layout(location = 3) uniform int mode;


layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 P;	//camera projection * view matrix
   mat4 V;
   vec4 eye_w;	//world-space eye position
   vec4 light_w; //world-space light position
};

layout(std140, binding = 3) uniform MaterialUniforms
{
   vec4 dark;	//ambient material color
   vec4 midtone;	//diffuse material color
   vec4 highlight;	//specular material color
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

// Constant light colors
vec4 La = vec4(vec3(0.85f), 1.0f); // Ambient light color
vec4 Ld = vec4(vec3(0.5f), 1.0f); // Diffuse light color
vec4 Ls = vec4(1.0f); // Specular light color


vec4 outline();
vec4 celshading();
vec3 desaturate(vec3 color, float amount);
vec4 blur(int rad, sampler2D tex);
vec4 phong();

void main(void)
{   
    // but then toon shading under -> needs to change with mode
    fragcolor = mix(celshading(), phong(), 0.5);
    // random from noise function
    //0.02 to lower the range, adding subtle variation
    float noise = (fract(sin(dot(inData.tex_coord, vec2(12.9898,78.233))) * 43758.5453)*0.02);
    fragcolor += noise;
    fragcolor.a = texelFetch(fbo_tex, ivec2(gl_FragCoord), 0).b * inData.color;
    fragcolor *= outline();
    fragcolor.rgb = desaturate(fragcolor.rgb, clamp(pow(length(vec3(eye_w) - inData.pw)/15, 4), 0, 0.8));
}

vec3 desaturate(vec3 color, float amount)
{
    vec3 gray = vec3(dot(vec3(1.0), color));
    //return gray;
    return vec3(mix(color, gray, amount));
}

// All components are in the range [0�1], including hue.
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0�1], including hue.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec4 celshading() {
    // Compute Cook-Torrance Lighting
    const float eps = 1e-8; //small value to avoid division by 0

    vec3 nw = normalize(inData.nw); //world-space unit normal vector
    if (mode == 1) {
        // need to recalc normal
        vec2 p = 2.0*gl_PointCoord.xy - vec2(1.0);
        float mag = dot(p, p); 
        if (mag > 1.0) discard; // discard if outside of radius length
        float z = sqrt(1.0 - mag);
        nw = normalize(vec3(p, z)); // normalize or no? 
    }
    vec3 lw = normalize(light_w.xyz - inData.pw.xyz); //world-space unit light vector
    vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);	//world-space unit view vector
    vec3 hw = normalize(lw + vw); // Halfway vector

    // reflect
    vec3 r = normalize(reflect(-lw, nw));
    float nl = dot(nw, lw);

    if (nl < 0) {
        // ambient 
        fragcolor = dark;
    } else {
        // if (specular >=0) : diffuse
        fragcolor = midtone  ;
    }

    if (pow(dot(r, vw), shininess) > 0.95) {
        fragcolor = highlight;
    }

    return fragcolor;
}

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

vec4 outline() {
    // add outlines (consistent thicknesS)
    mat3 I;

    // have minimum at 3, *5 for higher range
    // if not float then not smooth transition 
    float d = 1.0 - clamp(pow(length(vec3(eye_w) - inData.pw)/4, 1), 0, 1.0); // inverse so higher number closer
    // now turn 0-1 float to int (equally spaced if possible, but we don't know the range of d) unless we do the bounding box but that would be local 

    int linethickness = 5 + int(floor(d * 10));


    for (int i=0; i< linethickness; i++) {
        for (int j=0; j<linethickness; j++) {
            I[i][j] = texelFetch(fbo_tex, ivec2(gl_FragCoord) + ivec2(i-1 ,j-1), 0).g; // bw so any value is the same
           
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

vec4 phong() {
     //Compute per-fragment Phong lighting	

      const float eps = 1e-8; //small value to avoid division by 0
      float d = distance(light_w.xyz, inData.pw.xyz);
      float atten = 1.0/(d*d+eps); //d-squared attenuation

      vec3 nw = normalize(inData.nw);			//world-space unit normal vector
      vec3 lw = normalize(light_w.xyz - inData.pw.xyz);	//world-space unit light vector
      vec4 diffuse_term = atten*midtone*max(0.0, dot(nw, lw));

      vec3 vw = normalize(eye_w.xyz - inData.pw.xyz);	//world-space unit view vector
      vec3 rw = reflect(-lw, nw);	//world-space unit reflection vector

      vec4 specular_term = highlight*pow(max(0.0, dot(rw, vw)), shininess);

      return dark + diffuse_term + specular_term;
}