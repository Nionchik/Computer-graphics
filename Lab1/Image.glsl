const float accuracy = 0.025;
const float limit = 0.18;
const float speed = 0.7;

struct Surface {
  float dist;
  vec3 color;
};

float sdLineX(vec2 uv, float n){
    float d = n * iResolution.x/iResolution.y;
    d = uv.x - d;
    return d;
}

//пунктирная линия
Surface sdLineRoad(vec2 uv, float n, vec3 colorRoad){
    Surface s;
    s.dist = n * iResolution.x/iResolution.y;
    s.dist = abs(uv.x - s.dist);
    
    s.color = vec3(0.75, 0.75, 0.75);
    float phase = mod(uv.y + iTime * speed, 0.4);
    if (phase < 0.2) {
        s.color = vec3(1., 1., 1.);
    }
    
    return s;
}

Surface sdRectangle(vec2 uv, vec2 r, vec2 offset, vec3 color){
    Surface s;
    vec2 d = abs(uv-offset) - r;
    s.dist = length(max(d,0.)) + min(max(d.x,d.y),0.); 
    s.color = color;
    return s;
}

float dot2(in vec2 v ) { return dot(v,v); }

Surface sdTrapezoid( in vec2 p, in float r1, float r2, float he, vec2 offset ,vec3 color)
{
    Surface s;
    vec2 k1 = vec2(r2,he);
    vec2 k2 = vec2(r2-r1,2.0*he);
    p -= offset;
    p.x = abs(p.x);
    vec2 ca = vec2(p.x-min(p.x,(p.y<0.0)?r1:r2), abs(p.y)-he);
    vec2 cb = p - k1 + k2*clamp( dot(k1-p,k2)/dot2(k2), 0.0, 1.0 );
    float i = (cb.x<0.0 && ca.y<0.0) ? -1.0 : 1.0;
    s.dist = i*sqrt( min(dot2(ca),dot2(cb)) );
    s.color = color;
    
    return s;
}

vec2 rotate(vec2 uv, float th) {
  return mat2(cos(th), sin(th), -sin(th), cos(th)) * uv;
}

Surface sdTriangle(vec2 uv, float size, vec2 offset, vec3 color){
    Surface s;
    vec2 p = uv - offset;
    p.x = abs(p.x);
    s.dist = max(p.x*0.866025 + p.y*0.5, -p.y) - size*0.5;
    s.color = color;
    return s;
}

//машина
Surface sdCar(vec2 uv){
    Surface s;
    Surface dec;
    
    float Xuv = iResolution.x/iResolution.y;
    vec2 offset = vec2(Xuv*(limit+0.07),0.2);
    float keyboardX = texelFetch( iChannel0, ivec2(0,0),0).x;
    
    float roadWidth = Xuv * (1.0 - 2.0 * limit);
    float carHalfWidth = 0.063;
    float maxOffset = roadWidth * 0.5 - carHalfWidth - 0.02;
    
    offset.x += keyboardX * maxOffset * 2.0;
    
    float rotateN = texelFetch( iChannel0, ivec2(0,0),0).y * 15.0;
    vec2 rotateUv = rotate(uv - offset,radians(rotateN)) + offset;
    
    float LocalAccuracy = accuracy * 0.1;
    
    vec3 carColor = vec3(0.0, 0.2, 0.8);
    vec3 lightUp = vec3(0.8,0.8,0.);
    vec3 lightDown = vec3(0.8,0.1,0.);
    
    s = sdRectangle(rotateUv,vec2(0.04,0.10),offset,carColor); //корпус
    
    //колеса
    dec = sdRectangle(rotateUv,vec2(0.063,0.013),vec2(offset.x,offset.y+0.067),vec3(0.,0.,0.));
    if(s.dist>accuracy && dec.dist <= accuracy) { s = dec;}
    dec = sdRectangle(rotateUv,vec2(0.063,0.013),vec2(offset.x,offset.y-0.067),vec3(0.,0.,0.));
    if(s.dist>accuracy && dec.dist <= accuracy) { s = dec;}
    
    //крыша
    dec = sdRectangle(rotateUv,vec2(0.033,0.051),vec2(offset.x,offset.y-0.02),carColor-0.08);
    if (dec.dist <= LocalAccuracy) {s.color = dec.color;}
    
    //фары
    dec = sdRectangle(rotateUv,vec2(0.009,0.005),vec2(offset.x-0.053,offset.y+0.103),lightUp);
    if (dec.dist <= LocalAccuracy) {s.color = dec.color;}
    dec = sdRectangle(rotateUv,vec2(0.009,0.005),vec2(offset.x+0.053,offset.y+0.103),lightUp);
    if (dec.dist <= LocalAccuracy) {s.color = dec.color;}
    
    dec = sdRectangle(rotateUv,vec2(0.009,0.005),vec2(offset.x-0.053,offset.y-0.103),lightDown);
    if (dec.dist <= LocalAccuracy) {s.color = dec.color;}
    dec = sdRectangle(rotateUv,vec2(0.009,0.005),vec2(offset.x+0.053,offset.y-0.103),lightDown);
    if (dec.dist <= LocalAccuracy) {s.color = dec.color;}

    return s;
}

