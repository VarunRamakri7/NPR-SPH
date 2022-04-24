#include <windows.h>

//When using this as a template, be sure to make these changes in the new project: 
//1. In Debugging properties set the Environment to PATH=%PATH%;$(SolutionDir)\lib;
//2. Change window_title below
//3. Copy assets (mesh and texture) to new project directory

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
#include <vector>

#include "InitShader.h"    //Functions for loading shaders from text files
#include "LoadTexture.h"   //Functions for creating OpenGL textures from image files
#include "VideoMux.h"      //Functions for saving videos
#include "DebugCallback.h"
#include "UniformGui.h"
#include "LoadMesh.h"

#define NUM_PARTICLES 8000
#define PARTICLE_RADIUS 0.005f
#define WORK_GROUP_SIZE 1024
#define NUM_WORK_GROUPS 10 // Ceiling of particle count divided by work group size

const int init_window_width = 720;
const int init_window_height = 720;
const char* const window_title = "CGT 521 Final Project - NPR-SPH";

GLuint shader_program = -1;
GLuint compute_programs[3] = { -1, -1, -1 };
GLuint particle_position_vao = -1;
//GLuint particle_velocity_vao = -1;
GLuint particles_ssbo = -1;

// compute shaders
static const std::string rho_pres_com_shader("rho_pres_comp.glsl");
static const std::string force_comp_shader("force_comp.glsl");
static const std::string integrate_comp_shader("integrate_comp.glsl");

// shaders
enum render_style { toon, paint };
GLuint toon_shader_program = -1;
GLuint brush_shader_program = -1;
static const std::string toon_vs("toon_vs.glsl");
static const std::string toon_fs("toon_fs.glsl");
static const std::string brush_gs("brush_gs.glsl");
static const std::string brush_fs("brush_fs.glsl");
static const std::string brush_vs("brush_vs.glsl");

float angle = 0.75f;
float scale = 2.5f;
float aspect = 1.0f;
bool recording = false;
int simulate = 0;
float mesh_d = 0.0f;
float mesh_range = 0.0f;
float simulation_radius = 10.0f;
glm::vec3 center = glm::vec3(0.0f);	//world-space eye position
int paint_mode = render_style::toon;

// mesh TODO change to something else
static const std::string mesh_name = "purdueobj.obj";

GLuint fbo = -1;
GLuint fbo_tex = -1;
GLuint depthrenderbuffer;
GLuint texture_id = -1; //Texture map for mesh

GLuint scene_ubo = -1;
GLuint constants_ubo = -1;
GLuint boundary_ubo = -1;
GLuint material_ubo = -1;

MeshData mesh_data;

struct Particle
{
    glm::vec4 pos;
    glm::vec4 vel;
    glm::vec4 force;
    glm::vec4 extras; // 0 - rho, 1 - pressure, 2 - age
};

//This structure mirrors the uniform block declared in the shader
struct SceneUniforms
{
    glm::mat4 P;	//camera projection * view matrix
    glm::mat4 V;
    glm::vec4 eye_w = glm::vec4(10.0f, 2.0f, 0.0f, -10.f);	//world-space eye position
    glm::vec4 light_w = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f); //world-space light position
} SceneData;

struct ConstantsUniform
{
    float mass = 0.02f; // Particle Mass
    float smoothing_coeff = 4.0f; // Smoothing length coefficient for neighborhood
    float visc = 2000.0f; // Fluid viscosity
    float resting_rho = 1000.0f; // Resting density
}ConstantsData;

struct BoundaryUniform
{
    glm::vec4 upper = glm::vec4(0.25f, 1.0f, 0.25f, 0.0f);
    glm::vec4 lower = glm::vec4(-0.25f, -0.5f, -0.25f, 0.0f);
}BoundaryData;

struct MaterialUniforms
{
    glm::vec4 dark = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Ambient material color
    glm::vec4 midtone = glm::vec4(glm::vec3(0.5f), 1.0); // Diffuse material color
    glm::vec4 highlight = glm::vec4(glm::vec3(1.0f), 1.0); // Specular material color
    glm::vec4 outline = glm::vec4(glm::vec3(0.0f), 1.0);
    float shininess = 0.3;
    float brush_scale = 1.0;
} MaterialData;

