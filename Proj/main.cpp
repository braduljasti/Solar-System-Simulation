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

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1040;
const float PI = 3.14159265f;
const int ORBIT_RES = 100;

// Shader sources
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    uniform vec3 uCol;
    out vec4 FragColor;
    
    void main() {
        FragColor = vec4(uCol, 1.0);
    }
)";

struct Character {
    unsigned int TextureID;
    glm::ivec2   Size;
    glm::ivec2   Bearing;
    unsigned int Advance;
};

struct Moon {
    std::string name;
    float radius;
    float orbitRadius;
    float orbitSpeed;
    glm::vec3 color;
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
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() { glUseProgram(ID); }

    void setMat4(const char* name, const glm::mat4& mat) {
        glUniformMatrix4fv(glGetUniformLocation(ID, name), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void setVec3(const char* name, const glm::vec3& value) {
        glUniform3fv(glGetUniformLocation(ID, name), 1, glm::value_ptr(value));
    }

    unsigned int getId() const { return ID; }
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

    void setupBuffers() {
        std::vector<float> circleVertices;
        for (int i = 0; i < ORBIT_RES; i++) {
            float angle = 2.0f * PI * i / ORBIT_RES;
            circleVertices.push_back(cos(angle));
            circleVertices.push_back(sin(angle));
        }




        glGenVertexArrays(1, &circleVAO);
        glGenBuffers(1, &circleVBO);
        glBindVertexArray(circleVAO);
        glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
        glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float),
            circleVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);


        std::vector<float> plutoOrbitVertices;
        for (int i = 0; i <= ORBIT_RES; i++) {
            float angle = 2.0f * PI * i / ORBIT_RES;
            plutoOrbitVertices.push_back(5.5f * cos(angle)-1.0f);
            plutoOrbitVertices.push_back(4.1f * 0.9f * sin(angle)+0.4f);
        }

        glGenVertexArrays(1, &plutoOrbitVAO);
        glGenBuffers(1, &plutoOrbitVBO);
        glBindVertexArray(plutoOrbitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, plutoOrbitVBO);
        glBufferData(GL_ARRAY_BUFFER, plutoOrbitVertices.size() * sizeof(float), plutoOrbitVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);



        std::vector<float> ringVertices;
        for (int i = 0; i <= ORBIT_RES; i++) {
            float angle = 2.0f * PI * i / ORBIT_RES;
            ringVertices.push_back(cos(angle));
            ringVertices.push_back(sin(angle));
        }

        glGenVertexArrays(1, &ringVAO);
        glGenBuffers(1, &ringVBO);
        glBindVertexArray(ringVAO);
        glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
        glBufferData(GL_ARRAY_BUFFER, ringVertices.size() * sizeof(float),
            ringVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }
    void drawMoon(const Moon& moon, const glm::mat4& planetModel, float time, bool showOrbits) {
        shader->use();

        // Use absolute angle calculation
        float baseAngle = time * moon.orbitSpeed;
        glm::vec3 planetPos = glm::vec3(planetModel[3]);

        glm::vec3 moonOffset(
            moon.orbitRadius * cos(baseAngle),
            moon.orbitRadius * sin(baseAngle),
            0.0f
        );

        glm::mat4 moonModel = glm::translate(glm::mat4(1.0f), planetPos + moonOffset);
        moonModel = glm::scale(moonModel, glm::vec3(moon.radius));

        shader->setMat4("model", moonModel);
        shader->setVec3("uCol", moon.color);
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


    void drawRings(const SolarObject& obj, const glm::mat4& planetModel) {
        shader->use();
        glm::mat4 ringModel = planetModel;
        ringModel = glm::rotate(ringModel, currentTime * 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));

        glBindVertexArray(circleVAO);
        glLineWidth(2.0f);

        const int numRings = 50;
        float ringStep = (obj.ringOuterRadius - obj.ringInnerRadius) / numRings;

        for (int i = 0; i <= numRings; i++) {
            float t = static_cast<float>(i) / numRings;
            glm::vec3 ringColor = glm::mix(
                glm::vec3(0.9f, 0.8f, 0.6f),
                glm::vec3(0.6f, 0.5f, 0.3f),
                t
            );

            float radius = obj.ringInnerRadius + i * ringStep;
            glm::mat4 currentRingModel = ringModel;
            currentRingModel = glm::scale(currentRingModel, glm::vec3(radius));

            shader->setMat4("model", currentRingModel);
            shader->setVec3("uCol", ringColor);
            glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES);
        }

        const int numDotRings = 4;
        const int dotsPerRing = 26;

        for (int ring = 0; ring < numDotRings; ring++) {
            float ringT = static_cast<float>(ring) / (numDotRings - 1);
            float dotRadius = glm::mix(obj.ringInnerRadius, obj.ringOuterRadius, ringT);
            float rotationSpeed = 0.1f * (1.0f - ringT);

            for (int dot = 0; dot < dotsPerRing; dot++) {
                float baseAngle = (2.0f * PI * dot) / dotsPerRing;
                float rotationAngle = currentTime * rotationSpeed;
                float finalAngle = baseAngle + rotationAngle;

                glm::mat4 dotModel = planetModel;
                dotModel = glm::translate(dotModel,
                    glm::vec3(dotRadius * cos(finalAngle), dotRadius * sin(finalAngle), 0.0f));
                dotModel = glm::scale(dotModel, glm::vec3(0.006f));

                shader->setMat4("model", dotModel);
                shader->setVec3("uCol", glm::vec3(0.0f, 0.0f, 0.0f));
                glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);
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

    void drawObject(const SolarObject& obj, float time, bool showOrbits) {
        shader->use();
        glBindVertexArray(circleVAO);  

        // Debug visibility print
        std::cout << "Drawing " << obj.name << std::endl;

        if (obj.name == "Pluto") {
            float angle = time * obj.orbitSpeed;
            float x = 5.5f * cos(angle) - 1.0f;
            float y = 4.1f * 0.9f * sin(angle) + 0.4f;

            if (showOrbits) {
                shader->setMat4("model", glm::mat4(1.0f));
                shader->setVec3("uCol", glm::vec3(0.3f));
                glBindVertexArray(plutoOrbitVAO);
                glDrawArrays(GL_LINE_LOOP, 0, ORBIT_RES + 1);
                glBindVertexArray(circleVAO);
            }

            glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
            glm::mat4 model = glm::scale(glm::rotate(baseModel,
                time * obj.selfRotationSpeed, glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::vec3(obj.radius));

            shader->setMat4("model", model);
            shader->setVec3("uCol", obj.color);
            glDrawArrays(GL_TRIANGLE_FAN, 0, ORBIT_RES);

            // Draw moons around Pluto's actual position
            for (const auto& moon : obj.moons) {
                drawMoon(moon, baseModel, time, showOrbits);
            }
        }
        else {
            // Original code for other objects
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
float zoomLevel = 8.0f;
const float MIN_ZOOM = 1.0f;
const float MAX_ZOOM = 20.0f;
std::unique_ptr<Renderer> renderer = nullptr;
glm::mat4 view;
glm::vec3 cameraPosition(0.0f, 0.0f, zoomLevel);
glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
float cameraSpeed = 2.0f;

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

    // Handle zoom with +/- keys
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    float ndcX = (2.0f * mouseX) / SCR_WIDTH - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / SCR_HEIGHT;

    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {

        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
            zoomLevel = clamp(zoomLevel - 0.1f, MIN_ZOOM, MAX_ZOOM);
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) {
            zoomLevel = clamp(zoomLevel + 0.1f, MIN_ZOOM, MAX_ZOOM);
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

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    float zoomDelta = yoffset * 0.5f;
    float oldZoom = zoomLevel;
    zoomLevel = clamp(zoomLevel - zoomDelta, MIN_ZOOM, MAX_ZOOM);

    float zoomFactor = zoomLevel / oldZoom;
    cameraPosition *= zoomFactor;
    cameraTarget *= zoomFactor;
    cameraPosition.z = zoomLevel;

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

    for (const auto& obj : solarSystem) {
        float planetAngle = currentTime * obj.orbitSpeed;
        float planetX, planetY;

        if (obj.name == "Pluto") {
            planetX = 5.5f * cos(planetAngle) - 1.0f;
            planetY = 4.1f * 0.9f * sin(planetAngle) + 0.4f;
        }
        else {
            planetX = obj.orbitRadius * cos(planetAngle);
            planetY = obj.orbitRadius * sin(planetAngle);
        }

        for (const auto& moon : obj.moons) {
            // Use same absolute angle calculation
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

  

    textRenderer = std::make_unique<TextRenderer>("C:/Windows/Fonts/arial.ttf");
    glfwSetWindowPos(window, windowPosX, windowPosY);

    solarSystem = {
       {"Sun", 0.03f, 0.0f, 0.0f, 0.01f, {1.0f, 0.8f, 0.0f}, false,
        "The Sun: Mass = 1.989 × 10^30 kg\nSurface Temperature: 5,778 K"},
       {"Mercury", 0.04f, 0.4f, 0.048f, 0.1f, {0.7f, 0.7f, 0.7f}, true,
        "Mercury: Smallest planet\nSurface Temperature: -180°C to 430°C\nNo moons"},
       {"Venus", 0.035f, 0.6f, 0.035f, 0.01f, {0.9f, 0.7f, 0.5f}, true,
        "Venus: Hottest planet\nRotates backwards\nThick atmosphere of CO2"},
       {"Earth", 0.04f, 0.9f, 0.029f, 0.01f, {0.2f, 0.5f, 1.0f}, true,
        "Earth: Our home planet\nOnly known planet with life\nAge: 4.54 billion years",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Moon", 0.01f, 0.08f, 0.1f, {0.8f, 0.8f, 0.8f},
        "Earth's Moon\nDistance: 384,400 km\nAge: 4.51 billion years"}}},
       {"Mars", 0.03f, 1.4f, 0.024f, 0.01f, {1.0f, 0.4f, 0.0f}, true,
        "Mars: The Red Planet\nHas the largest volcano\nTwo moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Phobos", 0.005f, 0.06f, 0.2f, {0.6f, 0.6f, 0.6f}, "Phobos: Largest moon of Mars\nIrregular shape\nOrbits close to surface"},
         {"Deimos", 0.003f, 0.08f, 0.15f, {0.5f, 0.5f, 0.5f}, "Deimos: Smaller moon of Mars\nSmooth surface\nSlow orbit"}}},
       {"Jupiter", 0.08f, 1.8f, 0.013f, 0.01f, {0.8f, 0.7f, 0.6f}, true,
        "Jupiter: Largest planet\nGreat Red Spot is a giant storm\n79 known moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Io", 0.015f, 0.12f, 0.15f, {1.0f, 1.0f, 0.6f},
        "Io: Most volcanic body in solar system\nSurface temperature: -130°C to -150°C"},
        {"Europa", 0.014f, 0.15f, 0.12f, {0.9f, 0.9f, 0.9f},
        "Europa: Smooth ice surface\nPossibly contains subsurface ocean"},
        {"Ganymede", 0.018f, 0.18f, 0.10f, {0.8f, 0.8f, 0.7f},
        "Ganymede: Largest moon in solar system\nHas its own magnetic field"},
        {"Callisto", 0.016f, 0.21f, 0.08f, {0.6f, 0.6f, 0.6f},
        "Callisto: Most heavily cratered object\nPossibly has subsurface ocean"}}},
       {"Saturn", 0.07f, 2.4f, 0.009f, 0.01f, {0.9f, 0.8f, 0.5f}, true,
        "Saturn: Known for its rings\nLeast dense planet\n82 known moons",
        true, 0.1f, 0.15f, {0.8f, 0.8f, 0.6f},
        {{"Titan", 0.016f, 0.25f, 0.08f, {0.8f, 0.7f, 0.5f},
        "Titan: Dense atmosphere\nLiquid methane lakes\nEarth-like features"},
        {"Rhea", 0.01f, 0.2f, 0.1f, {0.7f, 0.7f, 0.7f},
        "Rhea: Saturn's 2nd largest\nWater ice surface\nThin atmosphere"},
        {"Enceladus", 0.008f, 0.17f, 0.12f, {1.0f, 1.0f, 1.0f},
        "Enceladus: Ice geysers\nSubsurface ocean\nActive geology"},
        {"Iapetus", 0.012f, 0.23f, 0.09f, {0.5f, 0.5f, 0.5f},
        "Iapetus: Two-toned surface\nEquatorial ridge\nWalnut shape"}}},
       {"Uranus", 0.05f, 2.8f, 0.006f, 0.01f, {0.5f, 0.8f, 0.8f}, true,
        "Uranus: Ice giant\nRotates on its side\n27 known moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Titania", 0.012f, 0.16f, 0.11f, {0.7f, 0.7f, 0.7f},
        "Titania: Largest Uranian moon\nScarped valleys\nIcy surface"},
        {"Oberon", 0.011f, 0.19f, 0.09f, {0.6f, 0.6f, 0.6f},
        "Oberon: Outermost major moon\nCraters with dark floors\nOld surface"},
        {"Miranda", 0.008f, 0.13f, 0.13f, {0.8f, 0.8f, 0.8f},
        "Miranda: Dramatic cliffs\nUnique surface features\nYoung terrain"}}},
       {"Neptune", 0.05f, 3.7f, 0.005f, 0.01f, {0.0f, 0.0f, 0.8f}, true,
        "Neptune: Windiest planet\nDarkest ring system\n14 known moons",
        false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
        {{"Triton", 0.014f, 0.22f, -0.07f, {0.9f, 0.9f, 1.0f},
        "Triton: Retrograde orbit\nNitrogen geysers\nFrozen surface"},
        {"Nereid", 0.006f, 0.28f, 0.05f, {0.7f, 0.7f, 0.8f},
        "Nereid: Irregular orbit\nCapture theory\nDark surface"}}},
        // Add to solarSystem array before the last closing brace:
{"Pluto", 0.02f, 5.5f, 0.004f, 0.01f, {0.8f, 0.7f, 0.7f}, true,
 "Pluto: Dwarf planet\nCrosses Neptune's orbit\n5 known moons",
 false, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f},
 {{"Charon", 0.01f, 0.05f, 0.08f, {0.7f, 0.7f, 0.7f},
 "Charon: Largest moon of Pluto\nTidally locked\nIcy surface"},
 {"Nix", 0.003f, 0.07f, 0.1f, {0.6f, 0.6f, 0.6f},
 "Nix: Small irregular moon\nRapid rotation\nHighly reflective"}}}
    };

    renderer = std::make_unique<Renderer>(zoomLevel);
    double lastFrame = glfwGetTime();

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

        for (const auto& obj : solarSystem) {
            renderer->drawObject(obj, currentTime, showOrbits);
        }

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

            // Add header (name) first
            lines.push_back(selectedObjectName);

            // Add description lines
            while (std::getline(descStream, line)) {
                lines.push_back(line);
            }

            // Render lines from bottom to top
            for (int i = lines.size() - 1; i >= 0; i--) {
                float y = baseY + (lines.size() - 1 - i) * lineHeight;

                // Header (name)
                if (i == 0) {
                    textRenderer->RenderText(lines[i],
                        SCR_WIDTH - margin - textRenderer->GetTextWidth(lines[i], 1.2f),
                        y,
                        1.2f,
                        glm::vec3(1.0f, 0.8f, 0.0f));  // Gold color for header
                }
                // Description lines
                else {
                    textRenderer->RenderText(lines[i],
                        SCR_WIDTH - margin - textRenderer->GetTextWidth(lines[i], 1.0f),
                        y,
                        1.0f,
                        glm::vec3(0.9f, 0.9f, 0.9f));  // White color for description
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