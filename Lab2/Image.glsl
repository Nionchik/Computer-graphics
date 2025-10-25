const int MAX_MARCHING_STEPS = 255;
const float MIN_DIST = 0.01;
const float MAX_DIST = 100.0;
const float PRECISION = 0.0001;

struct Surface {
  float dist;
  vec3 color;
};


mat3 rotateX(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(1, 0, 0),
        vec3(0, c, -s),
        vec3(0, s, c)
    );
}


mat3 rotateY(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, 0, s),
        vec3(0, 1, 0),
        vec3(-s, 0, c)
    );
}


mat3 rotateZ(float theta) {
    float c = cos(theta);
    float s = sin(theta);
    return mat3(
        vec3(c, -s, 0),
        vec3(s, c, 0),
        vec3(0, 0, 1)
    );
}


mat3 identity() {
    return mat3(
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(0, 0, 1)
    );
}

Surface opU( Surface d1, Surface d2 )
{
    if (d1.dist <= d2.dist){
        return d1;
    }
    return d2;
}

float sdLink( vec3 p, float le, float r1, float r2 )
{
  vec3 q = vec3( p.x, max(abs(p.y)-le,0.0), p.z );
  return length(vec2(length(q.xy)-r1,q.z)) - r2;
}

float sdRoundedCylinder( vec3 p, float ra, float rb, float h )
{
  vec2 d = vec2( length(p.xz)-2.0*ra+rb, abs(p.y) - h );
  return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rb;
}

Surface sdBox( vec3 p, vec3 b, vec3 col )
{
  vec3 q = abs(p) - b;
  float d = length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
  return Surface(d,col);
}

float sdRectangle( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  float d = length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
  return d;
}

float sdCylinder(vec3 p, vec3 a, vec3 b, float r)
{
    vec3  ba = b - a;
    vec3  pa = p - a;
    float baba = dot(ba,ba);
    float paba = dot(pa,ba);
    float x = length(pa*baba-ba*paba) - r*baba;
    float y = abs(paba-baba*0.5)-baba*0.5;
    float x2 = x*x;
    float y2 = y*y*baba;
    
    float d = (max(x,y)<0.0)?-min(x2,y2):(((x>0.0)?x2:0.0)+((y>0.0)?y2:0.0));
    
    return sign(d)*sqrt(abs(d))/baba;
}

// Закругленный куб
float sdRoundBox( vec3 p, vec3 b, float r )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r;
}

// Сфера для круглой башни
float sdSphere(vec3 p, float r)
{
    return length(p) - r;
}

// Трапециевидный корпус корабля с закругленными ЛЕВЫМИ углами
float sdShipHull(vec3 p, vec2 sizeTop, vec2 sizeBottom, float height)
{
   
    float t = (p.y + height * 0.5) / height;
    vec2 size = mix(sizeBottom, sizeTop, t);
    
    // Основная прямоугольная форма для обеих сторон
    vec2 rect = vec2(max(abs(p.x) - size.x, abs(p.z) - size.y), abs(p.y) - height * 0.5);
    float rectDist = min(max(rect.x, rect.y), 0.0) + length(max(rect, 0.0));
    
    // Для левой стороны добавляем закругление
    if (p.x < 0.0) {
        // Радиус закругления как доля от размера (например, 40% от меньшего размера)
        float radius = min(size.x, size.y) * 1.0;
        vec2 leftCorner = vec2(abs(p.x) - (size.x - radius), abs(p.z) - (size.y - radius));
        float roundedDist = length(max(leftCorner, 0.0)) + min(max(leftCorner.x, leftCorner.y), 0.0) - radius;
        return max(roundedDist, abs(p.y) - height * 0.5);
    }
    
    return rectDist;
}


float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// Шум для волн воды
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Генератор псевдослучайных чисел для цветов
float random(float seed) {
    return fract(sin(seed * 127.1) * 43758.5453);
}

//цвета для вражеских кораблей
vec3 randomColor(float seed) {
    float colorChoice = random(seed) * 8.0;
    
    if (colorChoice < 1.0) return vec3(1.0, 0.2, 0.2); 
    else if (colorChoice < 2.0) return vec3(0.2, 1.0, 0.2); 
    else if (colorChoice < 3.0) return vec3(0.2, 0.2, 1.0);
    else if (colorChoice < 4.0) return vec3(1.0, 1.0, 0.2); 
    else if (colorChoice < 5.0) return vec3(1.0, 0.5, 0.2); 
    else if (colorChoice < 6.0) return vec3(0.8, 0.2, 1.0); 
    else if (colorChoice < 7.0) return vec3(0.2, 1.0, 1.0); 
    else return vec3(1.0, 0.6, 0.8); 
}

