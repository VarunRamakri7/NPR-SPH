#version 430

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout(location = 2) uniform int pass;
layout(location = 3) uniform int paint_mode;

layout(std140, binding = 0) uniform SceneUniforms
{
   mat4 P;	//camera projection * view matrix
   mat4 V;
   vec4 eye_w;	//world-space eye position
   vec4 light_w; //world-space light position
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
} outData; 


void main() {    


    //Basic pass-through shader
	for(int n=0; n<gl_in.length(); ++n)
	{
//		 vec3 viewDirection = normalize(vec3(eye_w) - inData[n].pw);
//		 float _UnlitOutlineThickness = 0.0;
//		 float max_dilation = 1.0;
//		 float _LitOutlineThickness = max(max_dilation - (length(vec3(eye_w) - inData[n].pw)/2), 0.0); // clamp to 0
//	     if (dot(viewDirection, inData[n].nw) < 0)
//		 {
//			gl_Position = gl_in[n].gl_Position;
//			outData.tex_coord = inData[n].tex_coord;
//			outData.pw = inData[n].pw;
//			outData.depth = inData[n].depth;
//			outData.nw = inData[n].nw;
//			EmitVertex();
//		 }

			gl_Position = gl_in[n].gl_Position;
			outData.tex_coord = inData[n].tex_coord;
			outData.pw = inData[n].pw;
			outData.depth = inData[n].depth;
			outData.nw = inData[n].nw;
			EmitVertex();
	}
	EndPrimitive();

}