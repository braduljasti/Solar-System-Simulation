//Dejan Jovanovic RA-212-2021
//
//CONTROLS FOR SIMULATION:
//
//keys from 1 to 6->speed up or slow down(1 - slow down to 0.5x; 2 - normal speed; 3 - speed up 2x; 4 - speed up 5x; 5 - speed up 10x; 6 - speed up 20x)
//R -> click to go fullscreen or back to window
//SPACE -> pause/unpause simulation
//WASD -> move across 2D space
//O -> show/hide orbits of planets
//F -> return to Sun
//Scroll wheel up/down -> zoom in/ zoom out
// +- -> zoom in/ zoom out
//Left click on planetary body -> shows information about that body
//
//KONTROLE SIMULACIJE
// 
//tasteri od 1 do 6 -> ubrzaj ili uspori (1 - usopri na 0.5x; 2 - normalna brzina; 3 - ubrzaj 2x; 4 - ubrzaj 5x; 5 - ubrzaj 10x; 6 - ubrzaj 20x)
//R -> klikni za prelazak na cijeli ekran ili povratak na prozor
//SPACE -> pauziraj/ponisti pauzu simulacije
//WASD -> kretanje kroz 2D prostor
//O -> prikazi/sakrij orbite tijela
//F -> povratak na Sunce
//Tockic misa gore/dole -> zumiraj/odzumiraj
//+- -> zumiraj/odzumiraj
//Lijevi klik na nebesko tijelo -> prikazuje informacije o tom tijelu
//
//ZA POKRETANJE PROJEKTA IZBRISATI PACKAGES FOLDER, UCI U .SLN PONOVO I KLIKNUTI DESNIM KLIKOM NA SOLUTION I "RESTORE PACKAGES"
//za font,program koristi putanju "C:/Windows/Fonts/"
//DODATNE BIBLIOTEKE: 
//Freetype (text), stb_image.h(texture)

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>
#include <algorithm> 
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <thread>



const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1040;
const float PI = 3.14159265f;
const int ORBIT_RES = 100;

// Shader sources
const char* vertexShaderSource = R"(



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



)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 uCol;
uniform vec3 lightPos;
uniform float ambientStrength;
uniform bool isLightSource;
uniform sampler2D texture1;
uniform bool useTexture;

out vec4 FragColor;

void main() {
    if (isLightSource) {
        if(useTexture) {
            vec4 texColor = texture(texture1, TexCoords);
            FragColor = vec4(uCol * texColor.rgb, 1.0);
        } else {
            FragColor = vec4(uCol, 1.0);
        }
        return;
    }

   
    vec3 lightDir = normalize(lightPos - FragPos);
    
   
    float diff = max(dot(Normal, lightDir), 0.0);
    
    
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.0009 * distance);
    
   
    vec3 baseColor;
    if(useTexture) {
        baseColor = texture(texture1, TexCoords).rgb;
    } else {
        baseColor = uCol;
    }
    
   
    float darkSideAmbient = max(ambientStrength * 0.2, 0.08); 
    
   
    vec3 ambient = darkSideAmbient * baseColor;
    vec3 diffuse = diff * baseColor * 1.3; 
    
   
    float shadowFactor = smoothstep(0.0, 0.2, diff);
  
    vec3 result = (ambient + (diffuse * attenuation * shadowFactor)) * baseColor;
    
   
    float rim = 1.0 - max(dot(Normal, normalize(-FragPos)), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    vec3 rimColor = baseColor * rim * 0.25;
    
    result += rimColor;
    
    
    result = max(result, baseColor * 0.1);
    
   
    result = min(result, vec3(1.0));
    
    FragColor = vec4(result, 1.0);
}
)";





struct Character {
    unsigned int TextureID;
    glm::ivec2   Size;
    glm::ivec2   Bearing;
    unsigned int Advance;
};


struct Asteroid {
    float orbitRadius;
    float size;
    float orbitSpeed;
    float orbitOffset;
};

struct AsteroidBelt {
    std::string name;
    float minRadius;
    float maxRadius;
    int numAsteroids;
    std::vector<Asteroid> asteroids;
    glm::vec3 color;
    std::string info;
};

struct Moon {
    std::string name;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    glm::vec3 color;
    std::string texture;
    std::string info;
};

struct SolarObject {
    std::string name;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    float selfRotationSpeed;
    glm::vec3 color;
    bool drawOrbit;
    std::string info;
    bool hasRings;
    float ringInnerRadius;
    float ringOuterRadius;
    glm::vec3 ringColor;
    std::vector<Moon> moons;
};




class Shader {
private:
    unsigned int ID;

    void checkCompileErrors(unsigned int shader, const std::string& type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
            }
        }
    }