Surface sdWater(vec3 p)
{
    //волнение воды
    float waveHeight = 0.3; // Было 0.15, стало 0.3 (в 2 раза больше)
    float waveFrequency = 0.3; // Было 0.4, стало 0.3 (более крупные волны)
    
    // Основные волны
    float waves = fbm(p.xz * waveFrequency + iTime * 0.3);
    
    // Детализированные волны для барашков
    float detailWaves = fbm(p.xz * waveFrequency * 3.0 - iTime * 0.5) * 0.5;
    
    // Общая высота волн
    float totalHeight = waves * waveHeight + detailWaves * waveHeight * 0.7;
    
    // Базовая плоскость воды с волны
    float waterSurface = p.y - totalHeight;
    
    // Цвет воды 
    vec3 waterColor = vec3(0.05, 0.2, 0.7);
    
    // Добавляем немного зеленого для реалистичности
    waterColor += vec3(0.0, 0.15, 0.0) * (1.0 - abs(totalHeight) * 1.5);
    
    // Белые барашки на гребнях волн
    float foamIntensity = detailWaves * 2.0;
    if (foamIntensity > 0.6 && totalHeight > waveHeight * 0.5) {
        float foam = smoothstep(0.6, 1.0, foamIntensity);
        waterColor = mix(waterColor, vec3(1.0, 1.0, 1.0), foam * 0.8);
    }
    
    // Дополнительные белые блики на высоких волнах
    if (totalHeight > waveHeight * 0.7) {
        waterColor += vec3(0.4, 0.4, 0.5) * (totalHeight - waveHeight * 0.7) * 8.0;
    }
    
    return Surface(waterSurface, waterColor);
}

Surface sdPlayer(vec3 p)
{
    Surface s;
  
    vec3 shipCenter = vec3(0.,0.,-6.0);
    vec3 state = texelFetch(iChannel0, ivec2(0,0),0).xyz;
  
    vec3 localPos = p - shipCenter;
    vec3 offset = localPos;
  
    // Цвета корабля
    vec3 hullColor = vec3(0.5, 0.5, 0.55);
    vec3 towerColor = vec3(0.35, 0.35, 0.4);
    vec3 cannonColor = vec3(0.2, 0.2, 0.2);
  
    // Корпус корабля
    Surface hull;
    hull.dist = sdShipHull(offset - vec3(0.,0.5,0.8), vec2(4.2, 2.5), vec2(3.5, 2.2), 1.2);
    hull.color = hullColor;
    s = hull;
  
    // Рубка корабля
    Surface superstructure;
    superstructure.dist = sdCylinder(offset-vec3(0.4,0.,-0.7), 
                                 vec3(0.,1.4,1.5),
                                 vec3(0.,2.0,1.5),
                                 1.4);
    superstructure.color = towerColor;
    s = opU(s, superstructure);
  
    // Основная круглая башня корабля (центральная)
    Surface mainTower;
    mainTower.dist = sdRoundedCylinder(offset-vec3(0.4,2.4,0.8), 0.55, 0.6, 0.25);
    mainTower.color = towerColor;
    s = opU(s, mainTower);

    // ЛЕВАЯ маленькая башня
    Surface leftTower;
    leftTower.dist = sdRoundedCylinder(offset-vec3(-2.3,1.4,0.8), 0.35, 0.5, 0.2);
    leftTower.color = towerColor;
    s = opU(s, leftTower);

    // ПРАВАЯ маленькая башня
    Surface rightTower;
    rightTower.dist = sdRoundedCylinder(offset-vec3(2.9,1.4,0.8), 0.35, 0.4, 0.2);
    rightTower.color = towerColor;
    s = opU(s, rightTower);

    // Получаем прицеливание для всех орудий
    vec2 aim = texelFetch(iChannel1, ivec2(0,0),0).xy;
    vec3 cannonDirection = vec3(0.0, 0.0, -1.0);
    cannonDirection = rotateX(-aim.y) * cannonDirection;
    cannonDirection = rotateY(-aim.x) * cannonDirection;

    // Основное орудие корабля (из центральной башни)
    Surface cannon;
    float cannonLength = 3.0;
    vec3 cannonBasePos = vec3(0.4, 2.4, 0.8);
    vec3 cannonStart = cannonBasePos;
    vec3 cannonEnd = cannonStart + cannonDirection * cannonLength;

    cannon.dist = sdCylinder(offset, cannonStart, cannonEnd, 0.16);
    cannon.color = cannonColor;
    s = opU(s, cannon);

    // ЛЕВОЕ малое орудие 
    Surface leftCannon;
    vec3 leftCannonBasePos = vec3(-2.3, 1.6, 0.8); // Высота 1.6 вместо 2.2
    float leftCannonLength = 1.8;
    vec3 leftCannonStart = leftCannonBasePos;
    vec3 leftCannonEnd = leftCannonStart + cannonDirection * leftCannonLength;
    
    leftCannon.dist = sdCylinder(offset, leftCannonStart, leftCannonEnd, 0.08);
    leftCannon.color = cannonColor;
    s = opU(s, leftCannon);

    // ПРАВОЕ малое орудие
    Surface rightCannon;
    vec3 rightCannonBasePos = vec3(2.9, 1.6, 0.8); // Высота 1.6 вместо 2.2
    float rightCannonLength = 1.8;
    vec3 rightCannonStart = rightCannonBasePos;
    vec3 rightCannonEnd = rightCannonStart + cannonDirection * rightCannonLength;
    
    rightCannon.dist = sdCylinder(offset, rightCannonStart, rightCannonEnd, 0.08);
    rightCannon.color = cannonColor;
    s = opU(s, rightCannon);

    return s;
}

