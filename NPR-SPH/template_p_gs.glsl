#version 430

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

layout(location = 0) uniform mat4 M;

layout(location = 1) uniform float time;
layout(location = 2) uniform int pass;
layout(location = 3) uniform int paint_mode;
layout(location = 6) uniform float scale;

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
   float color;
} outData; 


void main() {    


    //Basic pass-through shader
//	for(int n=0; n<gl_in.length(); ++n)
//	{
////		 vec3 viewDirection = normalize(vec3(eye_w) - inData[n].pw);
////		 float _UnlitOutlineThickness = 0.0;
////		 float max_dilation = 1.0;
////		 float _LitOutlineThickness = max(max_dilation - (length(vec3(eye_w) - inData[n].pw)/2), 0.0); // clamp to 0
////	     if (dot(viewDirection, inData[n].nw) < 0)
////		 {
////			gl_Position = gl_in[n].gl_Position;
////			outData.tex_coord = inData[n].tex_coord;
////			outData.pw = inData[n].pw;
////			outData.depth = inData[n].depth;
////			outData.nw = inData[n].nw;
////			EmitVertex();
////		 }
//
//			outData.tex_coord = inData[n].tex_coord;
//			outData.pw = inData[n].pw;
//			outData.depth = inData[n].depth;
//			outData.nw = inData[n].nw;
//
//			gl_Position = gl_in[n].gl_Position + vec4(-0.1, 0.0, 0.0, 0.0);
//			EmitVertex();
//			gl_Position = gl_in[0].gl_Position + vec4(0.1, 0.0, 0.0, 0.0); 
//			EmitVertex();
//			gl_Position = gl_in[0].gl_Position + vec4( 0.1, 0.1, 0.0, 0.0);
//			EmitVertex();
//			gl_Position = gl_in[0].gl_Position + vec4( -0.1, 0.1, 0.0, 0.0);
//			EmitVertex();
//	}
//	EndPrimitive();

// square
//const vec2 coordinates [] = vec2 [] (vec2 (0.0f, 0.0f),
//                                       vec2 (0.1f, 0.0f),
//                                       vec2 (0.0f, 0.1f),
//                                       vec2 (0.1f, 0.1f));
const vec2 coordinates [] = vec2 [] (vec2 (0.0f, 0.0f),
                                       vec2 (0.1f, 0.0f),
                                       vec2 (0.0f, 0.05f),
                                       vec2 (0.1f, 0.05f));
                                       // ALLOW CHANGE OF BRUSH SIZE/ SHAPE? 
                                       // first add outline 
float opaque_min = 0.3;
float opaque_max = 1.0;
float trans_min = 0.0;
float trans_max = 0.3;

for(int i=0; i<gl_in.length(); ++i){
  //gl_Position = P*V*(gl_in[i].gl_Position) + 1.0*normalize(vec4(inData[i].nw, 0.0));
  for (int n = 0; n < 4; n++) {
    //gl_Position *= vec4(n, 0.0, 0.0, 1.0);
    gl_Position = P*V*(gl_in[i].gl_Position + (0.01)*scale*normalize(vec4(inData[i].nw, 0.0)));
    // squares
    gl_Position = gl_Position + (vec4(coordinates[n]*scale, 0.0,0.0));
    
    //gl_Position  = P*V*gl_Position;
    //gl_Position = P*V*(gl_in[i].gl_Position + (2.0*normalize(vec4(inData[i].nw, 0.0))));
    // use noise like the fur
    //tex_coord   =                               coordinates [i];
    outData.tex_coord = inData[i].tex_coord;
	outData.pw = inData[i].pw;
	outData.depth = inData[i].depth;
	outData.nw = inData[i].nw;

    // random from noise function
    float r = fract(sin(dot(inData[i].tex_coord,vec2(12.9898,78.233))) * 43758.5453);
    outData.color = trans_min + (trans_max - trans_min) * r;
    if ((n+1)%2) {
        outData.color = opaque_min + (opaque_max - opaque_min) * r;
    };
    // have range for starting and range for end to randomize (less) 
    EmitVertex ();
  }
  }

  EndPrimitive();
}