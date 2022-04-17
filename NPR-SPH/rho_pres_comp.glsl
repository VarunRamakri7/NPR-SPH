#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.1f
//#define PARTICLE_MASS 3.5f
//#define SMOOTHING_LENGTH (7.35f * PARTICLE_RADIUS)
//#define PARTICLE_RESTING_DENSITY 125

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

layout(std140, binding = 1) uniform ConstantsUniform
{
    float mass; // Particle Mass
    float smoothing_coeff; // Smoothing length coefficient for neighborhood
    float visc; // Fluid viscosity
    float resting_rho; // Resting density
};

const float POLY6 = 4.0f / (PI * pow((smoothing_coeff * PARTICLE_RADIUS), 8.0f)); // Poly6 kernal
const float GAS_CONST = 2000.0f; // const for equation of state

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;
    
    float smoothing_length = smoothing_coeff * PARTICLE_RADIUS; // Smoothing length for neighbourhood

    // Compute Density (rho)
    float rho = 0.0f;

    // Iterate through all particles
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        vec3 delta = particles[j].pos.xyz - particles[i].pos.xyz; // Get vector between current particle and particle in vicinity
        float r = length(delta); // Get length of the vector
        if (r < smoothing_length) // Check if particle is inside smoothing radius
        {
			rho += mass * POLY6 * pow(smoothing_length * smoothing_length - r, 3.0f);
        }
    }
    particles[i].extras[0] = rho; // Assign computed value
    
    // Compute Pressure
	particles[i].extras[1] = GAS_CONST * (rho - resting_rho);
}