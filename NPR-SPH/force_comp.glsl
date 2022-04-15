#version 440

#define WORK_GROUP_SIZE 1024
#define NUM_PARTICLES 8000

// For calculations
#define PI 3.141592741f
#define PARTICLE_RADIUS 0.1f
#define PARTICLE_MASS 3.5f // Mass = Density * Volume
#define SMOOTHING_LENGTH (10.0f * PARTICLE_RADIUS)
#define PARTICLE_VISCOSITY 200.0f
#define GRAVITY_FORCE vec3(0.0, -10.0f, 0.0f)

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

const float SPIKY_GRAD = -10.0f / (PI * pow(SMOOTHING_LENGTH, 5.0f));
const float VISC_LAP = 40.0f / (PI * pow(SMOOTHING_LENGTH, 5.0f));

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if(i >= NUM_PARTICLES) return;

    // Compute all forces
    vec3 pres_force = vec3(0.0f);
    vec3 visc_force = vec3(0.0f);
    
    for (uint j = 0; j < NUM_PARTICLES; j++)
    {
        if (i == j)
        {
			continue;
		}

        vec3 delta = particles[i].pos.xyz - particles[j].pos.xyz; // Get vector between current particle and particle in vicinity
        float r = length(delta); // Get length of the vector
        if (r < SMOOTHING_LENGTH) // Check if particle is inside smoothing radius
        {
			pres_force += normalize(-delta) * PARTICLE_MASS * (particles[i].extras[1] - particles[j].extras[1]) / (2.0f - particles[j].extras[0]) * SPIKY_GRAD * pow(SMOOTHING_LENGTH - r, 3.0f);
			visc_force += PARTICLE_VISCOSITY * PARTICLE_MASS * (particles[j].vel.xyz - particles[i].vel.xyz) / particles[j].extras[0] * VISC_LAP * (SMOOTHING_LENGTH - r);
        }
    }

	vec3 grav_force = GRAVITY_FORCE * PARTICLE_MASS / particles[i].extras[0];
    particles[i].force.xyz = pres_force + visc_force + grav_force;
}