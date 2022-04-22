#include <windows.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "InitShader.h"    //Functions for loading shaders from text files
#include "LoadMesh.h"      //Functions for creating OpenGL buffers from mesh files
#include "LoadTexture.h"   //Functions for creating OpenGL textures from image files
#include "VideoMux.h"      //Functions for saving videos

int window_width = 1024;
int window_height = 1024;
float aspect = 1.0f;
const char* const window_title = "NPR-SPH";

static const std::string toon_vs("toon_vs.glsl");
static const std::string toon_fs("toon_fs.glsl");
static const std::string brush_gs("brush_gs.glsl");
static const std::string brush_fs("brush_fs.glsl");
static const std::string brush_vs("brush_vs.glsl");

enum render_style {toon, paint};
GLuint toon_shader_program = -1;
GLuint brush_shader_program = -1;

static const std::string mesh_name = "Amago0.obj";

GLuint fbo = -1;
GLuint fbo_tex = -1;
GLuint depthrenderbuffer;

float mesh_d;
float mesh_range;


//This structure mirrors the uniform block declared in the shader
struct SceneUniforms
{
    glm::mat4 P;	//camera projection * view matrix
    glm::mat4 V;
    glm::vec4 eye_w;	//world-space eye position
    glm::vec4 light_w = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f); //world-space light position
} SceneData;

struct MaterialUniforms
{
    glm::vec4 dark = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Ambient material color
    glm::vec4 midtone = glm::vec4(glm::vec3(0.5f), 1.0f); // Diffuse material color
    glm::vec4 highlight = glm::vec4(1.0f); // Specular material color
} MaterialData;

//IDs for the buffer objects holding the uniform block data
GLuint scene_ubo = -1;
GLuint material_ubo = -1;

GLuint noise_id = -1; //noise texture for hair placement

namespace UboBinding
{
    //These values come from the binding value specified in the shader block layout
    int scene = 0;
    int material = 1;
}

//Locations for the uniforms which are not in uniform blocks
namespace UniformLocs
{
    int M = 0; //model matrix
    int time = 1;
    int pass = 2;
    int mode = 3; 
    int mesh_d = 4;
    int mesh_range = 5;
    int scale = 6;
}

GLuint texture_id = -1; //Texture map for mesh
MeshData mesh_data;

float angle = 0.0f;
float scale = 1.0f;
bool recording = false;
int paint_mode = render_style::toon;

//For an explanation of this program's structure see https://www.glfw.org/docs/3.3/quick.html 

void draw_gui(GLFWwindow* window)
{
    //Begin ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //Draw Gui
    ImGui::Begin("Debug window");
    if (ImGui::Button("Quit"))
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    const int filename_len = 256;
    static char video_filename[filename_len] = "capture.mp4";

    ImGui::InputText("Video filename", video_filename, filename_len);
    ImGui::SameLine();
    if (recording == false)
    {
        if (ImGui::Button("Start Recording"))
        {
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            recording = true;
            start_encoding(video_filename, w, h); //Uses ffmpeg
        }

    }
    else
    {
        if (ImGui::Button("Stop Recording"))
        {
            recording = false;
            finish_encoding(); //Uses ffmpeg
        }
    }
    ImGui::Image((void*)fbo_tex, ImVec2(500.0f, 500.0f), ImVec2(0.0, 1.0), ImVec2(1.0, 0.0));

    ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
    ImGui::SliderFloat("Scale", &scale, -10.0f, +10.0f);
    ImGui::RadioButton("ToonShader", &paint_mode, render_style::toon);
    ImGui::RadioButton("Paint", &paint_mode, render_style::paint);

    ImGui::SliderFloat4("Light position", &SceneData.light_w.x, -1.0f, 1.0f);

    // Control Material colors
    ImGui::ColorEdit3("Toon Darkest Color", &MaterialData.dark.r, 0);
    ImGui::ColorEdit3("Toon Midtone Color", &MaterialData.midtone.r, 0);
    ImGui::ColorEdit3("Toon Highlight Color", &MaterialData.highlight.r, 0);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    //End ImGui Frame
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
 
// This function gets called every time the scene gets redisplayed
void display(GLFWwindow* window)
{
    //Clear the screen to the color previously specified in the glClearColor(...) call.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SceneData.eye_w = glm::vec4(0.0f, 1.0f, 3.0f, 1.0f);
    glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale * mesh_data.mScaleFactor));
    glm::mat4 V = glm::lookAt(glm::vec3(SceneData.eye_w.x, SceneData.eye_w.y, SceneData.eye_w.z), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 P = glm::perspective(glm::pi<float>() / 4.0f, aspect, 0.1f, 100.0f);
    SceneData.P = P;
    SceneData.V = V;

    // toon shader
    glUseProgram(toon_shader_program);

    //Set uniforms
    glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M));
    glUniform1f(UniformLocs::mesh_d, mesh_d);
    glUniform1f(UniformLocs::mesh_range, mesh_range);

    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.
    //glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo

    glBindBuffer(GL_UNIFORM_BUFFER, material_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUniforms), &MaterialData); //Upload the new uniform values.
    //glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo

    glBindVertexArray(mesh_data.mVao);

    // pass 1: render mesh depth into texture and get alpha mask
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glUniform1i(UniformLocs::pass, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Render to FBO.
    glBindTextureUnit(0, fbo_tex); // TODO make it comp name
    glDrawBuffer(GL_COLOR_ATTACHMENT0); //Out variable in frag shader will be written to the texture attached to GL_COLOR_ATTACHMENT0.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // draw fish into texture
    glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
       
    glUniform1i(UniformLocs::pass, 1);
    glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
    // unbind
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // add brush
    if (paint_mode == render_style::paint) {

        // draw the bw into fbo comp tex with extra stuff -> alpha 

        //geometry_shader draw brush strokes
        glUseProgram(brush_shader_program);

        // send selected paint mode
        glUniform1i(UniformLocs::mode, paint_mode);
        glUniform1f(UniformLocs::mesh_d, mesh_d);
        glUniform1f(UniformLocs::mesh_range, mesh_range);
        glUniform1f(UniformLocs::scale, scale);

        //Set uniforms
        glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M));

        glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.
        //glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo

        glBindBuffer(GL_UNIFORM_BUFFER, material_ubo); //Bind the OpenGL UBO before we update the data.
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUniforms), &MaterialData); //Upload the new uniform values.
        //glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo
        glBindVertexArray(mesh_data.mVao);

        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTextureUnit(0, fbo_tex);
        glUniform1i(UniformLocs::pass, 0);
        glDrawElements(GL_POINTS, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        // unbind
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // GRAB frame before gui draws
    if (recording == true)
    {
        glFinish();
        glReadBuffer(GL_BACK);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        read_frame_to_encode(&rgb, &pixels, w, h);
        encode_frame(rgb);
    }

    draw_gui(window);


    /* Swap front and back buffers */
    glfwSwapBuffers(window);
}