//функция sdEnemy
Surface sdEnemy(vec3 p, vec3 shipCenter, float angle, float colorSeed, vec3 playerPos)
{
    Surface s;

    vec3 localPos = p - shipCenter;
    localPos = rotateY(angle) * localPos;
    vec3 offset = localPos;

    // Яркие цвета для вражеского корабля
    vec3 hullColor = randomColor(colorSeed + 10.0);
    vec3 towerColor = hullColor * 1.2;
    vec3 cannonColor = vec3(0.2, 0.2, 0.2);

    // Ограничиваем максимальную яркость
    hullColor = min(hullColor, vec3(1.0));
    towerColor = min(towerColor, vec3(1.0));

    // Корпус вражеского корабля
    Surface hull;
    hull.dist = sdShipHull(offset - vec3(0.,0.5,0.8), vec2(4.2, 2.5), vec2(3.5, 2.2), 1.2);
    hull.color = hullColor;
    s = hull;

    // Рубка вражеского корабля
    Surface superstructure;
    superstructure.dist = sdCylinder(offset-vec3(0.4,0.,0.), 
                                 vec3(0.,1.4,1.5),
                                 vec3(0.,2.0,1.5),
                                 1.4);
    superstructure.color = towerColor;
    s = opU(s, superstructure);

    // Основная круглая башня вражеского корабля (центральная)
    Surface mainTower;
    mainTower.dist = sdRoundedCylinder(offset-vec3(0.4,2.4,1.5), 0.55, 0.6, 0.25);
    mainTower.color = towerColor;
    s = opU(s, mainTower);

    // ЛЕВАЯ маленькая башня врага
    Surface leftTower;
    leftTower.dist = sdRoundedCylinder(offset-vec3(-2.3,1.4,1.5), 0.35, 0.4, 0.2);
    leftTower.color = towerColor;
    s = opU(s, leftTower);

    // ПРАВАЯ маленькая башня врага
    Surface rightTower;
    rightTower.dist = sdRoundedCylinder(offset-vec3(2.9,1.4,1.5), 0.35, 0.4, 0.2);
    rightTower.color = towerColor;
    s = opU(s, rightTower);

    // Основное орудие вражеского корабля - направлено на игрока
    Surface cannon;
    vec3 directionToPlayer = normalize(playerPos - shipCenter);
    
    // Поворачиваем направление в локальные координаты врага
    vec3 localDirection = rotateY(-angle) * directionToPlayer;
    vec3 directionCannon = localDirection * 2.5;
    
    cannon.dist = sdCylinder(offset-vec3(0.4,0.8,0.7), 
                         vec3(0.,1.5,0.6),
                         vec3(0.,2.2,1.5) + directionCannon,
                         0.16);
    cannon.color = cannonColor;
    s = opU(s, cannon);

    // ЛЕВОЕ малое орудие врага
    Surface leftCannon;
    vec3 leftCannonBasePos = vec3(-2.3, 1.6, 1.5);
    float leftCannonLength = 1.6;
    vec3 leftCannonStart = leftCannonBasePos;
    vec3 leftCannonEnd = leftCannonStart + localDirection * leftCannonLength;
    
    leftCannon.dist = sdCylinder(offset, leftCannonStart, leftCannonEnd, 0.08);
    leftCannon.color = cannonColor;
    s = opU(s, leftCannon);

    // ПРАВОЕ малое орудие врага
    Surface rightCannon;
    vec3 rightCannonBasePos = vec3(2.9, 1.6, 1.5);
    float rightCannonLength = 1.6;
    vec3 rightCannonStart = rightCannonBasePos;
    vec3 rightCannonEnd = rightCannonStart + localDirection * rightCannonLength;
    
    rightCannon.dist = sdCylinder(offset, rightCannonStart, rightCannonEnd, 0.08);
    rightCannon.color = cannonColor;
    s = opU(s, rightCannon);

    return s;
}

