#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.1f
#define PARTICLE_MASS 3.5f
#define SMOOTHING_LENGTH (7.35f * PARTICLE_RADIUS)
#define PARTICLE_RESTING_DENSITY 200

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout(location = 1) uniform float time;

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

const float POLY6 = 4.0f / (PI * pow(SMOOTHING_LENGTH, 8.0f)); // Poly6 kernal
const float GAS_CONST = 2000.0f; // const for equation of state

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;
    
    // Compute Density (rho)
    float rho = 0.0f;

    // Iterate through all particles
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        vec3 delta = particles[j].pos.xyz - particles[i].pos.xyz; // Get vector between current particle and particle in vicinity
        float r = length(delta); // Get length of the vector
        if (r < SMOOTHING_LENGTH) // Check if particle is inside smoothing radius
        {
			rho += PARTICLE_MASS * POLY6 * pow(SMOOTHING_LENGTH * SMOOTHING_LENGTH - r, 3.0f);
        }
    }
    particles[i].extras[0] = rho; // Assign computed value
    
    // Compute Pressure
	particles[i].extras[1] = GAS_CONST * (rho - PARTICLE_RESTING_DENSITY);
}