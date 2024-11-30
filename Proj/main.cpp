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

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1040;
const float PI = 3.14159265f;
const int ORBIT_RES = 100;

// Shader sources
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;
    
    out vec3 FragPos;
    out vec3 Normal;
    out vec2 TexCoords;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        FragPos = vec3(model * vec4(aPos.x, aPos.y, 0.0, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoords = aTexCoords;
        gl_Position = projection * view * model * vec4(aPos.x, aPos.y, 0.0, 1.0);
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
        vec3 result;
        if (isLightSource) {
            if(useTexture) {
                // For the Sun, blend the texture with the base color
                vec4 texColor = texture(texture1, TexCoords);
                result = uCol * texColor.rgb;
            } else {
                result = uCol;
            }
        } else {
            vec3 ambient = ambientStrength * uCol;
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(lightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = diff * uCol;
            
            if(useTexture) {
                vec4 texColor = texture(texture1, TexCoords);
                result = (ambient + diffuse) * texColor.rgb;
            } else {
                result = ambient + diffuse;
            }
        }
        
        FragColor = vec4(result, 1.0);
    }
)"; struct Character {
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

    void setupBuffers() {


        std::vector<float> circleVertices;
        for (int i = 0; i <= ORBIT_RES; i++) {  
            float angle = 2.0f * PI * i / ORBIT_RES;
            float x = cos(angle);
            float y = sin(angle);
            float u = angle / (2.0f * PI);  // Better texture coordinate calculation
            float v = 0.5f + y * 0.5f;      // Map y to [0,1]

            // Position
            circleVertices.push_back(x);
            circleVertices.push_back(y);
            // Normal
            circleVertices.push_back(x);
            circleVertices.push_back(y);
            circleVertices.push_back(0.0f);
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
            float x = 5.5f * cos(angle) - 1.0f;
            float y = 4.1f * 0.9f * sin(angle) + 0.4f;

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

            asteroidVertices.push_back(x);
            asteroidVertices.push_back(y);
            asteroidVertices.push_back(x);
            asteroidVertices.push_back(y);
            asteroidVertices.push_back(0.0f);
        }
        glGenVertexArrays(1, &asteroidVAO);
        glGenBuffers(1, &asteroidVBO);
        glBindVertexArray(asteroidVAO);
        glBindBuffer(GL_ARRAY_BUFFER, asteroidVBO);
        glBufferData(GL_ARRAY_BUFFER, asteroidVertices.size() * sizeof(float),
            asteroidVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);


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
        glm::mat4 ringModel = planetModel;
        ringModel = glm::rotate(ringModel, currentTime * 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));

        glBindVertexArray(ringVAO);
        glLineWidth(2.0f);

        
        struct RingSection {
            float startRadius;
            float endRadius;
            int numRings;
        };

        std::vector<RingSection> sections = {
            {0.7f, 0.8f, 15},  //  inner ring
            {0.8f, 1.0f, 8},   //  middle section
            {1.0f, 1.3f, 20},  //  outer ring
        };

        for (const auto& section : sections) {
            float ringStep = (section.endRadius - section.startRadius) / section.numRings;

            for (int i = 0; i <= section.numRings; i++) {
                float t = static_cast<float>(i) / section.numRings;
                glm::vec3 ringColor = glm::mix(
                    glm::vec3(0.9f, 0.8f, 0.6f),
                    glm::vec3(0.6f, 0.5f, 0.3f),
                    t
                );

                float radius = section.startRadius + i * ringStep;
                glm::mat4 currentRingModel = ringModel;
                currentRingModel = glm::scale(currentRingModel, glm::vec3(radius));

                shader->setMat4("model", currentRingModel);
                shader->setVec3("uCol", ringColor);
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
            }

            int blackRings = section.numRings / 4;
            float blackRingStep = (section.endRadius - section.startRadius) / blackRings;

            for (int i = 0; i < blackRings; i++) {
                float radius = section.startRadius + i * blackRingStep;
                glm::mat4 currentRingModel = ringModel;
                currentRingModel = glm::scale(currentRingModel, glm::vec3(radius));

                shader->setMat4("model", currentRingModel);
                shader->setVec3("uCol", glm::vec3(0.0f));
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
            }
        }

        glLineWidth(1.0f);
    }

