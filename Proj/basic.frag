#version 330 core
out vec4 FragColor;
uniform vec3 uCol;

void main() {
    FragColor = vec4(uCol, 1.0);
}
