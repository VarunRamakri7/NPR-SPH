#version 440

// constants
#define WORK_GROUP_SIZE 128
#define NUM_PARTICLES 10000

layout (local_size_x = WORK_GROUP_SIZE) in;

layout(std430, binding = 0) buffer position_block
{
    vec2 position[];
};

layout(std430, binding = 1) buffer velocity_block
{
    vec2 velocity[];
};

layout(std430, binding = 2) buffer force_block
{
    vec2 force[];
};

layout(std430, binding = 3) buffer density_block
{
    float density[];
};

layout(std430, binding = 4) buffer pressure_block
{
    float pressure[];
};

layout(std430, binding = 5) buffer age_block
{
    float age[];
};

void main()
{
    uint i = gl_GlobalInvocationID.x;

    velocity[i] += force[i] * 0.01f;
    position[i] += velocity[i] * 0.01f;
}