namespace UboBinding
{
    int scene = 0;
    int constants = 1;
    int boundary = 2;
    int material = 3;
}

//Locations for the uniforms which are not in uniform blocks
namespace UniformLocs
{
    int M = 0; //model matrix
    int style = 1;
    int pass = 2;
    int mode = 3; 
    int mesh_d = 4;
    int mesh_range = 5;
    int scale = 6;
    int sim_rad = 7;
}


void draw_gui(GLFWwindow* window)
{
    //Begin ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //UniformGui(shader_program);

    //Draw Gui
    ImGui::Begin("Visualization Window");
    if (ImGui::Button("Quit"))
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    const int filename_len = 256;
    static char video_filename[filename_len] = "capture.mp4";

    ImGui::InputText("Video filename", video_filename, filename_len);
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

    ImGui::RadioButton("ToonShader", &paint_mode, render_style::toon);
    ImGui::SameLine();
    ImGui::RadioButton("Paint", &paint_mode, render_style::paint);

    // Control Material colors
    ImGui::ColorEdit3("Outline Color", &MaterialData.outline.r, 0);
    ImGui::ColorEdit3("Toon Darkest Color", &MaterialData.dark.r, 0);
    ImGui::ColorEdit3("Toon Midtone Color", &MaterialData.midtone.r, 0);
    ImGui::ColorEdit3("Toon Highlight Color", &MaterialData.highlight.r, 0);
    ImGui::SliderFloat("Specular", &MaterialData.shininess, 0.0f, 1.0f);

    if (paint_mode == render_style::paint) {
        // add paint options
        ImGui::SliderFloat("Brush Size", &MaterialData.brush_scale, 0.0001f, 2.0f);
    }

    ImGui::RadioButton("Simulate", &simulate, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Mesh", &simulate, 0);

    if (simulate) {
        // add simulation options 
        ImGui::SliderFloat("Particle Size", &simulation_radius, 10.0f, 100.0f);
    }

    ImGui::SliderFloat("Scale", &scale, 0.0001f, 20.0f);
    ImGui::SliderFloat3("Camera Eye", &SceneData.eye_w[0], -100.0f, 100.0f);
    ImGui::SliderFloat3("Camera Center", &center[0], -100.0f, 100.0f);

    ImGui::SliderFloat3("Light position", &SceneData.light_w.x, -10.0f, 10.0f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::Begin("Constants Window");
    ImGui::SliderFloat("Mass", &ConstantsData.mass, 0.01f, 0.1f);
    ImGui::SliderFloat("Smoothing", &ConstantsData.smoothing_coeff, 7.0f, 10.0f);
    ImGui::SliderFloat("Viscosity", &ConstantsData.visc, 1000.0f, 5000.0f);
    ImGui::SliderFloat("Resting Density", &ConstantsData.resting_rho, 1000.0f, 5000.0f);
    ImGui::End();

    //static bool show_test = false;
    //ImGui::ShowDemoWindow(&show_test);

    //End ImGui Frame
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void sendUniforms() {

    glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale * mesh_data.mScaleFactor));
    // always looking at origin 
    glm::mat4 V = glm::lookAt(glm::vec3(SceneData.eye_w.x, SceneData.eye_w.y, SceneData.eye_w.z), center, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 P = glm::perspective(glm::pi<float>() / 4.0f, aspect, 0.1f, 100.0f);
    SceneData.P = P;
    SceneData.V = V;

    //Set uniforms
    glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M));
    glUniform1i(UniformLocs::style, paint_mode);
    glUniform1i(UniformLocs::mode, simulate);
    glUniform1f(UniformLocs::mesh_d, mesh_d);
    glUniform1f(UniformLocs::mesh_range, mesh_range);
    glUniform1f(UniformLocs::sim_rad, simulation_radius);
    glUniform1f(UniformLocs::scale, scale);

    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.

    glBindBuffer(GL_UNIFORM_BUFFER, constants_ubo); // Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ConstantsData), &ConstantsData); // Upload the new uniform values.

    glBindBuffer(GL_UNIFORM_BUFFER, boundary_ubo); // Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BoundaryUniform), &BoundaryData); // Upload the new uniform values.

    glBindBuffer(GL_UNIFORM_BUFFER, material_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUniforms), &MaterialData); //Upload the new uniform values.
}
 
