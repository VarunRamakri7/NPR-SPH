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

const int init_window_width = 1024;
const int init_window_height = 1024;
const char* const window_title = "CGT 521 Final Project - NPR-SPH";

static const std::string vertex_shader("npr-sph_vs.glsl");
static const std::string fragment_shader("npr-sph_fs.glsl");
GLuint shader_program = -1;

float angle = 0.0f;
float scale = 0.2f;
float aspect = 1.0f;
bool recording = false;

//Ping-pong pairs of objects and buffers since we don't have simultaneous read/write access to VBOs.
GLuint vao[2] = { -1, -1 };
GLuint vbo[2] = { -1, -1 };
const int num_particles = 10000;
GLuint tfo[2] = { -1, -1 }; //transform feedback objects

//Nmaes of the VS out variables that should be captured by transform feedback
const char* xform_feedback_varyings[] = { "pos_out", "vel_out", "for_out", "rho_out", "pres_out", "age_out" };

namespace AttribLocs // Indices of varying variables in the xform feedback
{
    int pos = 0;
    int vel = 1;
    int force = 2;
    int rho = 3;
    int pres = 4;
    int age = 5;
}

//These indices get swapped every frame to perform the ping-ponging
int read_index = 0;  //initially read from VBO_ID[0]
int write_index = 1; //initially write to VBO_ID[1]

//This structure mirrors the uniform block declared in the shader
struct SceneUniforms
{
    glm::mat4 PV;	//camera projection * view matrix
    glm::vec4 eye_w;	//world-space eye position
} SceneData;

//IDs for the buffer objects holding the uniform block data
GLuint scene_ubo = -1;

namespace UboBinding
{
    //These values come from the binding value specified in the shader block layout
    int scene = 0;
}

//Locations for the uniforms which are not in uniform blocks
namespace UniformLocs
{
    int M = 0; //model matrix
    int time = 1;
}

//For an explanation of this program's structure see https://www.glfw.org/docs/3.3/quick.html 

void draw_gui(GLFWwindow* window)
{
    //Begin ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    UniformGui(shader_program);

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
    glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::vec3(scale));
    glm::mat4 V = glm::lookAt(glm::vec3(SceneData.eye_w), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 P = glm::perspective(glm::pi<float>() / 4.0f, 1.0f, 0.1f, 100.0f);
    SceneData.PV = P * V;

    glUseProgram(shader_program);

    //Set uniforms
    glUniformMatrix4fv(UniformLocs::M, 1, false, glm::value_ptr(M));

    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo); //Bind the OpenGL UBO before we update the data.
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &SceneData); //Upload the new uniform values.
    glBindBuffer(GL_UNIFORM_BUFFER, 0); //unbind the ubo

    glDepthMask(GL_FALSE);
    //BEGIN TRANSFORM FEEDBACK RENDERING
    const bool TFO_SUPPORTED = true;

    if (TFO_SUPPORTED == true)
    {
        //Bind the current write transform feedback object.
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[write_index]);
    }
    else
    {
        //Binding the transform feedback object recalls the buffer range state shown below. If
        //your system does not support transform feedback objects you can uncomment the following lines.
        const GLint pos_varying = 0;
        const GLint vel_varying = 1;
        const GLint for_varying = 2;
        const GLint rho_varying = 3;
        const GLint pres_varying = 4;
        const GLint age_varying = 5;

        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, pos_varying, vbo[write_index]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, vel_varying, vbo[write_index]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, for_varying, vbo[write_index]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, rho_varying, vbo[write_index]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, pres_varying, vbo[write_index]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, age_varying, vbo[write_index]);
    }

    //Prepare the pipeline for transform feedback
    glBeginTransformFeedback(GL_POINTS);
    glBindVertexArray(vao[read_index]);
    glDrawArrays(GL_POINTS, 0, num_particles);
    glEndTransformFeedback();

    if (TFO_SUPPORTED == true)
    {
        //Bind the current write transform feedback object.
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }

    //Ping-pong the buffers.
    read_index = 1 - read_index;
    write_index = 1 - write_index;

    //END TRANSFORM FEEDBACK RENDERING
    glDepthMask(GL_TRUE);

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

void transform_feedback_relink(GLuint shader_program)
{
    //You need to specify which varying variables will capture transform feedback values.
    glTransformFeedbackVaryings(shader_program, 3, xform_feedback_varyings, GL_INTERLEAVED_ATTRIBS);

    //Must relink the program after specifying transform feedback varyings.
    glLinkProgram(shader_program);
    int status;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        printProgramLinkError(shader_program);
    }
}

