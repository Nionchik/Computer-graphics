// Buffer C - Выстрел
const int KEY_SPACE = 32;

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float space = texelFetch(iChannel1, ivec2(KEY_SPACE, 0), 0).x;
    
    fragColor = vec4(0.0, 0.0, space, 0);
}