// This function gets called every time the scene gets redisplayed
void display(GLFWwindow* window)
{
    //Clear the screen to the color previously specified in the glClearColor(...) call.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use compute shader
    if (simulate)
    {
        glBindVertexArray(particle_position_vao);

        glUseProgram(compute_programs[0]); // Use density and pressure calculation program
        glDispatchCompute(NUM_WORK_GROUPS, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glUseProgram(compute_programs[1]); // Use force calculation program
        glDispatchCompute(NUM_WORK_GROUPS, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glUseProgram(compute_programs[2]); // Use integration calculation program
        glDispatchCompute(NUM_WORK_GROUPS, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    else {
        // npr on mesh
        glBindVertexArray(mesh_data.mVao);
    }

    // toon shader
    glUseProgram(toon_shader_program);
    //Set uniforms
    sendUniforms();

    // pass 1: render mesh depth into texture and get alpha mask
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glUniform1i(UniformLocs::pass, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Render to FBO.
    glBindTextureUnit(0, fbo_tex); // TODO make it comp name
    glDrawBuffer(GL_COLOR_ATTACHMENT0); //Out variable in frag shader will be written to the texture attached to GL_COLOR_ATTACHMENT0.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // draw fish into texture
    if (simulate)
    {
        glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
    }
    else {
        glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUniform1i(UniformLocs::pass, 1);
    if (simulate)
    {
        glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
    }
    else {
        glDrawElements(GL_TRIANGLES, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
    }
    // unbind
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // add brush
    if (paint_mode == render_style::paint) {

        // draw the bw into fbo comp tex with extra stuff -> alpha 

        //geometry_shader draw brush strokes
        glUseProgram(brush_shader_program);
        sendUniforms();
    
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //glBindTextureUnit(0, fbo_tex);
        if (simulate)
        {
            glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);
        }
        else {
            glDrawElements(GL_POINTS, mesh_data.mSubmesh[0].mNumIndices, GL_UNSIGNED_INT, 0);
        }
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
    // time_sec = static_cast<float>(glfwGetTime());

    static float time_sec = 0.0f;
    time_sec += 1.0f / 60.0f;

    //Pass time_sec value to the shaders
    //glUniform1f(UniformLocs::time, time_sec);
}

void prepare_shader(GLuint* shader_name, const char* vShaderFile, const char* gShaderFile, const char* fShaderFile) {
    GLuint new_shader = gShaderFile != NULL ? InitShader(vShaderFile, gShaderFile, fShaderFile) : InitShader(vShaderFile, fShaderFile);

    if (new_shader == -1) // loading failed
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
        *shader_name = new_shader;
    }

}

void reload_shader()
{
    prepare_shader(&toon_shader_program, toon_vs.c_str(), NULL, toon_fs.c_str());
    prepare_shader(&brush_shader_program, brush_vs.c_str(), brush_gs.c_str(), brush_fs.c_str());

    // Load compute shaders
    GLuint compute_shader_handle = InitShader(rho_pres_com_shader.c_str());
    if (compute_shader_handle != -1)
    {
        compute_programs[0] = compute_shader_handle;
    }

    compute_shader_handle = InitShader(force_comp_shader.c_str());
    if (compute_shader_handle != -1)
    {
        compute_programs[1] = compute_shader_handle;
    }

    compute_shader_handle = InitShader(integrate_comp_shader.c_str());
    if (compute_shader_handle != -1)
    {
        compute_programs[2] = compute_shader_handle;
    }
}

//This function gets called when a key is pressed
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    //std::cout << "key : " << key << ", " << char(key) << ", scancode: " << scancode << ", action: " << action << ", mods: " << mods << std::endl;

    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case 'r':
        case 'R':
            reload_shader();
            break;

        case 'p':
        case 'P':
            simulate = !simulate;
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
    glViewport(0, 0, width, height); //Set viewport to cover entire framebuffer
    aspect = float(width) / float(height); // Set aspect ratio
}

/// <summary>
/// Make positions for a cube grid
/// </summary>
/// <returns>Vector of positions for the grid</returns>
std::vector<glm::vec4> make_grid()
{
    std::vector<glm::vec4> positions;

    // 20x20x20 Cube of particles within [0, 0.095] on all axes
    for (int i = 0; i < 20; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            for (int k = 0; k < 20; k++)
            {
                positions.push_back(glm::vec4((float)i * PARTICLE_RADIUS, (float)j * PARTICLE_RADIUS, (float)k * PARTICLE_RADIUS, 1.0f));
            }
        }
    }
    //std::cout << "Position count: " << positions.size() << std::endl;

    return positions;
}

#define BUFFER_OFFSET( offset )   ((GLvoid*) (offset))

//Initialize OpenGL state. This function only gets called once.
void initOpenGL()
{
    glewInit();

#ifdef _DEBUG
    RegisterCallback();
#endif

    int max_work_groups = -1;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_work_groups);

    //Print out information about the OpenGL version supported by the graphics driver.	
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Max work group invocations: " << max_work_groups << std::endl;
    glEnable(GL_DEPTH_TEST);

    //Enable alpha blending
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE); //additive alpha blending
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //semitransparent alpha blending

    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    // Initialize particle data
    std::vector<Particle> particles(NUM_PARTICLES);
    std::vector<glm::vec4> grid_positions = make_grid(); // Get grid positions
    for (int i = 0; i < NUM_PARTICLES; i++)
    {
        particles[i].pos = grid_positions[i];
        particles[i].vel = glm::vec4(0.0f); // Constant velocity along Y-Axis
        particles[i].force = glm::vec4(0.0f); // Gravity along the Y-Axis
        particles[i].extras = glm::vec4(0.0f); // 0 - rho, 1 - pressure, 2 - age
    }
    //std::cout << "Particles count: " << particles.size() << std::endl;

    // Generate and bind shader storage buffer
    glGenBuffers(1, &particles_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * NUM_PARTICLES, particles.data(), GL_STREAM_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particles_ssbo);

    // Generate and bind VAO for particle positions
    glGenVertexArrays(1, &particle_position_vao);
    glBindVertexArray(particle_position_vao);

    glBindBuffer(GL_ARRAY_BUFFER, particles_ssbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), nullptr); // Bind buffer containing particle positions to VAO
    glEnableVertexAttribArray(0); // Enable attribute with location = 0 (vertex position) for VAO

    /*glGenVertexArrays(1, &particle_velocity_vao);
    glBindVertexArray(particle_velocity_vao);*/

    //glBindBuffer(GL_ARRAY_BUFFER, particles_ssbo);
    //glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), nullptr); // Bind buffer containing particle velocity to VAO
    //glEnableVertexAttribArray(1); // Enable attribute with location = 0 (vertex position) for VAO

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind SSBO
    glBindVertexArray(0); // Unbind VAO
    //glBindVertexArray(particle_position_vao);

    reload_shader();
    mesh_data = LoadMesh(mesh_name);
    glBindVertexArray(0); // Unbind VAO

    // get mesh depth range from bounding box to compute normalized fading factor
    mesh_d = - mesh_data.mBbMin.z;
    // add mesh_d to vertex to offset everything to 0 and divide all by abs(min-max) 
    mesh_range = glm::abs(mesh_data.mBbMin.z - mesh_data.mBbMax.z);

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

    // attach depth renderbuffer to FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

    //unbind the fbo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //Create and initialize uniform buffers

    // For SceneUniforms
    glGenBuffers(1, &scene_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
    glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::scene, scene_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

    // For ConstantsUniform
    glGenBuffers(1, &constants_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, constants_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ConstantsUniform), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
    glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::constants, constants_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

    // boundary ubo
    glGenBuffers(1, &boundary_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, boundary_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(BoundaryUniform), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
    glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::boundary, boundary_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

    // for MaterialUniforms
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

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(init_window_width, init_window_height, window_title, NULL, NULL);
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