public:
    Shader(const char* vertexSrc, const char* fragmentSrc) {
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexSrc, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentSrc, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        use();
        glUniform1i(glGetUniformLocation(ID, "texture1"), 0);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    void use() {
        glUseProgram(ID);
    }

    void setMat4(const char* name, const glm::mat4& mat) {
        glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void setVec3(const char* name, const glm::vec3& value) {
        glUniform3fv(glGetUniformLocation(ID, name), 1, glm::value_ptr(value));
    }



    void setFloat(const char* name, float value) {
        glUniform1f(glGetUniformLocation(ID, name), value);
    }

    void setBool(const char* name, bool value) {
        glUniform1i(glGetUniformLocation(ID, name), value);
    }

    unsigned int getId() const {
        return ID;
    }

    ~Shader() {
        glDeleteProgram(ID);
    }
};

class TextRenderer {
private:
    std::map<char, Character> Characters;
    unsigned int VAO, VBO;
    std::unique_ptr<Shader> textShader;


    const char* textVertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex;
        out vec2 TexCoords;
        uniform mat4 projection;

        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* textFragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;

        void main() {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            color = vec4(textColor, 1.0) * sampled;
        }
    )";

public:
    TextRenderer(const char* fontPath) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
            return;
        }

        FT_Face face;
        if (FT_New_Face(ft, fontPath, 0, &face)) {
            std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
            return;
        }

        FT_Set_Pixel_Sizes(face, 0, 24);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                continue;
            }

            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<char, Character>(c, character));
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        textShader = std::make_unique<Shader>(textVertexShaderSource, textFragmentShaderSource);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    float GetTextWidth(const std::string& text, float scale) {
        float width = 0.0f;
        for (char c : text) {
            Character ch = Characters[c];
            width += (ch.Advance >> 6) * scale;
        }
        return width;
    }

    void RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color) {
        textShader->use();
        textShader->setVec3("textColor", color);

        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH),
            0.0f, static_cast<float>(SCR_HEIGHT));
        textShader->setMat4("projection", projection);

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        for (std::string::const_iterator c = text.begin(); c != text.end(); c++) {
            Character ch = Characters[*c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += (ch.Advance >> 6) * scale;
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~TextRenderer() {
        for (auto& ch : Characters) {
            glDeleteTextures(1, &ch.second.TextureID);
        }
    }
};

class Renderer {
private:
    unsigned int circleVAO, circleVBO;
    unsigned int ringVAO, ringVBO;
    std::unique_ptr<Shader> shader;
    glm::mat4 view;
    glm::mat4 projection;
    float currentTime;
    float& zoomLevel;
    unsigned int plutoOrbitVAO, plutoOrbitVBO;
    std::vector<AsteroidBelt> asteroidBelts;
    unsigned int asteroidVAO, asteroidVBO;
    std::map<std::string, unsigned int> textures;
    bool simulationPaused;
    float timeScale;

    void setupBuffers() {


        std::vector<float> circleVertices;
        for (int i = 0; i <= ORBIT_RES; i++) {
            float angle = 2.0f * PI * i / ORBIT_RES;
            float x = cos(angle);
            float y = sin(angle);


            float r = sqrt(x * x + y * y);
            float u = (x / r + 1.0f) * 0.5f;
            float v = (y / r + 1.0f) * 0.5f;


            float len = sqrt(x * x + y * y);
            float nx = x / len;
            float ny = y / len;
            float nz = 0.0f;

            // Position
            circleVertices.push_back(x);
            circleVertices.push_back(y);
            // Normal
            circleVertices.push_back(nx);
            circleVertices.push_back(ny);
            circleVertices.push_back(nz);
            // Texture coords
            circleVertices.push_back(u);
            circleVertices.push_back(v);
        }




        glGenVertexArrays(1, &circleVAO);
        glGenBuffers(1, &circleVBO);
        glBindVertexArray(circleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
        glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float),
            circleVertices.data(), GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
            (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // Texture coords attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
            (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);



        std::vector<float> plutoOrbitVertices;
        for (int i = 0; i <= ORBIT_RES; i++) {
            float angle = 2.0f * PI * i / ORBIT_RES;
            float x = 16.5f * cos(angle) - 3.0f;
            float y = 12.3f * 0.9f * sin(angle) + 1.2f;
            plutoOrbitVertices.push_back(x);
            plutoOrbitVertices.push_back(y);
            float len = sqrt(x * x + y * y);
            plutoOrbitVertices.push_back(x / len);
            plutoOrbitVertices.push_back(y / len);
            plutoOrbitVertices.push_back(0.0f);
        }
        glGenVertexArrays(1, &plutoOrbitVAO);
        glGenBuffers(1, &plutoOrbitVBO);
        glBindVertexArray(plutoOrbitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, plutoOrbitVBO);
        glBufferData(GL_ARRAY_BUFFER, plutoOrbitVertices.size() * sizeof(float),
            plutoOrbitVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);



        std::vector<float> ringVertices;
        for (int i = 0; i <= ORBIT_RES; i++) {
            float angle = 2.0f * PI * i / ORBIT_RES;
            float x = cos(angle);
            float y = sin(angle);

            ringVertices.push_back(x);
            ringVertices.push_back(y);
            ringVertices.push_back(x);
            ringVertices.push_back(y);
            ringVertices.push_back(0.0f);
        }


        glGenVertexArrays(1, &ringVAO);
        glGenBuffers(1, &ringVBO);
        glBindVertexArray(ringVAO);
        glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
        glBufferData(GL_ARRAY_BUFFER, ringVertices.size() * sizeof(float),
            ringVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        std::vector<float> asteroidVertices;
        for (int i = 0; i < ORBIT_RES / 2; i++) {
            float angle = 2.0f * PI * i / (ORBIT_RES / 2);
            float x = cos(angle);
            float y = sin(angle);
            float u = angle / (2.0f * PI);
            float v = 0.5f + y * 0.5f;

            // Calculate proper normal for lighting
            float len = sqrt(x * x + y * y);
            float nx = x / len;
            float ny = y / len;
            float nz = 0.0f;

            // Position
            asteroidVertices.push_back(x);
            asteroidVertices.push_back(y);
            // Normal
            asteroidVertices.push_back(nx);
            asteroidVertices.push_back(ny);
            asteroidVertices.push_back(nz);
            // Texture coordinates
            asteroidVertices.push_back(u);
            asteroidVertices.push_back(v);
        }
        glGenVertexArrays(1, &asteroidVAO);
        glGenBuffers(1, &asteroidVBO);
        glBindVertexArray(asteroidVAO);
        glBindBuffer(GL_ARRAY_BUFFER, asteroidVBO);
        glBufferData(GL_ARRAY_BUFFER, asteroidVertices.size() * sizeof(float),
            asteroidVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

    }
    void drawMoon(const Moon& moon, const glm::mat4& planetModel, float time, bool showOrbits) {
        shader->use();

        float baseAngle = time * moon.orbitSpeed;
        glm::vec3 planetPos = glm::vec3(planetModel[3]);

        glm::vec3 moonOffset(
            moon.orbitRadius * cos(baseAngle),
            moon.orbitRadius * sin(baseAngle),
            0.0f
        );

        glm::mat4 moonModel = glm::translate(glm::mat4(1.0f), planetPos + moonOffset);


        moonModel = glm::rotate(moonModel, time * moon.orbitSpeed * 5.0f,
            glm::vec3(0.0f, 0.0f, 1.0f));

        moonModel = glm::scale(moonModel, glm::vec3(moon.radius));

        shader->setMat4("model", moonModel);
        shader->setVec3("uCol", moon.color);

        std::string lowercaseTexName = moon.name;
        std::transform(lowercaseTexName.begin(), lowercaseTexName.end(),
            lowercaseTexName.begin(), ::tolower);

        if (textures.find(moon.name) != textures.end()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[moon.name]);
            shader->setBool("useTexture", true);
        }
        else {
            shader->setBool("useTexture", false);
        }

        glBindVertexArray(circleVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);

        if (showOrbits) {
            glm::mat4 orbitModel = glm::translate(glm::mat4(1.0f), planetPos);
            orbitModel = glm::scale(orbitModel, glm::vec3(moon.orbitRadius));
            shader->setMat4("model", orbitModel);
            shader->setVec3("uCol", glm::vec3(0.2f));
            glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
        }
    }


    unsigned int loadTexture(const char* path) {
        std::cout << "Attempting to load texture: " << path << std::endl;
        unsigned int textureID;
        glGenTextures(1, &textureID);

        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

        if (data) {
            std::cout << "Successfully loaded texture: " << path << " (" << width << "x" << height << ", " << nrChannels << " channels)" << std::endl;
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else {
            std::cout << "Failed to load texture: " << path << "\nError: " << stbi_failure_reason() << std::endl;
        }

        stbi_image_free(data);
        return textureID;
    }





    void drawRings(const SolarObject& obj, const glm::mat4& planetModel) {
        shader->use();
        shader->setVec3("lightPos", glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setFloat("ambientStrength", 0.1f);
        shader->setBool("isLightSource", false);


        shader->setBool("useTexture", false);
        glBindTexture(GL_TEXTURE_2D, 0);

        static std::vector<float> initialAngles;
        static bool initialized = false;

        if (!initialized) {
            initialAngles.clear();
            for (int i = 0; i < 600; i++) {
                initialAngles.push_back((static_cast<float>(rand()) / RAND_MAX) * 2.0f * PI);
            }
            initialized = true;
        }

        glBindVertexArray(ringVAO);
        glLineWidth(2.0f);


        glm::vec3 mainRingColor(0.4f, 0.35f, 0.15f);

        struct RingSection {
            float startRadius;
            float endRadius;
            int numRings;
            int numMeteors;
            float meteorSize;
            std::string meteorTexture;
        };

        std::vector<RingSection> sections = {
            {0.4f, 0.6f, 15, 200, 0.008f, "Meteor"},
            {0.6f, 0.8f, 7, 150, 0.007f, "Meteors"},
            {0.8f, 1.0f, 20, 250, 0.006f, "Meteorss"}
        };


        for (const auto& section : sections) {
            float ringStep = (section.endRadius - section.startRadius) / section.numRings;

            shader->setBool("useTexture", false);

            for (int i = 0; i <= section.numRings; i++) {
                float t = static_cast<float>(i) / section.numRings;
                glm::vec3 ringColor = glm::mix(
                    mainRingColor,
                    mainRingColor * 0.5f,
                    t
                );

                float radius = section.startRadius + i * ringStep;
                glm::mat4 currentRingModel = planetModel;
                currentRingModel = glm::scale(currentRingModel, glm::vec3(radius));

                shader->setMat4("model", currentRingModel);
                shader->setVec3("uCol", ringColor);
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
            }


            for (int i = 0; i < section.numRings / 4; i++) {
                float radius = section.startRadius + i * (section.endRadius - section.startRadius) / (section.numRings / 4);
                glm::mat4 currentRingModel = planetModel;
                currentRingModel = glm::scale(currentRingModel, glm::vec3(radius));

                shader->setMat4("model", currentRingModel);
                shader->setVec3("uCol", mainRingColor * 0.3f);
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
            }
        }

        glBindVertexArray(circleVAO);
        int meteorCounter = 0;
        glm::vec3 saturnPos = glm::vec3(planetModel[3]);
        float rotationSpeed = 0.25f;
        float currentRotation = simulationPaused ? currentTime : (currentTime * timeScale);

        for (const auto& section : sections) {

            if (textures.find(section.meteorTexture) != textures.end()) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures[section.meteorTexture]);
                shader->setBool("useTexture", true);
            }

            for (int i = 0; i < section.numMeteors; i++) {
                float radius = section.startRadius +
                    (static_cast<float>(i) / section.numMeteors) * (section.endRadius - section.startRadius);

                float baseAngle = initialAngles[meteorCounter++];
                float angle = baseAngle + (currentRotation * rotationSpeed);

                float meteorX = saturnPos.x + radius * cos(angle);
                float meteorY = saturnPos.y + radius * sin(angle);

                glm::mat4 meteorModel = glm::mat4(1.0f);
                meteorModel = glm::translate(meteorModel, glm::vec3(meteorX, meteorY, saturnPos.z));
                meteorModel = glm::scale(meteorModel, glm::vec3(section.meteorSize));
                meteorModel = glm::rotate(meteorModel, angle, glm::vec3(0.0f, 0.0f, 1.0f));

                shader->setMat4("model", meteorModel);
                shader->setVec3("uCol", glm::vec3(1.0f));
                glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);
            }
        }


        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
        glLineWidth(1.0f);
    }



public:
    void setCurrentTime(float time) { currentTime = time; }
    const glm::mat4& getCurrentView() const { return view; }
    void setSimulationPaused(bool paused) { simulationPaused = paused; }
    void setTimeScale(float scale) { timeScale = scale; }
    Renderer(float& zoomRef) : zoomLevel(zoomRef), simulationPaused(false), timeScale(1.0f) {
        shader = std::make_unique<Shader>(vertexShaderSource, fragmentShaderSource);
        setupBuffers();

        view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, zoomLevel),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        projection = glm::perspective(glm::radians(60.0f),
            (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 200.0f);

        shader->use();
        shader->setMat4("view", view);
        shader->setMat4("projection", projection);
    }

    void loadTextures() {
        const std::vector<std::string> objectNames = {
            "sun", "mercury", "venus", "earth", "mars", "phobos", "deimos",
            "jupiter",  "europa", "ganymede", "callisto", "io",
            "saturn", "enceladus", "tethys", "rhea", "titan", "iapetus",
            "uranus", "miranda", "titania", "oberon",
            "neptune",  "triton",
            "pluto", "eris", "moon", "nix", "dysnomia", "phobos", "deimos", "charon", "asteroid", "meteor", "meteors", "meteorss"
        };

        for (const auto& name : objectNames) {
            std::string path = "textures/" + name + ".jpg";
            unsigned int textureID = loadTexture(path.c_str());
            std::string objName = name;
            objName[0] = std::toupper(objName[0]);
            textures[objName] = textureID;
        }
    }

    void drawObject(const SolarObject& obj, float time, bool showOrbits) {
        shader->use();
        shader->setVec3("lightPos", glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setBool("isLightSource", obj.name == "Sun");

        glBindVertexArray(circleVAO);



        if (obj.name == "Sun") {
            shader->setFloat("ambientStrength", 1.0f);


            if (textures.find("Sun") != textures.end()) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures["Sun"]);
                shader->setBool("useTexture", true);
            }
            else {
                shader->setBool("useTexture", false);
            }
        }
        else {
            shader->setFloat("ambientStrength", 0.1f);

            if (textures.find(obj.name) != textures.end()) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures[obj.name]);
                shader->setBool("useTexture", true);
                shader->setFloat("ambientStrength", 0.5f);
            }
            else {
                shader->setBool("useTexture", false);
            }
        }


        if (obj.name == "Pluto" || obj.name == "Eris") {
            float angle = time * obj.orbitSpeed;
            float x, y;

            if (obj.name == "Pluto") {
                x = 121.5f * cos(angle) + 12.0f;
                y = 150.3f * 0.9f * sin(angle) - 49.2f;
            }
            else { // Eris
                x = 255.6f * cos(angle) - 78.0f;
                y = 140.4f * 0.85f * sin(angle) + 21.0f;
            }

            // Draw orbit path if enabled
            if (showOrbits) {
                shader->setMat4("model", glm::mat4(1.0f));
                shader->setVec3("uCol", glm::vec3(0.3f));

                std::vector<float> orbitVertices;
                for (int i = 0; i <= ORBIT_RES; i++) {
                    float a = 2.0f * PI * i / ORBIT_RES;
                    if (obj.name == "Pluto") {
                        orbitVertices.push_back(121.5f * cos(a) + 12.0f);
                        orbitVertices.push_back(150.3f * 0.9f * sin(a) - 49.2f);
                    }
                    else {
                        orbitVertices.push_back(255.6f * cos(a) - 78.0f);
                        orbitVertices.push_back(140.4f * 0.85f * sin(a) + 21.0f);
                    }
                }

                unsigned int orbitVBO, orbitVAO;
                glGenVertexArrays(1, &orbitVAO);
                glGenBuffers(1, &orbitVBO);
                glBindVertexArray(orbitVAO);
                glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
                glBufferData(GL_ARRAY_BUFFER, orbitVertices.size() * sizeof(float), orbitVertices.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES + 1);
                glDeleteVertexArrays(1, &orbitVAO);
                glDeleteBuffers(1, &orbitVBO);
                glBindVertexArray(circleVAO);
            }


            glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
            glm::mat4 model = glm::scale(
                glm::rotate(baseModel, time * obj.selfRotationSpeed, glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::vec3(obj.radius)
            );

            shader->setMat4("model", model);
            shader->setVec3("uCol", obj.color);
            glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);


            for (const auto& moon : obj.moons) {
                drawMoon(moon, baseModel, time, showOrbits);
            }
        }
        else {

            if (obj.drawOrbit && showOrbits) {
                glm::mat4 orbitModel = glm::scale(glm::mat4(1.0f),
                    glm::vec3(obj.orbitRadius, obj.orbitRadius, 1.0f));
                shader->setMat4("model", orbitModel);
                shader->setVec3("uCol", glm::vec3(0.3f));
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
            }

            float angle = time * obj.orbitSpeed;
            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                glm::vec3(obj.orbitRadius * cos(angle),
                    obj.orbitRadius * sin(angle), 0.0f));
            model = glm::rotate(model, time * obj.selfRotationSpeed,
                glm::vec3(0.0f, 0.0f, 1.0f));

            shader->setMat4("model", glm::scale(model, glm::vec3(obj.radius)));
            shader->setVec3("uCol", obj.color);
            glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);


            for (const auto& moon : obj.moons) {
                drawMoon(moon, model, time, showOrbits);
            }


            if (obj.hasRings) {
                drawRings(obj, model);
            }
        }


        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
    }

    void initializeAsteroidBelts() {

        AsteroidBelt mainBelt{
            "Main Asteroid Belt",
            6.3f,  // minRadius 
            9.9f,  // maxRadius
            1500,   // numAsteroids
            {},    // asteroids vector
            glm::vec3(0.6f, 0.6f, 0.6f),  // color
            "\nLocated between Mars and Jupiter\nContains millions of asteroids"
        };

       
        AsteroidBelt kuiperBelt{
            "Kuiper Belt",
            138.0f,  // minRadius 
             198.0f,  // maxRadius
            10000,   // numAsteroids
            {},    // asteroids vector
            glm::vec3(0.6f, 0.6f, 0.6f),  // color
            "\nBeyond Neptune's orbit\nHome to many dwarf planets"
        };

        //random asteroids for each belt
        for (auto& belt : { &mainBelt, &kuiperBelt }) {
            for (int i = 0; i < belt->numAsteroids; i++) {
                float radius = belt->minRadius + static_cast<float>(rand()) / RAND_MAX * (belt->maxRadius - belt->minRadius);
                float size = 0.002f + static_cast<float>(rand()) / RAND_MAX * 0.02f;
                float speed = 0.002f + static_cast<float>(rand()) / RAND_MAX * 0.004f;
                float offset = static_cast<float>(rand()) / RAND_MAX * 2 * PI;

                belt->asteroids.push_back({ radius, size, speed, offset });
            }
        }

        asteroidBelts = { mainBelt, kuiperBelt };
    }

    void drawAsteroidBelts(float time) {
        shader->use();
        shader->setVec3("lightPos", glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setFloat("ambientStrength", 0.5f);
        shader->setBool("isLightSource", false);

        // Apply asteroid texture
        if (textures.find("Asteroid") != textures.end()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures["Asteroid"]);
            shader->setBool("useTexture", true);
        }

        glBindVertexArray(asteroidVAO);

        for (const auto& belt : asteroidBelts) {
            shader->setVec3("uCol", belt.color);

            for (const auto& asteroid : belt.asteroids) {
                float angle = time * asteroid.orbitSpeed + asteroid.orbitOffset;

                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                    glm::vec3(asteroid.orbitRadius * cos(angle),
                        asteroid.orbitRadius * sin(angle), 0.0f));

                model = glm::rotate(model, angle * 0.5f + asteroid.orbitOffset,
                    glm::vec3(0.0f, 0.0f, 1.0f));

                model = glm::scale(model, glm::vec3(asteroid.size));

                shader->setMat4("model", model);
                glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES / 2);
            }
        }


        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
    }

    const std::vector<AsteroidBelt>& getAsteroidBelts() const {
        return asteroidBelts;
    }



    void setViewMatrix(const glm::mat4& newView) {
        view = newView;
    }

    void updateCamera() {
        shader->use();
        shader->setMat4("view", view);
    }

    ~Renderer() {
        glDeleteVertexArrays(1, &circleVAO);
        glDeleteBuffers(1, &circleVBO);
        glDeleteVertexArrays(1, &ringVAO);
        glDeleteBuffers(1, &ringVBO);
        glDeleteVertexArrays(1, &asteroidVAO);
        glDeleteBuffers(1, &asteroidVBO);
        glDeleteVertexArrays(1, &plutoOrbitVAO);
        glDeleteBuffers(1, &plutoOrbitVBO);
    }
};



