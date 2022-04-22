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

#define NUM_PARTICLES 8000
#define PARTICLE_RADIUS 0.005f
#define WORK_GROUP_SIZE 1024
#define NUM_WORK_GROUPS 10 // Ceiling of particle count divided by work group size

const int init_window_width = 720;
const int init_window_height = 720;
const char* const window_title = "CGT 521 Final Project - NPR-SPH";

static const std::string vertex_shader("npr-sph_vs.glsl");
static const std::string fragment_shader("npr-sph_fs.glsl");
static const std::string rho_pres_com_shader("rho_pres_comp.glsl");
static const std::string force_comp_shader("force_comp.glsl");
static const std::string integrate_comp_shader("integrate_comp.glsl");

GLuint shader_program = -1;
GLuint compute_programs[3] = { -1, -1, -1 };
GLuint particle_position_vao = -1;
GLuint particles_ssbo = -1;

glm::vec3 eye = glm::vec3(10.0f, 2.0f, 0.0f);
glm::vec3 center = glm::vec3(0.0f, -1.0f, 0.0f);
float angle = 0.75f;
float scale = 2.5f;
float aspect = 1.0f;
bool recording = false;
bool simulate = false;

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
    glm::mat4 PV; //camera projection * view matrix
    glm::vec4 eye_w; //world-space eye position
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
	glm::vec4 upper = glm::vec4(1.0f);
	glm::vec4 lower = glm::vec4(-1.0f);
}BoundaryData;

GLuint scene_ubo = -1;
GLuint constants_ubo = -1;
GLuint boundary_ubo = -1;
namespace UboBinding
{
    int scene = 0;
    int constants = 1;
	int boundary = 2;
}

//Locations for the uniforms which are not in uniform blocks
namespace UniformLocs
{
    int M = 0; //model matrix
    int time = 1;
}

void draw_gui(GLFWwindow* window)
{
    //Begin ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //UniformGui(shader_program);

    //Draw Gui
    ImGui::Begin("Debug window");
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

    ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
    ImGui::SliderFloat("Scale", &scale, 0.0001f, 20.0f);
    ImGui::SliderFloat3("Camera Eye", &eye[0], -10.0f, 10.0f);
    ImGui::SliderFloat3("Camera Center", &center[0], -10.0f, 10.0f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::Begin("Constants Window");
    ImGui::SliderFloat("Mass", &ConstantsData.mass, 0.01f, 0.1f);
    ImGui::SliderFloat("Smoothing", &ConstantsData.smoothing_coeff, 7.0f, 10.0f);
    ImGui::SliderFloat("Viscosity", &ConstantsData.visc, 1000.0f, 5000.0f);
    ImGui::SliderFloat("Resting Density", &ConstantsData.resting_rho, 1000.0f, 5000.0f);
	ImGui::SliderFloat3("Upper Bounds", &BoundaryData.upper[0], 0.001f, 1.0f);
	ImGui::SliderFloat3("Lowwer Bounds", &BoundaryData.lower[0], -1.0f, -0.001f);
	ImGui::End();

    //static bool show_test = false;
    //ImGui::ShowDemoWindow(&show_test);

    //End ImGui Frame
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// This function gets called every time the scene gets redisplayed
void display(GLFWwindow* window)
{
    //Clear the screen to the color previously specified in the glClearColor(...) call.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SceneData.eye_w = glm::vec4(eye, 1.0f);
    glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale));
    glm::mat4 V = glm::lookAt(glm::vec3(SceneData.eye_w), center, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 P = glm::perspective(glm::pi<float>() / 4.0f, 1.0f, 0.1f, 100.0f);
    SceneData.PV = P * V;

    //Set uniforms
    glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M));

    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.

    glBindBuffer(GL_UNIFORM_BUFFER, constants_ubo); // Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ConstantsData), &ConstantsData); // Upload the new uniform values.

	glBindBuffer(GL_UNIFORM_BUFFER, boundary_ubo); // Bind the OpenGL UBO before we update the data.
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BoundaryUniform), &BoundaryData); // Upload the new uniform values.

    glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo

    // Use compute shader
    if (simulate)
    {
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

    //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles_ssbo);
    //Particle* p = (Particle*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    glUseProgram(shader_program);
    glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);

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
    //time_sec = static_cast<float>(glfwGetTime());

    static float time_sec = 0.0f;
    time_sec += 1.0f / 60.0f;

    //Pass time_sec value to the shaders
    glUniform1f(UniformLocs::time, time_sec);
}

void reload_shader()
{
    GLuint new_shader = InitShader(vertex_shader.c_str(), fragment_shader.c_str());

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

    if (new_shader == -1) // loading failed
    {
        glClearColor(1.0f, 0.0f, 1.0f, 0.0f); //change clear color if shader can't be compiled
    }
    else
    {
        glClearColor(0.35f, 0.35f, 0.35f, 0.0f);

        if (shader_program != -1)
        {
            glDeleteProgram(shader_program);
        }
        shader_program = new_shader;

        glLinkProgram(shader_program);
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

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind SSBO
    //glBindVertexArray(0); // Unbind VAO
    //glBindVertexArray(particle_position_vao);

    reload_shader();

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

	glGenBuffers(1, &boundary_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, boundary_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(BoundaryUniform), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
	glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::boundary, boundary_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

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