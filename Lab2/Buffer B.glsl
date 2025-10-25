// Buffer B - Прицеливание орудия
const int KEY_LEFT  = 37;
const int KEY_UP    = 38;
const int KEY_RIGHT = 39;
const int KEY_DOWN  = 40;

vec2 handleKeyboard(vec2 aim) {
    float aimSpeed = 1.0; // Скорость прицеливания

    float left  = texelFetch(iChannel1, ivec2(KEY_LEFT, 0), 0).x;
    float right = texelFetch(iChannel1, ivec2(KEY_RIGHT, 0), 0).x;
    float up    = texelFetch(iChannel1, ivec2(KEY_UP, 0), 0).x;
    float down  = texelFetch(iChannel1, ivec2(KEY_DOWN, 0), 0).x;


    aim.x += (left - right) * aimSpeed * iTimeDelta;
    aim.y += (up - down) * aimSpeed * iTimeDelta;


    if (aim.y < -0.15){ aim.y = -0.15;}
    if (aim.y > 0.45){ aim.y = 0.45;}
    

    return aim;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 aim = texelFetch(iChannel0, ivec2(0, 0), 0).xy;
    aim = handleKeyboard(aim);
    
    fragColor = vec4(aim, 0, 0);
}