class StarfieldBackground {
private:
    struct Star {
        float x, y;
        float brightness;
        float twinkleSpeed;
        float twinklePhase;
        glm::vec3 color;
    };



    const std::vector<glm::vec3> starColors = {
  glm::vec3(0.85f, 0.90f, 1.00f),  // Blue-white (O type)
  glm::vec3(1.00f, 1.00f, 1.00f),  // White (A type)
  glm::vec3(1.00f, 0.95f, 0.80f),  // Yellow-white (F type)
  glm::vec3(1.00f, 0.85f, 0.60f),  // Yellow (G type)
  glm::vec3(1.00f, 0.75f, 0.40f),  // Orange (K type)
  glm::vec3(1.00f, 0.50f, 0.20f),  // Red (M type)
  glm::vec3(0.70f, 0.70f, 1.00f),  // Blue giants
  glm::vec3(0.90f, 0.60f, 0.60f)   // Red giants
    };

    std::vector<Star> stars;
    unsigned int starVAO, starVBO;
    std::unique_ptr<Shader> starShader;

    const char* starVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        uniform mat4 view;
        uniform mat4 projection;
        uniform float brightness;
        
        out float starBrightness;
        
        void main() {
            gl_Position = projection * view * vec4(aPos, 0.0, 1.0);
            starBrightness = brightness;
        }
    )";

    const char* starFragmentShader = R"(
        #version 330 core
        in float starBrightness;
        uniform vec3 starColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vec4(starColor * starBrightness, starBrightness);
        }
    )";