Surface sdScene(vec3 p) 
{
    Surface o;


    Surface objF = sdWater(p);
    o = objF;

    // Игрок
    Surface Player = sdPlayer(p);
    o = opU(o, Player);

    // Получаем данные о врагах и пулях из буферов
    vec4 bullet = texelFetch(iChannel3, ivec2(0,0), 0);
    vec4 enemy = texelFetch(iChannel3, ivec2(2,0), 0);
    vec4 enemy2 = texelFetch(iChannel3, ivec2(5,0), 0);
    vec4 enemy3 = texelFetch(iChannel3, ivec2(6,0), 0);
    vec4 explosion = texelFetch(iChannel3, ivec2(3,0), 0);
    vec4 waterSplash = texelFetch(iChannel3, ivec2(4,0), 0);
    
    // Малые пули (4 штуки - две очереди)
    vec4 smallBullet1 = texelFetch(iChannel3, ivec2(8,0), 0);
    vec4 smallBullet2 = texelFetch(iChannel3, ivec2(10,0), 0);
    vec4 smallBullet3 = texelFetch(iChannel3, ivec2(12,0), 0);
    vec4 smallBullet4 = texelFetch(iChannel3, ivec2(14,0), 0);

    // Получаем позицию игрока
    vec3 playerState = texelFetch(iChannel0, ivec2(0,0), 0).xyz;
    vec3 playerPos = vec3(playerState.x, 0.0, playerState.y - 6.0);

    // Враг 1 - используем w компонент как colorSeed
    if (enemy.w > 0.5) {
        Surface EnemyShip = sdEnemy(p, enemy.xyz, 0.0, enemy.w, playerPos);
        o = opU(o, EnemyShip);
    }
    
    // Враг 2 - используем w компонент как colorSeed
    if (enemy2.w > 0.5) {
        Surface EnemyShip = sdEnemy(p, enemy2.xyz, 0.0, enemy2.w, playerPos);
        o = opU(o, EnemyShip);
    }

    // Враг 3 - используем w компонент как colorSeed
    if (enemy3.w > 0.5) {
        Surface EnemyShip = sdEnemy(p, enemy3.xyz, 0.0, enemy3.w, playerPos);
        o = opU(o, EnemyShip);
    }

    // Основная пуля
    if (bullet.w > 0.5) {
        Surface b;
        b.dist = length(p - bullet.xyz) - 0.15;
        b.color = vec3(1.0, 0.2, 0.05); // Красный цвет
        o = opU(o, b);
    }

    // ПЕРВАЯ ОЧЕРЕДЬ малых пуль
    if (smallBullet1.w > 0.5) {
        Surface b;
        b.dist = length(p - smallBullet1.xyz) - 0.08; // Меньший размер
        b.color = vec3(1.0, 0.8, 0.1); // Желто-оранжевый цвет
        o = opU(o, b);
    }
    if (smallBullet2.w > 0.5) {
        Surface b;
        b.dist = length(p - smallBullet2.xyz) - 0.08;
        b.color = vec3(1.0, 0.8, 0.1);
        o = opU(o, b);
    }

    // ВТОРАЯ ОЧЕРЕДЬ малых пуль
    if (smallBullet3.w > 0.5) {
        Surface b;
        b.dist = length(p - smallBullet3.xyz) - 0.08;
        b.color = vec3(1.0, 0.9, 0.3); // Более светлый желтый для различия
        o = opU(o, b);
    }
    if (smallBullet4.w > 0.5) {
        Surface b;
        b.dist = length(p - smallBullet4.xyz) - 0.08;
        b.color = vec3(1.0, 0.9, 0.3);
        o = opU(o, b);
    }

    // Взрыв корабля
    if (explosion.w > 0.0) {
        float e = max(0.0, 1.0 - distance(p, explosion.xyz) * 0.08);
        if (e > 0.0) {
            Surface exp;
           
            float bubbleSize = mix(3.0, 0.5, smoothstep(0.3, 0.0, explosion.w));
            exp.dist = length(p - explosion.xyz) - (bubbleSize * explosion.w + 1.0);

            vec3 explosionColor = mix(vec3(1.0, 0.5, 0.1), vec3(0.3, 0.1, 0.0), smoothstep(0.5, 0.0, explosion.w));
            exp.color = explosionColor * e * explosion.w * 2.0;
            o = opU(o, exp);
        }
    }

    // Водный всплеск (синий/белый)
    if (waterSplash.w > 0.0) {
        float splash = max(0.0, 1.0 - distance(p, waterSplash.xyz) * 0.15);
        if (splash > 0.0) {
            Surface splashSurf;
            splashSurf.dist = length(p - waterSplash.xyz) - (1.5 * waterSplash.w);
            splashSurf.color = mix(vec3(0.2, 0.5, 1.0), vec3(1.0, 1.0, 1.0), waterSplash.w) * splash * waterSplash.w;
            o = opU(o, splashSurf);
        }
    }

    return o;
}

