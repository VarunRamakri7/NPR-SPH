#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 10000
#define PARTICLE_RADIUS 0.005f

// For calculations
#define DAMPING 0.3f // Boundary epsilon

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

layout(std140, binding = 2) uniform BoundaryUniform
{
    vec4 upper; // Upper bounds of particle area
    vec4 lower; // Lower bounds of particle area
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
    if (new_pos.x < lower.x)
    {
        new_pos.x = lower.x;
        new_vel.x *= -DAMPING;
    }
    else if (new_pos.x > upper.x)
    {
        new_pos.x = upper.x;
        new_vel.x *= -DAMPING;
    }
    
    if (new_pos.y < lower.y)
    {
        new_pos.y = lower.y;
        new_vel.y *= -DAMPING;
    }
    else if (new_pos.y > upper.y)
    {
        new_pos.y = upper.y;
        new_vel.y *= -DAMPING;
    }
    
    if (new_pos.z < lower.z)
    {
        new_pos.z = lower.z;
        new_vel.z *= -DAMPING;
    }
    else if (new_pos.z > upper.z)
    {
        new_pos.z = upper.z;
        new_vel.z *= -DAMPING;
    }

    // Assign calculated values
    particles[i].vel.xyz = new_vel;
    particles[i].pos.xyz = new_pos;
}