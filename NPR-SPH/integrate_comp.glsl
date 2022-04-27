#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define DAMPING 0.3f

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(location = 0) uniform mat4 M;

struct Particle
{
    vec4 pos;
    vec4 vel;
    vec4 force;
    vec4 extras; // 0 - rho, 1 - pressure, 2 - age
};

layout(std430, binding = 0) buffer PARTICLES
{
    Particle particles[];
};

const float dt = 1.0f / NUM_PARTICLES; // Time step

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;

    // Integrate all components
    vec3 acceleration = particles[i].force.xyz / particles[i].extras[0];
    vec3 new_vel = particles[i].vel.xyz + dt * acceleration;
    vec3 new_pos = particles[i].pos.xyz + dt * new_vel;

    // Boundary conditions
    if (new_pos.x < -1.0f)
    {
        new_pos.x = -1.0f;
        new_vel.x *= -DAMPING;
    }
    else if (new_pos.x > 1.0f)
    {
        new_pos.x = 1.0f;
        new_vel.x *= -DAMPING;
    }
    
    else if (new_pos.y < -1.0f)
    {
        new_pos.y = -1.0f;
        new_vel.y *= -DAMPING;
    }
    else if (new_pos.y > 1.0f)
    {
        new_pos.y = 1.0f;
        new_vel.y *= -DAMPING;
    }
    
    else if (new_pos.z < -1.0f)
    {
        new_pos.z = -1.0f;
        new_pos.z *= -DAMPING;
    }
    else if (new_pos.z > 1.0f)
    {
        new_pos.z = 1.0f;
        new_pos.z *= -DAMPING;
    }

    // Assign calculated values
    particles[i].vel.xyz = new_vel;
    particles[i].pos.xyz = new_pos;
}