float rayMarch(vec3 ro, vec3 rd, float start, float end) {
  float depth = start;

  for (int i = 0; i < MAX_MARCHING_STEPS; i++) {
    vec3 p = ro + depth * rd;
    float d = sdScene(p).dist;
    depth += d;
    if (d < PRECISION || depth > end) { break; }
  }

  return depth;
}

vec3 calcNormal(vec3 p, float r) {
    vec2 e = vec2(1.0, -1.0) * 0.0005;
    return normalize( e.xyy * sdScene(p + e.xyy).dist + e.yyx * sdScene(p + e.yyx).dist +
      e.yxy * sdScene(p + e.yxy).dist + e.xxx * sdScene(p + e.xxx).dist);
}

vec3 DrawScene(vec2 uv) {
    vec3 backgroundColor = vec3(0.7, 0.9, 1.0);
    vec3 col = vec3(0);

    vec3 state = texelFetch(iChannel0, ivec2(0,0),0).xyz; 
    vec3 shipCenter = vec3(state.x, 0., state.y - 6.);

    float rotY = texelFetch(iChannel2, ivec2(0,0),0).x;
    mat3 rot = rotateY(rotY);

    vec3 offsetCam = vec3(0., 07., 12.);
    
    vec3 ro = shipCenter + rot * offsetCam;

    vec3 shipForward = rot * vec3(0.0, 0.0, -1.0);
    vec3 target = shipCenter + shipForward * 4.0; 

    vec3 forward = normalize(target - ro); 
    vec3 up = vec3(0.0, 1., 0.0);            
    vec3 right = normalize(cross(forward, up));  
    up = cross(right, forward);          

    float fov = 0.8; 
    vec3 rd = normalize(forward + uv.x * right * fov + uv.y * up * fov);

    float d = rayMarch(ro, rd, MIN_DIST, MAX_DIST);

    if (d > MAX_DIST) {
        col = backgroundColor; 
    } else {
        vec3 p = ro + rd * d;
        vec3 normal = calcNormal(p, 2.0);
        vec3 lightPosition = vec3(10, 50, 10);
        vec3 lightDirection = normalize(lightPosition - p);

        float dif = clamp(dot(normal, lightDirection), 0.3, 1.);
        col = dif * sdScene(p).color;
        
        // Усиливаем блики на воде
        if (p.y < 1.0) { // Если это вода
            float specular = pow(max(dot(reflect(-lightDirection, normal), -rd), 0.0), 64.0);
            col += vec3(0.9, 0.95, 1.0) * specular * 0.8;
        }
        
        vec4 explosion = texelFetch(iChannel3, ivec2(3,0), 0);
        if (explosion.w > 0.0) {
            float e = max(0.0, 1.0 - distance(p, explosion.xyz) * 0.1);
            col += vec3(1.0, 0.5, 0.2) * e * explosion.w;
        }
    }

    return col;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
  vec2 uv = (fragCoord-0.5*iResolution.xy)/iResolution.y;
  fragColor = vec4(DrawScene(uv), 1.0);
}