public:
    StarfieldBackground(int numStars = 1000, float fieldSize = 200.0f) {
        starShader = std::make_unique<Shader>(starVertexShader, starFragmentShader);

        stars.resize(numStars);
        for (auto& star : stars) {
            star.x = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * fieldSize;
            star.y = (float(rand()) / RAND_MAX * 2.0f - 1.0f) * fieldSize;
            star.brightness = float(rand()) / RAND_MAX * 0.5f + 0.5f;
            star.twinkleSpeed = float(rand()) / RAND_MAX * 2.0f + 1.0f;
            star.twinklePhase = float(rand()) / RAND_MAX * 2.0f * PI;
            // Randomly assign a star color
            star.color = starColors[rand() % starColors.size()];
        }


        glGenVertexArrays(1, &starVAO);
        glGenBuffers(1, &starVBO);
        glBindVertexArray(starVAO);

        std::vector<float> vertices;
        vertices.reserve(numStars * 2);
        for (const auto& star : stars) {
            vertices.push_back(star.x);
            vertices.push_back(star.y);
        }

        glBindBuffer(GL_ARRAY_BUFFER, starVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    void render(const glm::mat4& view, const glm::mat4& projection, float currentTime) {
        starShader->use();
        starShader->setMat4("view", view);
        starShader->setMat4("projection", projection);

        glBindVertexArray(starVAO);
        glPointSize(2.0f);

        for (size_t i = 0; i < stars.size(); i++) {
            float brightness = stars[i].brightness *
                (0.8f + 0.2f * sin(currentTime * stars[i].twinkleSpeed + stars[i].twinklePhase));

            starShader->setFloat("brightness", brightness);
            starShader->setVec3("starColor", stars[i].color);
            glDrawArrays(GL_POINTS, i, 1);
        }
    }

    ~StarfieldBackground() {
        glDeleteVertexArrays(1, &starVAO);
        glDeleteBuffers(1, &starVBO);
    }
};







// Global variables
std::vector<SolarObject> solarSystem;
float timeScale = 1.0f;
bool simulationPaused = false;
bool showOrbits = true;
double mouseX, mouseY;
std::string selectedObjectInfo = "";
std::string selectedObjectName = "";
std::string selectedObjectDescription = "";
std::unique_ptr<TextRenderer> textRenderer;
double lastMouseX = 0, lastMouseY = 0;
float currentTime = 0.0f;
float zoomLevel = 20.0f;
const float MIN_ZOOM = 0.2f;
const float MAX_ZOOM = 150.0f;
std::unique_ptr<Renderer> renderer = nullptr;
glm::mat4 view;
glm::vec3 cameraPosition(0.0f, 0.0f, zoomLevel);
glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
float cameraSpeed = 3.0f;
std::unique_ptr<StarfieldBackground> starfield;
bool isFullscreen = false;
double lastTime = 0.0;
int frameCount = 0;
double lastFPSUpdate = 0.0;
int currentFPS = 0;


float clamp(float value, float min, float max) {
    return std::min(std::max(value, min), max);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (!renderer) return;

    float deltaTime = 0.016f;
    float currentSpeed = cameraSpeed * deltaTime * zoomLevel * 0.25f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPosition.y += currentSpeed;
        cameraTarget.y += currentSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPosition.y -= currentSpeed;
        cameraTarget.y -= currentSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPosition.x -= currentSpeed;
        cameraTarget.x -= currentSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPosition.x += currentSpeed;
        cameraTarget.x += currentSpeed;
    }

    glm::mat4 newView = glm::lookAt(
        cameraPosition,
        cameraTarget,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    renderer->setViewMatrix(newView);

    //zoom 
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    float ndcX = (2.0f * mouseX) / SCR_WIDTH - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / SCR_HEIGHT;

    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
            zoomLevel = clamp(zoomLevel - 0.3f, MIN_ZOOM, MAX_ZOOM);
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
            zoomLevel = clamp(zoomLevel + 0.3f, MIN_ZOOM, MAX_ZOOM);
        }

        cameraPosition.z = zoomLevel;

        glm::mat4 newView = glm::lookAt(
            cameraPosition,
            cameraTarget,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        renderer->setViewMatrix(newView);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (!renderer) return;

    // Get mouse position
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // Convert mouse position to normalized device coordinates (NDC)
    float ndcX = (2.0f * mouseX) / SCR_WIDTH - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / SCR_HEIGHT;

    // Convert NDC to world space coordinates
    float aspectRatio = static_cast<float>(SCR_WIDTH) / SCR_HEIGHT;
    float fov = glm::radians(60.0f);
    float worldScale = zoomLevel * tan(fov / 2.0f);

    float worldX = ndcX * worldScale * aspectRatio + cameraTarget.x;
    float worldY = ndcY * worldScale + cameraTarget.y;

    // Calculate zoom factor
    float zoomDelta = yoffset * 1.0f;
    float oldZoom = zoomLevel;
    zoomLevel = clamp(zoomLevel - zoomDelta, MIN_ZOOM, MAX_ZOOM);
    float zoomFactor = zoomLevel / oldZoom;

    // Update camera position and target to zoom toward mouse position
    glm::vec3 mouseWorld(worldX, worldY, 0.0f);
    glm::vec3 directionToMouse = mouseWorld - cameraTarget;

    // Move camera target toward mouse position during zoom
    float targetLerp = 1.0f - zoomFactor;
    cameraTarget += directionToMouse * targetLerp * 0.5f;

    // Update camera position while maintaining relative position to target
    cameraPosition = cameraTarget + glm::vec3(0.0f, 0.0f, zoomLevel);

    // Update view matrix
    glm::mat4 newView = glm::lookAt(
        cameraPosition,
        cameraTarget,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    renderer->setViewMatrix(newView);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_SPACE:
            simulationPaused = !simulationPaused;
            renderer->setSimulationPaused(simulationPaused);
            break;
        case GLFW_KEY_O:
            showOrbits = !showOrbits;
            break;
        case GLFW_KEY_1:
            timeScale = 0.5f;
            renderer->setTimeScale(timeScale);
            break;
        case GLFW_KEY_2:
            timeScale = 1.0f;
            renderer->setTimeScale(timeScale);
            break;
        case GLFW_KEY_3:
            timeScale = 2.0f;
            renderer->setTimeScale(timeScale);
            break;
        case GLFW_KEY_4:
            timeScale = 5.0f;
            renderer->setTimeScale(timeScale);
            break;
        case GLFW_KEY_5:
            timeScale = 10.0f;
            renderer->setTimeScale(timeScale);
            break;
        case GLFW_KEY_6:
            timeScale = 20.0f;
            renderer->setTimeScale(timeScale);
            break;
        case GLFW_KEY_F:
            cameraPosition = glm::vec3(0.0f, 0.0f, zoomLevel);
            cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
            if (renderer) {
                glm::mat4 newView = glm::lookAt(
                    cameraPosition,
                    cameraTarget,
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );
                renderer->setViewMatrix(newView);
            }
            break;

        case GLFW_KEY_R:
            GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

            if (!isFullscreen) {

                glfwSetWindowMonitor(window, primaryMonitor, 0, 0,
                    mode->width, mode->height, mode->refreshRate);
            }
            else {

                int windowPosX = (mode->width - SCR_WIDTH) / 2;
                int windowPosY = (mode->height - SCR_HEIGHT) / 2;
                glfwSetWindowMonitor(window, nullptr, windowPosX, windowPosY,
                    SCR_WIDTH, SCR_HEIGHT, mode->refreshRate);
            }
            isFullscreen = !isFullscreen;
            break;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    lastMouseX = xpos;
    lastMouseY = ypos;

    float x = (2.0f * xpos) / SCR_WIDTH - 1.0f;
    float y = 1.0f - (2.0f * ypos) / SCR_HEIGHT;

    float aspectRatio = static_cast<float>(SCR_WIDTH) / SCR_HEIGHT;
    float fov = glm::radians(60.0f);
    float worldScale = zoomLevel * tan(fov / 2.0f);

    float worldX = x * worldScale * aspectRatio + cameraTarget.x;
    float worldY = y * worldScale + cameraTarget.y;

    selectedObjectInfo = "";


    for (const auto& belt : renderer->getAsteroidBelts()) {
        float dist = sqrt(worldX * worldX + worldY * worldY);
        if (dist >= belt.minRadius && dist <= belt.maxRadius) {
            selectedObjectInfo = belt.name;
            return;
        }
    }

    for (const auto& obj : solarSystem) {
        float planetAngle = currentTime * obj.orbitSpeed;
        float planetX, planetY;

        if (obj.name == "Pluto") {
            planetX = 121.5f * cos(planetAngle) + 12.0f;
            planetY = 150.3f * 0.9f * sin(planetAngle) - 49.2f;
        }
        else if (obj.name == "Eris") {
            planetX = 255.6f * cos(planetAngle) - 78.0f;
            planetY = 140.4f * 0.85f * sin(planetAngle) + 21.0f;
        }
        else {
            planetX = obj.orbitRadius * cos(planetAngle);
            planetY = obj.orbitRadius * sin(planetAngle);
        }


        for (const auto& moon : obj.moons) {
            float baseAngle = currentTime * moon.orbitSpeed;
            float moonX = planetX + moon.orbitRadius * cos(baseAngle);
            float moonY = planetY + moon.orbitRadius * sin(baseAngle);

            float moonDistance = sqrt(pow(worldX - moonX, 2) + pow(worldY - moonY, 2));
            float moonSelectionRadius = moon.radius * 3.5f;

            if (moonDistance < moonSelectionRadius) {
                selectedObjectInfo = obj.name + " - " + moon.name;
                return;
            }
        }

        float distance = sqrt(pow(worldX - planetX, 2) + pow(worldY - planetY, 2));
        if (distance < obj.radius * 2.5f) {
            selectedObjectInfo = obj.name;
            return;
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (!selectedObjectInfo.empty()) {

            for (const auto& belt : renderer->getAsteroidBelts()) {
                if (selectedObjectInfo == belt.name) {
                    selectedObjectName = belt.name;
                    selectedObjectDescription = belt.info;
                    return;
                }
            }


            size_t hyphenPos = selectedObjectInfo.find(" - ");
            if (hyphenPos != std::string::npos) {

                std::string planetName = selectedObjectInfo.substr(0, hyphenPos);
                std::string moonName = selectedObjectInfo.substr(hyphenPos + 3);

                for (const auto& planet : solarSystem) {
                    if (planet.name == planetName) {
                        for (const auto& moon : planet.moons) {
                            if (moon.name == moonName) {
                                selectedObjectName = moonName;
                                selectedObjectDescription = moon.info;
                                return;
                            }
                        }
                    }
                }
            }
            else {

                for (const auto& obj : solarSystem) {
                    if (obj.name == selectedObjectInfo) {
                        selectedObjectName = obj.name;
                        selectedObjectDescription = obj.info;
                        return;
                    }
                }
            }
        }
    }
}

void limitFPS(double desiredFPS) {
    static double lastTime = glfwGetTime();
    const double frameTime = 1.0 / desiredFPS;


    while (true) {
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= frameTime) {
            lastTime = currentTime;
            break;
        }

        std::this_thread::yield();
    }
}

int main() {
    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);


    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Solar System", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }


    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    int windowPosX = (mode->width - SCR_WIDTH) / 2;
    int windowPosY = (mode->height - SCR_HEIGHT) / 2;

    glfwSetWindowPos(window, windowPosX, windowPosY);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);






    if (glewInit() != GLEW_OK) {
        return -1;
    }



    //"Sun" - object name
    //    0.03f - radius of rendered sphere
    //    0.0f - orbit radius(0 since Sun is center)
    //    0.0f - orbit speed(0 since Sun doesn't orbit)
    //        0.01f - self - rotation speed
    //        { 1.0f, 0.8f, 0.0f } - RGB color values(yellow)
    //        false - whether to draw orbit line
    //        "The Sun: Mass..." - information text shown when clicked

    textRenderer = std::make_unique<TextRenderer>("C:/Windows/Fonts/arial.ttf");
    glfwSetWindowPos(window, windowPosX, windowPosY);
    solarSystem = {
        // Sun
        {"Sun", 0.5f, 0.0f, 0.0f, 0.28f * (365.26 / 27), {1.0f, 0.8f, 0.0f}, false,
         "\nMass = 1.989 × 10^30 kg\nDiameter: 1.39 million km\nType: Yellow Dwarf Star\nSurface Temperature: 5,778 K\nContains 99.86% of solar system's mass"},

         // Mercury
         {"Mercury", 0.027f, 1.5461f, 0.0712f, 0.0294f, {0.7f, 0.7f, 0.7f}, true,
          "\nMass: 3.285 × 10^23 kg\nDiameter: 4,879 km\nType: Terrestrial Planet\nSmallest planet\nSurface Temperature: -180°C to 430°C\nNo moons"},

          // Venus
          {"Venus", 0.067f, 2.169f, 0.0279f, -0.0071f, {0.9f, 0.7f, 0.5f}, true,
           "\nMass: 4.867 × 10^24 kg\nDiameter: 12,104 km\nType: Terrestrial Planet\nHottest planet\nRotates backwards\nThick atmosphere of CO2"},

           // Earth
           {"Earth", 0.07f, 3.0f, 0.0172f, 6.28f, {0.2f, 0.5f, 1.0f}, true,
            "\nMass: 5.972 × 10^24 kg\nDiameter: 12,742 km\nType: Terrestrial Planet\nOnly known planet with life\nAge: 4.54 billion years",
            false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
            {{"Moon", 0.019f, 0.28f, 0.1f, {0.8f, 0.8f, 0.8f}, "moon",
              "\nMass: 7.34767 × 10^22 kg\nDiameter: 3,474 km\nType: Natural Satellite\nDistance: 384,400 km\nAge: 4.51 billion years\nOnly natural satellite of Earth"}}},

              // Mars
              {"Mars", 0.037f, 4.572f, 0.0091f, 6.10f, {1.0f, 0.4f, 0.0f}, true,
               "\nMass: 6.39 × 10^23 kg\nDiameter: 6,779 km\nType: Terrestrial Planet\nThe Red Planet\nHas the largest volcano\nTwo moons",
               false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
               {{"Phobos", 0.005f, 0.09f, 0.2f, {0.6f, 0.6f, 0.6f}, "phobos",
                 "\nMass: 1.06 × 10^16 kg\nDiameter: 22.2 km\nType: Natural Satellite\nLargest moon of Mars\nIrregular shape\nSpiraling closer to Mars"},
                {"Deimos", 0.003f, 0.12f, 0.15f, {0.5f, 0.5f, 0.5f}, "deimos",
                 "\nMass: 1.48 × 10^15 kg\nDiameter: 12.6 km\nType: Natural Satellite\nSmooth surface\nSlow orbit\nGradually moving away from Mars"}}},

                 // Jupiter
                 {"Jupiter", 0.284f, 15.609f, 0.00145f, 15.32f, {0.8f, 0.7f, 0.6f}, true,
                  "\nMass: 1.898 × 10^27 kg\nDiameter: 139,820 km\nType: Gas Giant\nLargest planet\nGreat Red Spot is a giant storm\n79 known moons",
                  false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
                  {{"Io", 0.02f, 0.42f, 0.15f, {1.0f, 1.0f, 0.6f}, "io",
                    "\nMass: 8.93 × 10^22 kg\nDiameter: 3,642 km\nType: Galilean Moon\nMost volcanic body\nSurface temperature: -130°C to -150°C\nOver 400 active volcanoes"},
                   {"Europa", 0.018f, 0.525f, 0.12f, {0.9f, 0.9f, 0.9f}, "europa",
                    "\nMass: 4.8 × 10^22 kg\nDiameter: 3,122 km\nType: Galilean Moon\nSmooth ice surface\nPossibly contains subsurface ocean\nThinnest atmosphere of Galilean moons"},
                   {"Ganymede", 0.029f, 0.63f, 0.10f, {0.8f, 0.8f, 0.7f}, "ganymede",
                    "\nMass: 1.48 × 10^23 kg\nDiameter: 5,268 km\nType: Galilean Moon\nLargest moon in solar system\nHas its own magnetic field\nLarger than Mercury"},
                   {"Callisto", 0.026f, 0.735f, 0.08f, {0.6f, 0.6f, 0.6f}, "callisto",
                    "\nMass: 1.08 × 10^23 kg\nDiameter: 4,821 km\nType: Galilean Moon\nMost heavily cratered object\nPossibly has subsurface ocean\nOldest Galilean moon"}}},

                    // Saturn
                    {"Saturn", 0.24f, 28.746f, 0.00058f, 14.11f, {0.9f, 0.8f, 0.5f}, true,
                     "\nMass: 5.683 × 10^26 kg\nDiameter: 116,460 km\nType: Gas Giant\nKnown for its rings\nLeast dense planet\n82 known moons",
                     true, 0.225f, 0.375f, {0.8f, 0.6f, 0.2f},
                     {{"Enceladus", 0.004f, 1.5f, 0.12f, {1.0f, 1.0f, 1.0f}, "enceladus",
                       "\nMass: 1.08 × 10^20 kg\nDiameter: 504 km\nType: Natural Satellite\nIce geysers\nSubsurface ocean\nReflects 99% of sunlight"},
                      {"Tethys", 0.006f, 1.8f, 0.11f, {0.9f, 0.9f, 0.9f}, "tethys",
                       "\nMass: 6.17 × 10^20 kg\nDiameter: 1,062 km\nType: Natural Satellite\nLarge impact crater\nIcy surface\nHeavily cratered"},
                      {"Rhea", 0.008f, 1.95f, 0.10f, {0.7f, 0.7f, 0.7f},"rhea",
                       "\nMass: 2.31 × 10^21 kg\nDiameter: 1,527 km\nType: Natural Satellite\nSaturn's 2nd largest\nWater ice surface\nThin atmosphere"},
                      {"Titan", 0.028f, 2.07f, 0.08f, {0.8f, 0.7f, 0.5f}, "titan",
                       "\nMass: 1.34 × 10^23 kg\nDiameter: 5,150 km\nType: Natural Satellite\nDense atmosphere\nLiquid methane lakes\nEarth-like features"},
                      {"Iapetus", 0.008f, 2.39f, 0.06f, {0.5f, 0.5f, 0.5f}, "iapetus",
                       "\nMass: 1.81 × 10^21 kg\nDiameter: 1,469 km\nType: Natural Satellite\nTwo-toned surface\nEquatorial ridge\nWalnut shape"}}},

                       // Uranus
                       {"Uranus", 0.15f, 57.603f, 0.00020f, -8.72f, {0.5f, 0.8f, 0.8f}, true,
                        "\nMass: 8.681 × 10^25 kg\nDiameter: 50,724 km\nType: Ice Giant\nRotates on its side\n27 known moons",
                        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
                        {{"Miranda", 0.004f, 0.18f, 0.13f, {0.8f, 0.8f, 0.8f}, "miranda",
                          "\nMass: 6.59 × 10^19 kg\nDiameter: 472 km\nType: Natural Satellite\nDramatic cliffs\nUnique surface features\nYoungest Uranian moon"},
                         {"Titania", 0.009f, 0.27f, 0.11f, {0.7f, 0.7f, 0.7f}, "titania",
                          "\nMass: 3.4 × 10^21 kg\nDiameter: 1,578 km\nType: Natural Satellite\nLargest Uranian moon\nScarped valleys\nIcy surface"},
                         {"Oberon", 0.008f, 0.33f, 0.09f, {0.6f, 0.6f, 0.6f}, "oberon",
                          "\nMass: 3.08 × 10^21 kg\nDiameter: 1,522 km\nType: Natural Satellite\nOutermost major moon\nCraters with dark floors\nOldest Uranian moon"}}},

                          // Neptune
                          {"Neptune", 0.14f, 90.141f, 0.00010f, 9.37f, {0.0f, 0.0f, 0.8f}, true,
                           "\nMass: 1.024 × 10^26 kg\nDiameter: 49,244 km\nType: Ice Giant\nWindiest planet\nDarkest ring system\n14 known moons",
                           false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
                           {{"Triton", 0.015f, 0.33f, -0.07f, {0.9f, 0.9f, 1.0f}, "triton",
                             "\nMass: 2.14 × 10^22 kg\nDiameter: 2,707 km\nType: Natural Satellite\nRetrograde orbit\nNitrogen geysers\nLikely captured Kuiper Belt object"}}},

                             // Pluto
                             {"Pluto", 0.013f, 118.446f, 0.000069f, 0.983f, {0.8f, 0.7f, 0.7f}, true,
                              "\nMass: 1.303 × 10^22 kg\nDiameter: 2,377 km\nType: Dwarf Planet\nDue to orbital resonance, cannot collide with Neptune or Eris\n5 known moons",
                              false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
                              {{"Charon", 0.006f, 0.075f, 0.08f, {0.7f, 0.7f, 0.7f}, "charon",
                                "\nMass: 1.586 × 10^21 kg\nDiameter: 1,212 km\nType: Natural Satellite\nTidally locked with Pluto\nLargest moon relative to parent body"},
                               {"Nix", 0.001f, 0.105f, 0.1f, {0.6f, 0.6f, 0.6f}, "nix",
                                "\nMass: ~5 × 10 ^ 16 kg\nDiameter : ~50 km\nType : Natural Satellite\nRapid rotation\nHighly reflective surface\nIrregular shape"}}},

                                // Eris
                                {"Eris", 0.012f, 203.343f, 0.000054f, 0.932f, {0.85f, 0.85f, 0.85f}, true,
                                 "\nMass: 1.67 × 10^22 kg\nDiameter: 2,326 km\nType: Dwarf Planet\nMore massive than Pluto\nOrbital mechanics prevent collision with Pluto\nHighly eccentric orbit",
                                 false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
                                 {{"Dysnomia", 0.002f, 0.06f, 0.09f, {0.6f, 0.6f, 0.6f}, "dysnomia",
                                   "\nMass: ~2 × 10^19 kg\nDiameter: ~700 km\nType: Natural Satellite\nNamed after daughter of Eris\nOnly known moon of Eris\nVery little known about its composition"}}}
    };
    renderer = std::make_unique<Renderer>(zoomLevel);
    renderer->initializeAsteroidBelts();
    starfield = std::make_unique<StarfieldBackground>(2000, zoomLevel * 10.0f);
    double lastFrame = glfwGetTime();
    renderer->loadTextures();
    lastTime = glfwGetTime();
    lastFPSUpdate = lastTime;

    while (!glfwWindowShouldClose(window)) {
        double currentFrame = glfwGetTime();
        float deltaTime = static_cast<float>(currentFrame - lastFrame);
        lastFrame = currentFrame;


        frameCount++;
        if (currentFrame - lastFPSUpdate >= 1.0) {
            currentFPS = frameCount;
            frameCount = 0;
            lastFPSUpdate = currentFrame;
        }

        if (!simulationPaused) {
            currentTime += deltaTime * timeScale;
        }
        renderer->setCurrentTime(currentTime);

        processInput(window);


        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderer->updateCamera();


        if (starfield) {
            starfield->render(renderer->getCurrentView(),
                glm::perspective(glm::radians(60.0f),
                    (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 200.0f),
                currentTime);
        }


        renderer->drawAsteroidBelts(currentTime);
        for (const auto& obj : solarSystem) {
            renderer->drawObject(obj, currentTime, showOrbits);
        }

        for (const auto& obj : solarSystem) {
            renderer->drawObject(obj, currentTime, showOrbits);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::string fpsText = "FPS: " + std::to_string(currentFPS);
        textRenderer->RenderText(
            "Dejan Jovanovic RA-212-2021",
            20.0f,
            SCR_HEIGHT - 40.0f,
            1.0f,
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        
        textRenderer->RenderText(
            "FPS: " + std::to_string(currentFPS),
            SCR_WIDTH - 150.0f,  
            SCR_HEIGHT - 40.0f,  
            1.0f,
            glm::vec3(1.0f, 1.0f, 1.0f)  
        );

        glDisable(GL_BLEND);


        if (!selectedObjectInfo.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            textRenderer->RenderText(selectedObjectInfo,
                lastMouseX + 15,
                SCR_HEIGHT - lastMouseY - 15,
                1.0f,
                glm::vec3(1.0f, 1.0f, 1.0f));

            glDisable(GL_BLEND);
        }

        // Render selected object description in bottom right
        if (!selectedObjectName.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float margin = 20.0f;
            float baseY = margin;
            float lineHeight = 30.0f;

            std::istringstream descStream(selectedObjectDescription);
            std::string line;
            std::vector<std::string> lines;


            lines.push_back(selectedObjectName);


            while (std::getline(descStream, line)) {
                lines.push_back(line);
            }


            for (int i = lines.size() - 1; i >= 0; i--) {
                float y = baseY + (lines.size() - 1 - i) * lineHeight;


                if (i == 0) {
                    textRenderer->RenderText(lines[i],
                        SCR_WIDTH - margin - textRenderer->GetTextWidth(lines[i], 1.2f),
                        y,
                        1.2f,
                        glm::vec3(1.0f, 0.8f, 0.0f));
                }

                else {
                    textRenderer->RenderText(lines[i],
                        SCR_WIDTH - margin - textRenderer->GetTextWidth(lines[i], 1.0f),
                        y,
                        1.0f,
                        glm::vec3(0.9f, 0.9f, 0.9f));
                }
            }

            glDisable(GL_BLEND);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
        limitFPS(60.0);
    }

    glfwTerminate();
    return 0;
}