#include "UniformGui.h"
#include <glm/glm.hpp>
#include "imgui.h"
#include <vector>
#include <string>

//Helper functions
bool CheckboxN(const char* label, bool* b, int n);
std::string ArrayElementName(const std::string& name, int index);
bool IsColor(const std::string& name);

void UniformGuiBasic(GLuint program)
{
   //Get the number of uniforms in program
   int num_uniforms = 0;
   glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &num_uniforms);

   ImGui::Begin("Uniforms"); 

   std::vector<GLchar> name(256);
   std::vector<GLenum> properties;
   //These are the properties of each uniform variable we are going to get the values of 
   properties.push_back(GL_NAME_LENGTH);
   properties.push_back(GL_TYPE);
   properties.push_back(GL_LOCATION);
   properties.push_back(GL_ARRAY_SIZE);

   //The values of the properties will be in here
   std::vector<GLint> values(properties.size());

   for(int uniform_i = 0; uniform_i < num_uniforms; ++uniform_i)
   {
      //get uniform variable properties
      glGetProgramResourceiv(program, GL_UNIFORM, uniform_i, properties.size(),
         &properties[0], values.size(), NULL, &values[0]);
 
      //get uniform variable name
      glGetProgramResourceName(program, GL_UNIFORM, uniform_i, name.size(), NULL, &name[0]);
   
      const int type = values[1];
      const int loc = values[2];
      if(type == GL_SAMPLER_2D)//type = 2d texture
      {
         ImGui::Text(name.data());
         GLint binding, tex_id;
         glGetUniformiv(program, loc, &binding);
         glGetIntegeri_v(GL_TEXTURE_BINDING_2D, binding, &tex_id);
         ImGui::Image((void*)tex_id, ImVec2(256,256));
      }

      /// 
      /// Floats
      /// 

      if(type == GL_FLOAT)
      {
         float v;
         //Get the current value of the uniform variable
         glGetUniformfv(program, loc, &v);
         //Make a slider to change the uniform variable
         if(ImGui::SliderFloat(name.data(), &v, 0.0f, 1.0f))
         {
            //If the slider was dragged, update the value of the uniform
            glUniform1f(loc, v);
         }
      }
      if(type == GL_FLOAT_VEC2)
      {
         glm::vec2 v;
         glGetUniformfv(program, loc, &v.x);
         if(ImGui::SliderFloat2(name.data(), &v.x, 0.0f, 1.0f))
         {
            glUniform2fv(loc, 1, &v.x);
         }
      }
      if(type == GL_FLOAT_VEC3)
      {
         glm::vec3 v;
         glGetUniformfv(program, loc, &v.x);
         if(ImGui::SliderFloat3(name.data(), &v.x, 0.0f, 1.0f))
         {
            glUniform3fv(loc, 1, &v.x);
         }
      }
      if(type == GL_FLOAT_VEC4)
      {
         glm::vec4 v;
         glGetUniformfv(program, loc, &v.x);
         if(ImGui::SliderFloat4(name.data(), &v.x, 0.0f, 1.0f))
         {
            glUniform4fv(loc, 1, &v.x);
         }
      }

      /// 
      /// Int
      /// 

      if(type == GL_INT)
      {
         int v;
         glGetUniformiv(program, loc, &v);
         if(ImGui::SliderInt(name.data(), &v, 0.0f, 1.0f))
         {
            glUniform1i(loc, v);
         }
      }
      if(type == GL_INT_VEC2)
      {
         glm::ivec2 v;
         glGetUniformiv(program, loc, &v.x);
         if(ImGui::SliderInt2(name.data(), &v.x, 0.0f, 1.0f))
         {
            glUniform2iv(loc, 1, &v.x);
         }
      }
      if(type == GL_INT_VEC3)
      {
         glm::ivec3 v;
         glGetUniformiv(program, loc, &v.x);
         if(ImGui::SliderInt3(name.data(), &v.x, 0.0f, 1.0f))
         {
            glUniform3iv(loc, 1, &v.x);
         }
      }
      if(type == GL_INT_VEC4)
      {
         glm::ivec4 v;
         glGetUniformiv(program, loc, &v.x);
         if(ImGui::SliderInt4(name.data(), &v.x, 0.0f, 1.0f))
         {
            glUniform4iv(loc, 1, &v.x);
         }
      }

      /// 
      /// Bool
      /// 

      if(type == GL_BOOL)
      {
         int v;
         glGetUniformiv(program, loc, &v);
         bool bv = v;
         if(ImGui::Checkbox(name.data(), &bv))
         {
            v = bv;
            glUniform1i(loc, v);
         }
      }
      if(type == GL_BOOL_VEC2)
      {
         glm::ivec2 v;
         glGetUniformiv(program, loc, &v.x);
         glm::bvec2 bv(v);
         if(CheckboxN(name.data(), &bv.x, 2))
         {
            v = bv;
            glUniform2iv(loc, 1, &v.x);
         }
      }
      if(type == GL_BOOL_VEC3)
      {
         glm::ivec3 v;
         glGetUniformiv(program, loc, &v.x);
         glm::bvec3 bv(v);
         if(CheckboxN(name.data(), &bv.x, 3))
         {
            v = bv;
            glUniform3iv(loc, 1, &v.x);
         }
      }
      if(type == GL_BOOL_VEC4)
      {
         glm::ivec4 v;
         glGetUniformiv(program, loc, &v.x);
         glm::bvec4 bv(v);
         if(CheckboxN(name.data(), &bv.x, 4))
         {
            v = bv;
            glUniform4iv(loc, 1, &v.x);
         }
      } 
   }
   ImGui::End();
}

