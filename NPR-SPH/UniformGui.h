#pragma once
#include <windows.h>
#include "GL/glew.h"

/*
Note: This function is intended to be used for prototyping only.

Future work TODO:
Priority 1. Handle uniform blocks
Priority 2. Handle per variable ranges cleanly
Priority 3. Do something smart with matrix uniforms, like if named V, set with glm::lookAt parameters, if named P set with glm::perspective parameters
*/

void UniformGuiBasic(GLuint program);
void UniformGui(GLuint program);
