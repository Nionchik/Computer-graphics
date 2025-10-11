const int KEY_LEFT  = 37;
const int KEY_RIGHT = 39;
const float velocity = 0.8; 
const float rotationSpeed = 4.0;
const float maxRotation = 1.5;

vec2 handleKeyboard(vec2 offset) {
    vec2 left = texelFetch(iChannel1, ivec2(KEY_LEFT, 0), 0).x * vec2(-1, -1);
    vec2 right = texelFetch(iChannel1, ivec2(KEY_RIGHT, 0), 0).x * vec2(1, 1);

    // Движение по X
    offset.x += (left.x + right.x) * velocity * iTimeDelta;
    
    // поворот
    if (left.x < 0.0) {
        offset.y -= rotationSpeed * iTimeDelta;
    } else if (right.x > 0.0) {
        offset.y += rotationSpeed * iTimeDelta;
    } else {

        if (offset.y > 0.0) {
            offset.y -= rotationSpeed * iTimeDelta;
            if (offset.y < 0.0) offset.y = 0.0;
        } else if (offset.y < 0.0) {
            offset.y += rotationSpeed * iTimeDelta;
            if (offset.y > 0.0) offset.y = 0.0;
        }
    }
    
    return offset;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 offset = texelFetch( iChannel0, ivec2(0, 0), 0).xy;
    
    offset = handleKeyboard(offset);
    
    if(offset.x < 0.) {offset.x = 0.;}
    if(offset.x > 1.) {offset.x = 1.;}
    
    if(offset.y > maxRotation) {offset.y = maxRotation;}
    if(offset.y < -maxRotation) {offset.y = -maxRotation;}

    fragColor = vec4(offset.xy, 0.0, 1.0);
}