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

#define SPH_NUM_PARTICLES 10000
#define SPH_PARTICLE_RADIUS 0.005f
#define SPH_WORK_GROUP_SIZE 1024
#define SPH_NUM_WORK_GROUPS ((SPH_NUM_PARTICLES + SPH_WORK_GROUP_SIZE - 1) / SPH_WORK_GROUP_SIZE) // Ceiling of particle count divided by work group size

const int init_window_width = 1024;
const int init_window_height = 1024;
const char* const window_title = "CGT 521 Final Project - NPR-SPH";

static const std::string vertex_shader("npr-sph_vs.glsl");
static const std::string fragment_shader("npr-sph_fs.glsl");
static const std::string force_comp_shader("force_comp.glsl");
static const std::string integrate_comp_shader("integrate_comp.glsl");
static const std::string rho_pres_com_shader("rho_pres_comp.glsl");

GLuint shader_program = -1;
GLuint compute_programs[3] = { -1, -1, -1 };
GLuint particle_position_vao = -1;
GLuint particles_ssbo = -1;

float angle = 0.0f;
float scale = 0.4f;
float aspect = 1.0f;
bool recording = false;

struct Particle
{
    glm::vec4 pos;
    glm::vec4 vel;
    glm::vec4 force;
    float rho;;
    float pres;
    float age;
};

//This structure mirrors the uniform block declared in the shader
struct SceneUniforms
{
    glm::mat4 PV; //camera projection * view matrix
    glm::vec4 eye_w; //world-space eye position
} SceneData;

GLuint scene_ubo = -1;
namespace UboBinding
{
    int scene = 0;
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

    ImGui::SliderFloat("View angle", &angle, -glm::pi<float>(), +glm::pi<float>());
    ImGui::SliderFloat("Scale", &scale, 0.0001f, 2.0f);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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

    SceneData.eye_w = glm::vec4(0.0f, -50.0f, -55.0f, 1.0f);
    glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, -50.0f, 0.0f)) * glm::scale(glm::vec3(scale));
    glm::mat4 V = glm::lookAt(glm::vec3(SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 P = glm::perspective(glm::pi<float>() / 4.0f, 1.0f, 0.1f, 100.0f);
    SceneData.PV = P * V;

    //Set uniforms
    glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M));

    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.
    glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo

    // Use compute shader
    glUseProgram(compute_programs[0]);
    glDispatchCompute(SPH_NUM_WORK_GROUPS, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glUseProgram(compute_programs[1]);
    glDispatchCompute(SPH_NUM_WORK_GROUPS, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glUseProgram(compute_programs[2]);
    glDispatchCompute(SPH_NUM_WORK_GROUPS, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles_ssbo);
    //glm::vec4* p = (glm::vec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

    glUseProgram(shader_program);
    glDrawArrays(GL_POINTS, 0, SPH_NUM_PARTICLES);

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
    float spacing = 2.5f;

    for (int i = 0; i < 100; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            for (int k = 0; k < 10; k++)
            {
                positions.push_back(glm::vec4((float)i * spacing, (float)j * spacing, (float)k * spacing, 1.0f));
            }
        }
    }

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

    //Print out information about the OpenGL version supported by the graphics driver.	
    std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    glEnable(GL_DEPTH_TEST);

    //Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); //additive alpha blending
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //semitransparent alpha blending

    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    // Initialize particle data
    std::vector<Particle> p(SPH_NUM_PARTICLES);
    
    // Get grid positions
    std::vector<glm::vec4> grid_positions = make_grid();
    /*float spacing = 5.0f;
    for (int i = 0; i < 100; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            for (int k = 0; k < 10; k++)
            {
                grid_positions.push_back(glm::vec4((float)i * spacing, (float)j * spacing, (float)k * spacing, 1.0f));
            }
        }
    }*/

    for (int i = 0; i < SPH_NUM_PARTICLES; i++)
    {
        p[i].pos = grid_positions[i];
        p[i].vel = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        p[i].force = glm::vec4(0.0f, -9.81f, 0.0f, 0.0f);
        p[i].rho = 2.0f;
        p[i].pres = 10.0f;
        p[i].age = 10.0f;
    }

    glGenBuffers(1, &particles_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particles_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Particle) * SPH_NUM_PARTICLES, p.data(), GL_STREAM_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particles_ssbo);

    glGenVertexArrays(1, &particle_position_vao);
    glBindVertexArray(particle_position_vao);

    glBindBuffer(GL_ARRAY_BUFFER, particles_ssbo);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), nullptr); // Bind buffer containing particle positions to VAO
    glEnableVertexAttribArray(0); // Enable attribute with location = 0 (vertex position) for VAO

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindVertexArray(particle_position_vao);

    reload_shader();

    //Create and initialize uniform buffers
    glGenBuffers(1, &scene_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneUniforms), nullptr, GL_STREAM_DRAW); //Allocate memory for the buffer, but don't copy (since pointer is null).
    glBindBufferBase(GL_UNIFORM_BUFFER, UboBinding::scene, scene_ubo); //Associate this uniform buffer with the uniform block in the shader that has the same binding.

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