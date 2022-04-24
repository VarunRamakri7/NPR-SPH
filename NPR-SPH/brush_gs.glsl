#version 440

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

layout(location = 0) uniform mat4 M;
layout(location = 6) uniform float scale;
layout(location = 7) uniform float sim_rad;
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
   vec4 outline_color;
   float shininess;
   float brush_scale;
};

in VertexData
{
   vec3 pw;       //world-space vertex position
   vec3 nw;   //world-space normal vector
   float depth;
   vec2 tex_coord;
} inData[]; 

out VertexData
{
   vec3 pw;       //world-space vertex position
   vec3 nw;   //world-space normal vector
   float depth;
   vec2 tex_coord;
   float color;
} outData; 


void main() {    

    // sim -> use sim radius as fragment paint 
    const vec2 coordinates [] = vec2 [] (
        vec2 (-0.05f, -0.02f),
        vec2 (0.05f, -0.02f),
        vec2 (-0.05f, 0.02f),
        vec2 (0.05f, 0.02f)
    );

    // ALLOW CHANGE OF BRUSH SIZE/ SHAPE? 
    float opaque_min = 0.3;
    float opaque_max = 0.8;
    float trans_min = 0.0;
    float trans_max = 0.3;

    for(int i=0; i<gl_in.length(); ++i){
        for (int n = 0; n < 4; n++) {

        outData.tex_coord = inData[i].tex_coord;
	    outData.pw = inData[i].pw;
	    outData.depth = inData[i].depth;
	    outData.nw = inData[i].nw;

        // random from noise function
        float r = fract(sin(dot(inData[i].tex_coord,vec2(12.9898,78.233))) * 43758.5453)+0.001;

        // OA = r * n -> OA can be the point in world space 
        // y = 0, 
        // U = up and normal
        vec3 world_up = vec3(M*vec4(0,1,0,0));
        vec3 u = normalize(cross(world_up, normalize(outData.nw)));
        // W = normal and U 
        vec3 w = cross(normalize(outData.nw), u);
        // OFFSET FROM POINT
        float width = 0.03*scale*brush_scale; //0.03 do 0.01x0.01 to check
        float height = 0.02*scale*brush_scale; //0.02
        float n_offset = 0.0001;
        outData.color = trans_min + (trans_max - trans_min) * r;
        outData.color -= 0.01; // blur edge
        vec3 C1 = inData[i].pw + (n_offset*outData.nw) + (width / 2) * u + (height / 2) * w;
        vec3 C2 = inData[i].pw + (n_offset*outData.nw) + (width / 2) * u - (height / 2) * w;
        gl_Position = P*V*vec4(C1, 1.0);
        EmitVertex ();
        gl_Position = P*V*vec4(C2, 1.0);
        EmitVertex ();

        outData.color = opaque_min + (opaque_max - opaque_min) * r;
        outData.color -= 0.01; // blur edge
        vec3 C3 = inData[i].pw + (n_offset*outData.nw) - (width / 2) * u + (height / 2) * w;
        vec3 C4 = inData[i].pw + (n_offset*outData.nw) - (width / 2) * u - (height / 2) * w;

        gl_Position = P*V*vec4(C3, 1.0);
        EmitVertex ();
        gl_Position = P*V*vec4(C4, 1.0);
        EmitVertex ();
        }
    }

    EndPrimitive();

}