float rand(vec2 co)
{
   return fract(sin(dot(co.xy,vec2(12.9898,78.233))) * 43758.5453);
}

//движение по оси У
float yPosCycle(float seed, float minDelay, float maxDelay){
    float r = rand(vec2(seed*210.)) * maxDelay;
    float cycleTime = mod(iTime + r, (minDelay/speed) + maxDelay);
    return 3.0 - cycleTime * speed; // Начинаем с y=3.0 (выше экрана)
}

float yPosCycle(float seed){
    return yPosCycle(seed, 0.8, 3.5);
}

Surface sdCircle( vec2 uv, float r, vec2 offset, vec3 color )
{
    Surface s;
    s.dist = length(uv - offset) - r;
    s.color = color;
    return s;
}

//деревья слева
Surface sdTreeLeft(vec2 uv, float seed){
    Surface s;
    Surface dec;
    
    float yPos = yPosCycle(seed);
    if(yPos<-1.0){ s.dist=1000.0; return s; }
    
    float xPos = mix(0.08, 0.18, rand(vec2(seed, 123.45)));

    s = sdTriangle(uv, 0.09, vec2(xPos, yPos+0.01), vec3(0.1,0.4,0.1));
    dec = sdTriangle(uv, 0.085, vec2(xPos, yPos+0.06), vec3(0.2,0.5,0.2));
    if(dec.dist<s.dist) s=dec;
    dec = sdTriangle(uv, 0.07, vec2(xPos, yPos+0.11), vec3(0.3,0.6,0.3));
    if(dec.dist<s.dist) s=dec;

    dec = sdRectangle(uv, vec2(0.015,0.01), vec2(xPos, yPos-0.05), vec3(0.4,0.25,0.1));
    if(dec.dist<s.dist) s=dec;

    return s;
}

//деревья справа
Surface sdTreeRight(vec2 uv, float seed, float Xuv) {
    Surface s;
    Surface dec;
    
    float yPos = yPosCycle(seed);
    if (yPos < -1.0) {
        s.dist = 1000.0;
        return s;
    }
    
    float xPos = mix(Xuv - 0.18, Xuv - 0.08, rand(vec2(seed, 678.9)));
    
    s = sdCircle(uv, 0.08, vec2(xPos, yPos), vec3(0., 0.75, 0.)); 
    
    dec = sdCircle(uv, 0.052, vec2(xPos, yPos), vec3(0., 0.6, 0.)); 
    if (dec.dist <= accuracy) {s.color = dec.color;}
    dec = sdCircle(uv, 0.03, vec2(xPos, yPos), vec3(0., 0.45, 0.)); 
    if (dec.dist <= accuracy) {s.color = dec.color;}
    
    dec = sdRectangle(uv, vec2(0.001, 0.1), vec2(xPos, yPos - 0.09), vec3(0.4, 0.25, 0.1)); 
    if (dec.dist < s.dist) { s = dec; }
    
    return s;
}

float spawnIndex(float seed, float minDelay, float maxDelay) {
    float cycleTime = (minDelay / speed) + maxDelay;
    return floor((iTime + seed) / cycleTime);
}

//движение по оси Х
float xPosCycle(float seed, float minDelay, float maxDelay) {
    float index = spawnIndex(seed, minDelay, maxDelay);
    float r = rand(vec2(seed, index));
    return ceil(rand(vec2(seed, r))*4.) * (iResolution.x/iResolution.y*(1.-2.0*limit))/4.+limit;
}