void UniformGui(GLuint program)
{
   const int MAX_NAME_LEN = 256;
   int num_uniforms = 0;
   glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &num_uniforms);

   std::string title("UniformGui: Program ");
   title += std::to_string(program);
   
   if(ImGui::Begin(title.c_str()))
   { 
      static glm::vec2 float_range(0.0f, 1.0f);
      static glm::ivec2 int_range(0, 255);
      if (ImGui::CollapsingHeader("Settings"))
      {
         ImGui::InputFloat2("Float range", &float_range[0]);
         ImGui::InputInt2("Int range", &int_range[0]);
      }


      std::string name;
      name.resize(MAX_NAME_LEN);
      std::vector<GLenum> properties;
      properties.push_back(GL_NAME_LENGTH);
      properties.push_back(GL_TYPE);
      properties.push_back(GL_LOCATION);
      properties.push_back(GL_ARRAY_SIZE);

      std::vector<GLint> values(properties.size());
      for(int uniform_i = 0; uniform_i < num_uniforms; ++uniform_i)
      {
         //get uniform name
         glGetProgramResourceiv(program, GL_UNIFORM, uniform_i, properties.size(),
            &properties[0], values.size(), NULL, &values[0]);
 
         glGetProgramResourceName(program, GL_UNIFORM, uniform_i, name.size(), NULL, &name[0]);
   
         const int type = values[1];
         const int loc = values[2];
         const int array_size = values[3];

         if(loc == -1) continue; //If loc == -1 it is a variable from a uniform block

         if(type == GL_SAMPLER_2D)//type = 2d texture
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               ImGui::Text(element_name.data());
               GLint binding, tex_id;
               glGetUniformiv(program, loc+i, &binding);
               glGetIntegeri_v(GL_TEXTURE_BINDING_2D, binding, &tex_id);
               ImGui::Image((void*)tex_id, ImVec2(256,256));
            }
         }

         /// 
         /// Floats
         /// 

         if(type == GL_FLOAT)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               float v;
               glGetUniformfv(program, loc+i, &v);
               if(ImGui::SliderFloat(name.data(), &v, float_range[0], float_range[1]))
               {
                  glUniform1f(loc+i, v);
               }
            }
         }
         if(type == GL_FLOAT_VEC2)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::vec2 v;
               glGetUniformfv(program, loc+i, &v.x);
               if(ImGui::SliderFloat2(element_name.data(), &v.x, float_range[0], float_range[1]))
               {
                  glUniform2fv(loc+i, 1, &v.x);
               }
            }
         }
         if(type == GL_FLOAT_VEC3)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::vec3 v;
               glGetUniformfv(program, loc+i, &v.x);

               if(IsColor(element_name))
               {
                  if(ImGui::ColorEdit3(element_name.data(), &v.x))
                  {
                     glUniform3fv(loc+i, 1, &v.x);
                  }
               }
               else
               {
                  if(ImGui::SliderFloat3(element_name.data(), &v.x, float_range[0], float_range[1]))
                  {
                     glUniform3fv(loc+i, 1, &v.x);
                  }
               }
            }
         }
         if(type == GL_FLOAT_VEC4)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::vec4 v;
               glGetUniformfv(program, loc+i, &v.x);

               if(IsColor(element_name))
               {
                  if(ImGui::ColorEdit4(element_name.data(), &v.x))
                  {
                     glUniform4fv(loc+i, 1, &v.x);
                  }
               }
               else
               {
                  if(ImGui::SliderFloat4(element_name.data(), &v.x, float_range[0], float_range[1]))
                  {
                     glUniform4fv(loc+i, 1, &v.x);
                  }
               }
            }
         }

         if(type == GL_FLOAT_MAT4)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               element_name.resize(strlen(element_name.c_str()));
               glm::mat4 M;
               glGetUniformfv(program, loc+i, &M[0].x);
               for(int c=0; c<4; c++)
               {
                  std::string column_name = element_name + "[" + std::to_string(c) + "]";
                  ImGui::PushID(c);
                  if(ImGui::SliderFloat4(column_name.data(), &M[c].x, float_range[0], float_range[1]))
                  {
                     glUniform4fv(loc+i, 1, &M[c].x);
                  }
                  ImGui::PopID();
               }
            }  
         }

         /// 
         /// Int
         /// 

         if(type == GL_INT)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               int v;
               glGetUniformiv(program, loc+i, &v);
               if(ImGui::SliderInt(element_name.data(), &v, int_range[0], int_range[1]))
               {
                  glUniform1i(loc+i, v);
               }
            }
         }
         if(type == GL_INT_VEC2)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::ivec2 v;
               glGetUniformiv(program, loc+i, &v.x);
               if(ImGui::SliderInt2(element_name.data(), &v.x, int_range[0], int_range[1]))
               {
                  glUniform2iv(loc+i, 1, &v.x);
               }
            }
         }
         if(type == GL_INT_VEC3)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::ivec3 v;
               glGetUniformiv(program, loc+i, &v.x);
               if(ImGui::SliderInt3(element_name.data(), &v.x, int_range[0], int_range[1]))
               {
                  glUniform3iv(loc+i, 1, &v.x);
               }
            }
         }
         if(type == GL_INT_VEC4)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::ivec4 v;
               glGetUniformiv(program, loc+i, &v.x);
               if(ImGui::SliderInt4(element_name.data(), &v.x, int_range[0], int_range[1]))
               {
                  glUniform4iv(loc+i, 1, &v.x);
               }
            }
         }

         /// 
         /// Bool
         /// 

         if(type == GL_BOOL)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               int v;
               glGetUniformiv(program, loc+i, &v);
               bool bv = v;
               if(ImGui::Checkbox(element_name.data(), &bv))
               {
                  v = bv;
                  glUniform1i(loc+i, v);
               }
            }
         }
         if(type == GL_BOOL_VEC2)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::ivec2 v;
               glGetUniformiv(program, loc+i, &v.x);
               glm::bvec2 bv(v);
               if(CheckboxN(element_name.data(), &bv.x, 2))
               {
                  v = bv;
                  glUniform2iv(loc+i, 1, &v.x);
               }
            }
         }
         if(type == GL_BOOL_VEC3)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::ivec3 v;
               glGetUniformiv(program, loc+i, &v.x);
               glm::bvec3 bv(v);
               if(CheckboxN(element_name.data(), &bv.x, 3))
               {
                  v = bv;
                  glUniform3iv(loc+i, 1, &v.x);
               }
            }
         }
         if(type == GL_BOOL_VEC4)
         {
            for(int i=0; i<array_size; i++)
            {
               std::string element_name = ArrayElementName(name, i);
               glm::ivec4 v;
               glGetUniformiv(program, loc+i, &v.x);
               glm::bvec4 bv(v);
               if(CheckboxN(element_name.data(), &bv.x, 4))
               {
                  v = bv;
                  glUniform4iv(loc+i, 1, &v.x);
               }
            }
         } 
      }
   }
   ImGui::End();
}