public:
    void setCurrentTime(float time) { currentTime = time; }
    const glm::mat4& getCurrentView() const { return view; }

    Renderer(float& zoomRef) : zoomLevel(zoomRef) {
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
            "pluto", "eris", "moon", "nix", "dysnomia", "phobos", "deimos", "charon"
        };

        for (const auto& name : objectNames) {
            std::string path = "textures/" + name + ".jpg";
            unsigned int textureID = loadTexture(path.c_str());
            std::string objName = name;
            objName[0] = std::toupper(objName[0]);  // Capitalize first letter for SolarObject name match
            textures[objName] = textureID;
        }
    }

    void drawObject(const SolarObject& obj, float time, bool showOrbits) {
        shader->use();
        shader->setVec3("lightPos", glm::vec3(0.0f, 0.0f, 0.0f));
        shader->setBool("isLightSource", obj.name == "Sun");
        glBindVertexArray(circleVAO);

       
        if (obj.name == "Sun") {
            shader->setFloat("ambientStrength", 1.0f);  // Full brightness for the Sun

            // Apply Sun texture if available
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

        // Handle Pluto and Eris (elliptical orbits)
        if (obj.name == "Pluto" || obj.name == "Eris") {
            float angle = time * obj.orbitSpeed;
            float x, y;

            if (obj.name == "Pluto") {
                x = 40.5f * cos(angle) + 4.0f;
                y = 50.1f * 0.9f * sin(angle) - 16.4f;
            }
            else { // Eris
                x = 85.2f * cos(angle) - 26.0f;
                y = 46.8f * 0.85f * sin(angle) + 7.0f;
            }

            // Draw orbit path if enabled
            if (showOrbits) {
                shader->setMat4("model", glm::mat4(1.0f));
                shader->setVec3("uCol", glm::vec3(0.3f));

                std::vector<float> orbitVertices;
                for (int i = 0; i <= ORBIT_RES; i++) {
                    float a = 2.0f * PI * i / ORBIT_RES;
                    if (obj.name == "Pluto") {
                        orbitVertices.push_back(40.5f * cos(a) + 4.0f);
                        orbitVertices.push_back(50.1f * 0.9f * sin(a) - 16.4f);
                    }
                    else {
                        orbitVertices.push_back(85.2f * cos(a) - 26.0f);
                        orbitVertices.push_back(46.8f * 0.85f * sin(a) + 7.0f);
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

            // Draw the planet
            glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
            glm::mat4 model = glm::scale(
                glm::rotate(baseModel, time * obj.selfRotationSpeed, glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::vec3(obj.radius)
            );

            shader->setMat4("model", model);
            shader->setVec3("uCol", obj.color);
            glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);

            // Draw moons
            for (const auto& moon : obj.moons) {
                drawMoon(moon, baseModel, time, showOrbits);
            }
        }
        else {
            // Regular circular orbits for other planets
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

            // Draw moons
            for (const auto& moon : obj.moons) {
                drawMoon(moon, model, time, showOrbits);
            }

            // Draw rings if the planet has them
            if (obj.hasRings) {
                drawRings(obj, model);
            }
        }

        // Reset texture state
        glBindTexture(GL_TEXTURE_2D, 0);
        shader->setBool("useTexture", false);
    }

    void initializeAsteroidBelts() {
        // Main Asteroid Belt after Mars before Jupiter
        AsteroidBelt mainBelt{
            "Main Asteroid Belt",
            2.1f,  // minRadius 
            3.3f,  // maxRadius
            1000,   // numAsteroids
            {},    // asteroids vector
            glm::vec3(0.6f, 0.6f, 0.6f),  // color
            "Main Asteroid Belt\nLocated between Mars and Jupiter\nContains millions of asteroids"
        };

        // Kuiper Belt which is beyond Neptune
        AsteroidBelt kuiperBelt{
            "Kuiper Belt",
            46.0f,  // minRadius 
             66.0f,  // maxRadius
            15000,   // numAsteroids
            {},    // asteroids vector
            glm::vec3(0.4f, 0.4f, 0.4f),  // color
            "Kuiper Belt\nBeyond Neptune's orbit\nHome to many dwarf planets"
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
        shader->setFloat("ambientStrength", 0.5f);  // Increased ambient light for better visibility
        shader->setBool("isLightSource", false);
        shader->setBool("useTexture", false);  
        glBindVertexArray(asteroidVAO);

        for (const auto& belt : asteroidBelts) {
            shader->setVec3("uCol", belt.color);

            for (const auto& asteroid : belt.asteroids) {
                float angle = time * asteroid.orbitSpeed + asteroid.orbitOffset;

                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                    glm::vec3(asteroid.orbitRadius * cos(angle),
                        asteroid.orbitRadius * sin(angle), 0.0f));
                model = glm::scale(model, glm::vec3(asteroid.size));

                shader->setMat4("model", model);
                glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES / 2);
            }
        }
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
const float MIN_ZOOM = 0.1f;
const float MAX_ZOOM = 75.0f;
std::unique_ptr<Renderer> renderer = nullptr;
glm::mat4 view;
glm::vec3 cameraPosition(0.0f, 0.0f, zoomLevel);
glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
float cameraSpeed = 3.0f;

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
            zoomLevel = clamp(zoomLevel - 0.5f, MIN_ZOOM, MAX_ZOOM);
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
            zoomLevel = clamp(zoomLevel + 0.5f, MIN_ZOOM, MAX_ZOOM);
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
    float zoomDelta = yoffset * 1.5f;
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
        case GLFW_KEY_SPACE: simulationPaused = !simulationPaused; break;
        case GLFW_KEY_O: showOrbits = !showOrbits; break;
        case GLFW_KEY_1: timeScale = 0.5f; break;
        case GLFW_KEY_2: timeScale = 1.0f; break;
        case GLFW_KEY_3: timeScale = 2.0f; break;
        case GLFW_KEY_4: timeScale = 5.0f; break;
        case GLFW_KEY_5: timeScale = 10.0f; break;
        case GLFW_KEY_6: timeScale = 20.0f; break;
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

    // Check asteroid belts
    for (const auto& belt : renderer->getAsteroidBelts()) {
        float dist = sqrt(worldX * worldX + worldY * worldY);
        if (dist >= belt.minRadius && dist <= belt.maxRadius) {
            selectedObjectInfo = belt.name;
            return;
        }
    }

    // Check planets and moons
    for (const auto& obj : solarSystem) {
        float planetAngle = currentTime * obj.orbitSpeed;
        float planetX, planetY;

        if (obj.name == "Pluto") {
            planetX = 40.5f * cos(planetAngle) + 4.0f;
            planetY =50.1f * 0.9f * sin(planetAngle) - 16.4f;
        }
        else if (obj.name == "Eris") {
            planetX = 85.2f * cos(planetAngle) - 26.0f;
            planetY = 46.8f * 0.85f * sin(planetAngle) + 7.0f;
        }
        else {
            planetX = obj.orbitRadius * cos(planetAngle);
            planetY = obj.orbitRadius * sin(planetAngle);
        }

        // Check moons
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
            // Check asteroid belts
            for (const auto& belt : renderer->getAsteroidBelts()) {
                if (selectedObjectInfo == belt.name) {
                    selectedObjectName = belt.name;
                    selectedObjectDescription = belt.info;
                    return;
                }
            }

            // Check planets and moons
            size_t hyphenPos = selectedObjectInfo.find(" - ");
            if (hyphenPos != std::string::npos) {
                std::string planetName = selectedObjectInfo.substr(0, hyphenPos);
                std::string moonName = selectedObjectInfo.substr(hyphenPos + 3);

                for (const auto& obj : solarSystem) {
                    if (obj.name == planetName) {
                        for (const auto& moon : obj.moons) {
                            if (moon.name == moonName) {
                                selectedObjectName = moon.name;
                                selectedObjectDescription = moon.info;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            else {
                for (const auto& obj : solarSystem) {
                    if (obj.name == selectedObjectInfo) {
                        selectedObjectName = obj.name;
                        selectedObjectDescription = obj.info;
                        break;
                    }
                }
            }
        }
    }
}

void limitFPS(double desiredFPS) {
    static double lastTime = 0.0;
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastTime;

    if (deltaTime < 1.0 / desiredFPS) {
        glfwWaitEventsTimeout(1.0 / desiredFPS - deltaTime);
    }
    lastTime = currentTime;
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

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    // Calculate window position
    int windowPosX = (mode->width - SCR_WIDTH) / 2;
    int windowPosY = (mode->height - SCR_HEIGHT) / 2;

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
       {"Sun", 0.07f, 0.0f, 0.0f, 0.01f, {1.0f, 0.8f, 0.0f}, false,
        "The Sun: Mass = 1.989 � 10^30 kg\nSurface Temperature: 5,778 K"},
       {"Mercury", 0.0273f, 0.39f, 0.048f, 0.1f, {0.7f, 0.7f, 0.7f}, true,
        "Mercury: Smallest planet\nSurface Temperature: -180�C to 430�C\nNo moons"},
       {"Venus", 0.0665f, 0.72f, 0.035f, 0.01f, {0.9f, 0.7f, 0.5f}, true,
        "Venus: Hottest planet\nRotates backwards\nThick atmosphere of CO2"},
       {"Earth", 0.07f, 1.0f, 0.029f, 0.01f, {0.2f, 0.5f, 1.0f}, true,
        "Earth: Our home planet\nOnly known planet with life\nAge: 4.54 billion years",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Moon", 0.0189f, 0.12f, 0.1f, {0.8f, 0.8f, 0.8f},
        "Earth's Moon\nDistance: 384,400 km\nAge: 4.51 billion years"}}},
       {"Mars", 0.0371f, 1.52f, 0.024f, 1.01f, {1.0f, 0.4f, 0.0f}, true,
        "Mars: The Red Planet\nHas the largest volcano\nTwo moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Phobos", 0.0007f, 0.06f, 0.2f, {0.6f, 0.6f, 0.6f}, "Phobos: Largest moon of Mars\nIrregular shape\nOrbits close to surface"},
         {"Deimos", 0.000644f, 0.08f, 0.15f, {0.5f, 0.5f, 0.5f}, "Deimos: Smaller moon of Mars\nSmooth surface\nSlow orbit"}}},
       {"Jupiter", 0.784f, 5.2f, 0.013f, 0.01f, {0.8f, 0.7f, 0.6f}, true,
        "Jupiter: Largest planet\nGreat Red Spot is a giant storm\n79 known moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Io", 0.02002f, 1.0f, 0.15f, {1.0f, 1.0f, 0.6f},
        "Io: Most volcanic body in solar system\nSurface temperature: -130�C to -150�C"},
        {"Europa", 0.01715f, 1.2f, 0.12f, {0.9f, 0.9f, 0.9f},
        "Europa: Smooth ice surface\nPossibly contains subsurface ocean"},
        {"Ganymede", 0.02891f, 1.4f, 0.10f, {0.8f, 0.8f, 0.7f},
        "Ganymede: Largest moon in solar system\nHas its own magnetic field"},
        {"Callisto", 0.02646f, 1.6f, 0.08f, {0.6f, 0.6f, 0.6f},
        "Callisto: Most heavily cratered object\nPossibly has subsurface ocean"}}},
       {"Saturn", 0.6398f, 9.58f, 0.009f, 0.01f, {0.9f, 0.8f, 0.5f}, true,
        "Saturn: Known for its rings\nLeast dense planet\n82 known moons",
        true, 0.7f, 1.3f, {0.8f, 0.8f, 0.6f},
        {
        {"Enceladus", 0.0002772f, 1.4f, 0.12f, {1.0f, 1.0f, 1.0f},
        "Enceladus: Ice geysers\nSubsurface ocean\nActive geology"},
        {"Tethys", 0.00581f, 1.55f, 0.12f, {1.0f, 1.0f, 1.0f},
        "Tethys: Ice geysers\nSubsurface ocean\nActive geology"},
        {"Rhea", 0.0084f, 1.7f, 0.1f, {0.7f, 0.7f, 0.7f},
        "Rhea: Saturn's 2nd largest\nWater ice surface\nThin atmosphere"},
        {"Titan", 0.02828f, 1.85f, 0.08f, {0.8f, 0.7f, 0.5f},
        "Titan: Dense atmosphere\nLiquid methane lakes\nEarth-like features"},

        {"Iapetus", 0.00805f, 2.1f, 0.09f, {0.5f, 0.5f, 0.5f},
        "Iapetus: Two-toned surface\nEquatorial ridge\nWalnut shape"}}},
       {"Uranus", 0.28f, 19.22f, 0.006f, 0.01f, {0.5f, 0.8f, 0.8f}, true,
        "Uranus: Ice giant\nRotates on its side\n27 known moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Miranda", 0.00259f, 0.35f, 0.13f, {0.8f, 0.8f, 0.8f},
        "Miranda: Dramatic cliffs\nUnique surface features\nYoung terrain"},
            {"Titania", 0.00861f, 0.46f, 0.11f, {0.7f, 0.7f, 0.7f},
        "Titania: Largest Uranian moon\nScarped valleys\nIcy surface"},
        {"Oberon", 0.0084f, 0.52f, 0.09f, {0.6f, 0.6f, 0.6f},
        "Oberon: Outermost major moon\nCraters with dark floors\nOld surface"}
        }},
       {"Neptune", 0.2716f, 30.05f, 0.005f, 0.01f, {0.0f, 0.0f, 0.8f}, true,
        "Neptune: Windiest planet\nDarkest ring system\n14 known moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Triton", 0.01484f, 0.41f, -0.07f, {0.9f, 0.9f, 1.0f},
        "Triton: Retrograde orbit\nNitrogen geysers\nFrozen surface"},
       }},
{"Pluto", 0.0133f, 78.48f, 0.004f, 0.01f, {0.8f, 0.7f, 0.7f}, true,
 "Pluto: Dwarf planet\nCrosses Neptune's orbit\n5 known moons",
 false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
 {{"Charon", 0.01f, 0.05f, 0.08f, {0.7f, 0.7f, 0.7f},
 "Charon: Largest moon of Pluto\nTidally locked\nIcy surface"},
 {"Nix", 0.003f, 0.07f, 0.1f, {0.6f, 0.6f, 0.6f},
 "Nix: Small irregular moon\nRapid rotation\nHighly reflective"}}},
{"Eris", 0.01274f, 68.7f, 0.003f, 0.01f, {0.85f, 0.85f, 0.85f}, true,
 "Eris: Largest dwarf planet\nMore massive than Pluto\nHighly eccentric orbit",
 false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
 {{"Dysnomia", 0.005f, 0.04f, 0.09f, {0.6f, 0.6f, 0.6f},
 "Dysnomia: Only known moon of Eris\nNamed after daughter of Eris\nDiameter ~700km"}}}
    };

    renderer = std::make_unique<Renderer>(zoomLevel);
    renderer->initializeAsteroidBelts();
    double lastFrame = glfwGetTime();
    renderer->loadTextures();

    while (!glfwWindowShouldClose(window)) {
        double currentFrame = glfwGetTime();
        float deltaTime = static_cast<float>(currentFrame - lastFrame);
        lastFrame = currentFrame;

        if (!simulationPaused) {
            currentTime += deltaTime * timeScale;
        }
        renderer->setCurrentTime(currentTime);

        processInput(window);

        glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderer->updateCamera();


        renderer->drawAsteroidBelts(currentTime);  
        for (const auto& obj : solarSystem) {
            renderer->drawObject(obj, currentTime, showOrbits);
        }

        for (const auto& obj : solarSystem) {
            renderer->drawObject(obj, currentTime, showOrbits);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        textRenderer->RenderText(
            "Dejan Jovanovic RA-212-2021",
            20.0f,  
            SCR_HEIGHT - 40.0f, 
            1.0f,  // scale
            glm::vec3(1.0f, 1.0f, 1.0f)  
        );

        glDisable(GL_BLEND);

        // Render hover text
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