//препятствия на дороге
Surface sdObstacle(vec2 uv, float seed, float shapeOffset) {
    Surface s;
    
    float uniqueSeed = seed + shapeOffset * 10.0;

    float spawnIdx = spawnIndex(uniqueSeed, 0.8, 3.5);
    float r = rand(vec2(uniqueSeed, spawnIdx));

    float xPos = ceil(r * 4.) * (iResolution.x/iResolution.y*(1.-2.0*limit))/4. + limit;

    float shapeType = mod(shapeOffset, 4.0) / 4.0;

    vec3 color = vec3(
        rand(vec2(uniqueSeed * 3.0 + spawnIdx * 7.0, 111.0)) * 0.7 + 0.05,
        rand(vec2(uniqueSeed * 7.0 + spawnIdx * 13.0, 222.0)) * 0.7 + 0.05,
        rand(vec2(uniqueSeed * 11.0 + spawnIdx * 19.0, 333.0)) * 0.7 + 0.05
    );
    
    // Движение по Y
    float baseY = yPosCycle(seed, 0.8, 3.5);
    float yOffset = shapeOffset * 0.5;
    float yPos = baseY - yOffset;
    
    // Если фигура за экраном - не отображаем
    if (yPos < -2.5 || yPos > 3.0) {
        s.dist = 1000.0;
        return s;
    }

    if (shapeType < 0.25) {
        s = sdTriangle(uv, 0.08, vec2(xPos, yPos), color);
    } else if (shapeType < 0.5) {
        s = sdTrapezoid(uv, 0.08, 0.04, 0.05, vec2(xPos, yPos), color);
    } else if (shapeType < 0.75) {
        s = sdCircle(uv, 0.06, vec2(xPos, yPos), color);
    } else {
        s = sdRectangle(uv, vec2(0.05, 0.05), vec2(xPos, yPos), color);
    }
    
    return s;
}

vec4 DrawScene(vec2 uv){
    float Xuv = iResolution.x/iResolution.y;
    vec3 OutColor = vec3(0.75, 0.75, 0.75);
    
    if (sdLineX(uv, limit) < accuracy) { OutColor = vec3(0.3, 0.3, 0.3); }
    if (sdLineX(uv, 1. - limit) > accuracy) { OutColor = vec3(0.3, 0.3, 0.3); }
    if (sdLineX(uv, limit - accuracy) < accuracy) { OutColor = vec3(0.7, 0.9, 0.7); }
    if (sdLineX(uv, 1. - limit + accuracy) > accuracy) { OutColor = vec3(0.7, 0.9, 0.7); }
    
    Surface LineRoad1 = sdLineRoad(uv, limit + (1.0 - 2.0*limit) * 0.25, vec3(0.75,0.75,0.75));
    if (LineRoad1.dist < accuracy * 0.3) { OutColor = LineRoad1.color; }
    
    Surface LineRoad2 = sdLineRoad(uv, limit + (1.0 - 2.0*limit) * 0.5, vec3(0.75,0.75,0.75));
    if (LineRoad2.dist < accuracy * 0.3) { OutColor = LineRoad2.color; }
    
    Surface LineRoad3 = sdLineRoad(uv, limit + (1.0 - 2.0*limit) * 0.75, vec3(0.75,0.75,0.75));
    if (LineRoad3.dist < accuracy * 0.3) { OutColor = LineRoad3.color; }
    
    Surface treeLeft = sdTreeLeft(uv, 1.0);
    if (treeLeft.dist < accuracy) {OutColor = treeLeft.color;}
    treeLeft = sdTreeLeft(uv, 3.5);
    if (treeLeft.dist < accuracy) {OutColor = treeLeft.color;}
    treeLeft = sdTreeLeft(uv, 6.2);
    if (treeLeft.dist < accuracy) {OutColor = treeLeft.color;}
    treeLeft = sdTreeLeft(uv, 8.7);
    if (treeLeft.dist < accuracy) {OutColor = treeLeft.color;}
    treeLeft = sdTreeLeft(uv, 11.3);
    if (treeLeft.dist < accuracy) {OutColor = treeLeft.color;}
    
    Surface treeRight = sdTreeRight(uv, 2.1, Xuv);
    if (treeRight.dist < accuracy) {OutColor = treeRight.color;}
    treeRight = sdTreeRight(uv, 4.8, Xuv);
    if (treeRight.dist < accuracy) {OutColor = treeRight.color;}
    treeRight = sdTreeRight(uv, 7.4, Xuv);
    if (treeRight.dist < accuracy) {OutColor = treeRight.color;}
    treeRight = sdTreeRight(uv, 10.0, Xuv);
    if (treeRight.dist < accuracy) {OutColor = treeRight.color;}
    treeRight = sdTreeRight(uv, 12.6, Xuv);
    if (treeRight.dist < accuracy) {OutColor = treeRight.color;}
    
    Surface obstacle1 = sdObstacle(uv, 1.0, 0.0);
    if (obstacle1.dist < accuracy) {OutColor = obstacle1.color;}
    Surface obstacle2 = sdObstacle(uv, 1.0, 1.0);
    if (obstacle2.dist < accuracy) {OutColor = obstacle2.color;}
    Surface obstacle3 = sdObstacle(uv, 1.0, 2.0);
    if (obstacle3.dist < accuracy) {OutColor = obstacle3.color;}
    Surface obstacle4 = sdObstacle(uv, 1.0, 3.0);
    if (obstacle4.dist < accuracy) {OutColor = obstacle4.color;}
    
    Surface car = sdCar(uv);
    if (car.dist < accuracy) {OutColor = car.color;}
    
    return vec4(OutColor,1.0);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.y; 
    fragColor = DrawScene(uv);
}