void idle()
{
    float time_sec = static_cast<float>(glfwGetTime());

    //Pass time_sec value to the shaders
    glUniform1f(UniformLocs::time, time_sec);
}

void prepare_shader(GLuint* shader_name, const char* vShaderFile, const char* gShaderFile, const char* fShaderFile) {
    GLuint shader = gShaderFile != NULL ? InitShader(vShaderFile, gShaderFile, fShaderFile) : InitShader(vShaderFile, fShaderFile);

    if (shader == -1) // loading failed
    {
        glClearColor(1.0f, 0.0f, 1.0f, 0.0f); //change clear color if shader can't be compiled
    }
    else
    {
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

        if (*shader_name != -1)
        {
            glDeleteProgram(*shader_name);
        }
        *shader_name = shader;
    }

}

void reload_shader()
{
    prepare_shader(&toon_shader_program, toon_vs.c_str(), NULL, toon_fs.c_str());
    prepare_shader(&brush_shader_program, brush_vs.c_str(), brush_gs.c_str(), brush_fs.c_str());
}

//This function gets called when a key is pressed
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    std::cout << "key : " << key << ", " << char(key) << ", scancode: " << scancode << ", action: " << action << ", mods: " << mods << std::endl;

    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case 'r':
        case 'R':
            reload_shader();
            break;

        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        }
    }
}

//This function gets called when the mouse moves over the window.
void mouse_cursor(GLFWwindow* window, double x, double y)
{
    //std::cout << "cursor pos: " << x << ", " << y << std::endl;
}

//This function gets called when a mouse button is pressed.
void mouse_button(GLFWwindow* window, int button, int action, int mods)
{
    //std::cout << "button : "<< button << ", action: " << action << ", mods: " << mods << std::endl;
}

void resize(GLFWwindow* window, int width, int height)
{
    window_width = width;
    window_height = height;

    glViewport(0, 0, width, height); //Set viewport to cover entire framebuffer
    aspect = float(width) / float(height); //Set aspect ratio used in view matrix calculation
}

//Initialize OpenGL state. This function only gets called once.
void initOpenGL()
{
    glewInit();

    //Print out information about the OpenGL version supported by the graphics driver.	
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    glEnable(GL_DEPTH_TEST);

    reload_shader();
    mesh_data = LoadMesh(mesh_name);

    // get mesh orthogonal tangents 

    // get distance between the 2 

    // do we only want z ? but we will have to rotate it anyways > this is local tho 
    mesh_d = 0.0 - mesh_data.mBbMin.z;
    // get d = 0 - min 
    // add d to vertex to offset everything to 0 
    mesh_range = glm::abs(mesh_data.mBbMin.z - mesh_data.mBbMax.z);
    // divide all by abs(min-max) 

    // get biggest texture size needed, so resize wont clip textures
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    float max_x = 0.0f;
    float max_y = 0.0f;
    for (int m = 0; m < count; m++) {
        
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[m]);
        float xscale = mode->width;
        float yscale = mode->height;

        if ((xscale * yscale) > (max_x * max_y)) {
            // maintain aspect ratio
            max_x = xscale;
            max_y = yscale;
        }
    }
    
    //Create a texture object and set initial wrapping and filtering state
    // R: BW value to get edges, G: depth value, B: alpha channel
    glGenTextures(1, &fbo_tex);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, max_x, max_y, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    //Create the framebuffer object
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    //http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
    // The depth buffer
    glGenRenderbuffers(1, &depthrenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, max_x, max_y);

    // bind fbo texture to render to 
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0);

    // attach depth renderbugger to FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

    //unbind the fbo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //Create and initialize uniform buffers
    glGenBuffers(1, &scene_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
    glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::scene, scene_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

    glGenBuffers(1, &material_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, material_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(MaterialUniforms), &MaterialData, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
    glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::material, material_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//C++ programs start executing in the main() function.
int main(int argc, char** argv)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
    {
        return -1;
    }

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(window_width, window_height, window_title, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    //Register callback functions with glfw. 
    glfwSetKeyCallback(window, keyboard);
    glfwSetCursorPosCallback(window, mouse_cursor);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetFramebufferSizeCallback(window, resize);

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    initOpenGL();

    //Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        idle();
        display(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}