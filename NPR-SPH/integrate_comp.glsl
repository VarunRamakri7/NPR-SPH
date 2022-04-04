#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 1000

// For calculations
#define TIME_STEP 0.01f
#define WALL_DAMPING 0.3f

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(location = 0) uniform mat4 M;
layout(location = 1) uniform float time;

struct Particle
{
    vec4 pos;
    vec4 vel;
    vec4 force;
    float rho;
    float pres;
    float age;
};

layout(std430, binding = 0) buffer PARTICLES
{
    Particle particles[];
};

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;

    /*
    // Integrate all components
    vec3 acceleration = particles[i].force.xyz / particles[i].rho;
    vec3 new_vel = particles[i].vel.xyz + TIME_STEP * acceleration;
    vec3 new_pos = particles[i].pos.xyz + TIME_STEP * new_vel;

    // Boundary conditions. Keep particlex within [-1, 1] in all axis
    if (new_pos.x < -1.2f)
    {
        new_pos.x = -1.f;
        new_vel.x *= -1.0f * WALL_DAMPING;
    }
    else if (new_pos.x > 1.2f)
    {
        new_pos.x = 1.0f;
        new_vel.x *= -1.0f * WALL_DAMPING;
    }
    else if (new_pos.y < -1.2f)
    {
        new_pos.y = -1.0f;
        new_vel.y *= -1.0f * WALL_DAMPING;
    }
    else if (new_pos.y > 1.2f)
    {
        new_pos.y = 1.0f;
        new_vel.y *= -1.0f * WALL_DAMPING;
    }
    else if (new_pos.z < -1.2f)
    {
        new_pos.z = -1.0f;
        new_pos.z *= -1.0f * WALL_DAMPING;
    }
    else if (new_pos.z > 1.2f)
    {
        new_pos.z = 1.0f;
        new_pos.z *= -1.0f * WALL_DAMPING;
    }

    // Assign calculated values
    particles[i].vel.xyz = new_vel;
    particles[i].pos.xyz = new_pos;
    */
    
    // Placeholder
    //particles[i].vel.xyz += particles[i].force.xyz * TIME_STEP;
    particles[i].pos.y += -0.00981f;
}