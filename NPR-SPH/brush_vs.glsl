#version 440   

layout(location = 0) uniform mat4 M;
layout(location = 4) uniform float mesh_d;
layout(location = 5) uniform float mesh_range;
layout(location = 3) uniform int mode;
layout(location = 7) uniform float sim_rad;
layout(location = 6) uniform float scale;


layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 P;	//camera projection * view matrix
   mat4 V;
   vec4 eye_w;	//world-space eye position
   vec4 light_w; //world-space light position
};

in vec3 pos_attrib; //this variable holds the position of mesh vertices
in vec3 normal_attrib;  
in vec2 tex_coord_attrib ;


out VertexData
{
   vec3 pw;       //world-space vertex position
   vec3 nw;   //world-space normal vector
   float depth;
   vec2 tex_coord;
   vec3 pos_attrib;
} outData;

void main(void)
{
	gl_Position = M*vec4(pos_attrib, 1.0); //transform vertices and send result into pipeline
	outData.pw = vec3(M * vec4(pos_attrib, 1.0)); //world-space vertex position
	outData.nw = vec3(M* vec4(normal_attrib, 0.0));	//world-space normal vector
    outData.tex_coord = tex_coord_attrib;
	outData.pos_attrib = pos_attrib;
	// its just the edge of the model, we are looking at the z depth within the model (model space)
	outData.depth = ((pos_attrib.z + mesh_d) / mesh_range);
	
	if (mode == 0) {
		// mesh
		outData.nw = vec3(M*vec4(normal_attrib, 0.0));	//world-space normal normal vector
		outData.tex_coord = vec2(1.0,0.0) ;
		// its just the edge of the model, we are looking at the z depth within the model (model space)
		outData.depth = ((pos_attrib.z + mesh_d) / mesh_range);
	} else {
		// simulate
		outData.nw = vec3(1.0,0.0,0.0);	//world-space normal normal vector
		outData.tex_coord = vec2(1.0,0.0) ;
		// its just the edge of the model, we are looking at the z depth within the model (model space)
		outData.depth = 0.0f;
		gl_PointSize = sim_rad;
	} 

}