bool CheckboxN(const char* label, bool* b, int n)
{
   bool clicked = false;
   for(int i=0; i<n; i++)
   {
      ImGui::PushID(i);
      if(i<n-1)
      {
         if(ImGui::Checkbox("", &b[i]))
         {
            clicked = true;
         }
         ImGui::PopID();
         ImGui::SameLine();
      }
      else
      {
         if(ImGui::Checkbox(label, &b[i]))
         {
            clicked = true;
         }
         ImGui::PopID();
      }
   }
   return clicked;
}

std::string ArrayElementName(const std::string& name, int index)
{
   const std::string bracketed_zero("[0]");
   std::string element_name(name);
   size_t start_pos = name.find(bracketed_zero);
   if(start_pos == std::string::npos)
   {
      return element_name;
   }
   std::string index_str; index_str+='['; index_str+=std::to_string(index); index_str+=']';
   element_name.replace(start_pos, sizeof(bracketed_zero) - 1, index_str);
   return element_name;
}

inline bool ends_with(const std::string const & value, const std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool IsColor(const std::string& name)
{
   const std::string bracket("[");
   std::string element_name(name);
   size_t bracket_pos = name.find(bracket);
   element_name = element_name.substr(0, bracket_pos-1);
   element_name.resize(strlen(element_name.c_str()));

   if(ends_with(element_name, "rgb")) return true;
   if(ends_with(element_name, "rgba")) return true;
   return false;
}