void reload_shader()
{
    GLuint new_shader = InitShader(vertex_shader.c_str(), fragment_shader.c_str());

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

        transform_feedback_relink(shader_program);
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
    //Set viewport to cover entire framebuffer
    glViewport(0, 0, width, height);
    //Set aspect ratio used in view matrix calculation
    aspect = float(width) / float(height);
}

std::vector<float> make_cube()
{
    std::vector<glm::vec3> positions;
    float spacing = -50.0f;
    for (int i = 0; i < 12 * 100; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            for (int k = 0; k < 10; k++)
            {
                positions.push_back(glm::vec3((float)i + spacing, (float)j + spacing, (float)k + spacing));
            }
        }
    }

    std::vector<float> buffer;
    for (int i = 0; i < 12 * num_particles; i++)
    {
        // Push position
        buffer.push_back(positions[i].x);
        buffer.push_back(positions[i].y);
        buffer.push_back(positions[i].z);

        // Push velocity
        buffer.push_back(0.0f);
        buffer.push_back(1.0f);
        buffer.push_back(0.0f);

        // Push forces
        buffer.push_back(0.0f);
        buffer.push_back(1.0f);
        buffer.push_back(0.0f);

        // Push pressue
        buffer.push_back(1.0f);

        // Push density
        buffer.push_back(1.0f);

        // Push age
        buffer.push_back(10.0f);
    }

    return buffer;
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

    //Enable gl_PointCoord in shader
    glEnable(GL_POINT_SPRITE);
    //Allow setting point size in fragment shader
    glEnable(GL_PROGRAM_POINT_SIZE);

    //create TFOs
    glGenTransformFeedbacks(2, tfo);

    //all attribs are initially zero
    std::vector<float> zeros = make_cube(); //particle positions, velocities, forces, densities, pressures, ages

    //These are the indices in the array passed to glTransformFeedbackVaryings
    // (const char *xform_feedback_varyings[] = { "pos_out", "vel_out", "for_out", "rho_out", "pres_out", "age_out" };)
    const GLint pos_varying = 0;
    const GLint vel_varying = 1;
    const GLint for_varying = 2;
    const GLint rho_varying = 3;
    const GLint pres_varying = 4;
    const GLint age_varying = 5;

    //create VAOs and VBOs
    glGenVertexArrays(2, vao);
    glGenBuffers(2, vbo);

    const int stride = 12 * sizeof(float);

    const int pos_offset = 0;
    const int vel_offset = sizeof(glm::vec3);
    const int for_offset = 2 * sizeof(glm::vec3);
    const int rho_offset = 3 * sizeof(glm::vec3);
    const int pres_offset = 3 * sizeof(glm::vec3) + sizeof(float);
    const int age_offset = 3 * sizeof(glm::vec3) + 2 * sizeof(float);

    const int pos_size = num_particles * sizeof(glm::vec3);
    const int vel_size = num_particles * sizeof(glm::vec3);
    const int for_size = num_particles * sizeof(glm::vec3);;
    const int rho_size = num_particles * sizeof(float);
    const int pres_size = num_particles * sizeof(float);
    const int age_size = num_particles * sizeof(float);

    for (int i = 0; i < 2; i++)
    {
        //Create VAO and VBO with interleaved attributes
        glBindVertexArray(vao[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * zeros.size(), zeros.data(), GL_DYNAMIC_COPY);

        glEnableVertexAttribArray(AttribLocs::pos);
        glVertexAttribPointer(AttribLocs::pos, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(pos_offset));

        glEnableVertexAttribArray(AttribLocs::vel);
        glVertexAttribPointer(AttribLocs::vel, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(vel_offset));

        glEnableVertexAttribArray(AttribLocs::force);
        glVertexAttribPointer(AttribLocs::force, 3, GL_FLOAT, false, stride, BUFFER_OFFSET(for_offset));

        glEnableVertexAttribArray(AttribLocs::rho);
        glVertexAttribPointer(AttribLocs::rho, 1, GL_FLOAT, false, stride, BUFFER_OFFSET(rho_offset));

        glEnableVertexAttribArray(AttribLocs::pres);
        glVertexAttribPointer(AttribLocs::pres, 1, GL_FLOAT, false, stride, BUFFER_OFFSET(pres_offset));

        glEnableVertexAttribArray(AttribLocs::age);
        glVertexAttribPointer(AttribLocs::age, 1, GL_FLOAT, false, stride, BUFFER_OFFSET(age_offset));

        //Tell the TFO where each varying variable should be written in the VBO.

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfo[i]);

        //Specify VBO to write into
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, pos_varying, vbo[i]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, vel_varying, vbo[i]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, for_varying, vbo[i]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, rho_varying, vbo[i]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, pres_varying, vbo[i]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, age_varying, vbo[i]);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        glBindVertexArray(0); //unbind vao
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind vbo
    }

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