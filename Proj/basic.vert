
#version 330 core
layout (location = 0) in vec2 aPos;

layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
   
    vec3 pos3D;
    float r = 1.0; 
    pos3D.x = aPos.x;
    pos3D.y = aPos.y;
  
    pos3D.z = sqrt(max(0.0, r*r - aPos.x*aPos.x - aPos.y*aPos.y));

  
    Normal = normalize(vec3(aPos.x, aPos.y, pos3D.z));
    
   
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * Normal);

    FragPos = vec3(model * vec4(aPos.x, aPos.y, pos3D.z, 1.0));
    
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos.x, aPos.y, pos3D.z, 1.0);
}
