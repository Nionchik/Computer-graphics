// Buffer D - Логика игры (враги, пули, столкновения)
mat3 rotateX(float theta) {
    float c = cos(theta), s = sin(theta);
    return mat3(
        vec3(1,0,0),
        vec3(0,c,-s),
        vec3(0,s,c)
    );
}

mat3 rotateY(float theta) {
    float c = cos(theta), s = sin(theta);
    return mat3(
        vec3(c,0,s),
        vec3(0,1,0),
        vec3(-s,0,c)
    );
}

// Генератор псевдослучайных чисел
float random(float seed) {
    return fract(sin(seed * 127.1) * 43758.5453);
}

// Получение позиции врага относительно игрока
vec3 getEnemyPosition(int index, vec3 playerPos) {
    float baseZ = -40.0;
    float baseY = 0.0;
    
    if (index == 0) {
        return vec3(playerPos.x, baseY, baseZ);
    } else if (index == 1) {
        return vec3(playerPos.x - 15.0, baseY, baseZ);
    } else {
        return vec3(playerPos.x + 15.0, baseY, baseZ);
    }
}

// Выбор случайной позиции из трех
vec3 randomEnemyPosition(float seed, vec3 playerPos) {
    int index = int(random(seed) * 3.0);
    return getEnemyPosition(index, playerPos);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    ivec2 uv = ivec2(fragCoord);

    // Чтение предыдущего состояния
    vec4 prevBullet = texelFetch(iChannel3, ivec2(0,0), 0);
    vec4 prevBulletDir = texelFetch(iChannel3, ivec2(1,0), 0);
    vec4 prevExplosion = texelFetch(iChannel3, ivec2(3,0), 0);
    vec4 prevWaterSplash = texelFetch(iChannel3, ivec2(4,0), 0);
    vec4 prevEnemy = texelFetch(iChannel3, ivec2(2,0), 0);
    vec4 prevEnemy2 = texelFetch(iChannel3, ivec2(5,0), 0);
    vec4 prevEnemy3 = texelFetch(iChannel3, ivec2(6,0), 0);
    vec4 prevNewEnemyTimer = texelFetch(iChannel3, ivec2(7,0), 0);
    
    // Чтение состояния малых пуль
    vec4 prevSmallBullet1 = texelFetch(iChannel3, ivec2(8,0), 0);
    vec4 prevSmallBulletDir1 = texelFetch(iChannel3, ivec2(9,0), 0);
    vec4 prevSmallBullet2 = texelFetch(iChannel3, ivec2(10,0), 0);
    vec4 prevSmallBulletDir2 = texelFetch(iChannel3, ivec2(11,0), 0);
    vec4 prevSmallBullet3 = texelFetch(iChannel3, ivec2(12,0), 0);
    vec4 prevSmallBulletDir3 = texelFetch(iChannel3, ivec2(13,0), 0);
    vec4 prevSmallBullet4 = texelFetch(iChannel3, ivec2(14,0), 0);
    vec4 prevSmallBulletDir4 = texelFetch(iChannel3, ivec2(15,0), 0);
    
    // Таймеры для второй очереди малых пуль
    vec4 prevSmallCannonTimer = texelFetch(iChannel3, ivec2(16,0), 0);

    // Чтение состояния игрока и управления
    vec4 player = texelFetch(iChannel0, ivec2(0,0), 0); 
    vec4 ch1 = texelFetch(iChannel1, ivec2(0,0), 0);    
    vec4 ch2 = texelFetch(iChannel2, ivec2(0,0), 0);   

    // Определение прицела
    vec2 aim = vec2(0.0);
    if (length(ch1.xy) > 1e-5) {
        aim = ch1.xy;
    } else if (length(ch2.xy) > 1e-5) {
        aim = ch2.xy;
    }

    // Проверка нажатия пробела
    bool nowSpace = (ch1.z > 0.5) || (ch2.z > 0.5);

    // Текущее состояние
    vec4 bullet = prevBullet;
    vec4 bulletDir = prevBulletDir;
    vec4 enemy = prevEnemy;
    vec4 enemy2 = prevEnemy2;
    vec4 enemy3 = prevEnemy3;
    vec4 explosion = prevExplosion;
    vec4 waterSplash = prevWaterSplash;
    vec4 newEnemyTimer = prevNewEnemyTimer;
    
    // Текущее состояние малых пуль
    vec4 smallBullet1 = prevSmallBullet1;
    vec4 smallBulletDir1 = prevSmallBulletDir1;
    vec4 smallBullet2 = prevSmallBullet2;
    vec4 smallBulletDir2 = prevSmallBulletDir2;
    vec4 smallBullet3 = prevSmallBullet3;
    vec4 smallBulletDir3 = prevSmallBulletDir3;
    vec4 smallBullet4 = prevSmallBullet4;
    vec4 smallBulletDir4 = prevSmallBulletDir4;
    
    // Таймер для второй очереди малых пуль
    vec4 smallCannonTimer = prevSmallCannonTimer;

    // Позиция игрока в мировых координатах
    vec3 playerWorldPos = vec3(player.x, 0.0, player.y - 6.0);

    // Инициализация врагов при старте
    if (iFrame < 3) {
        enemy = vec4(getEnemyPosition(0, playerWorldPos), 1.0);
        enemy2 = vec4(getEnemyPosition(1, playerWorldPos), 0.0);
        enemy3 = vec4(getEnemyPosition(2, playerWorldPos), 0.0);
    }

    // Локальные позиции орудий (относительно корабля)
    vec3 cannonBasePos = vec3(0.4, 2.4, 0.8);
    vec3 leftCannonBasePos = vec3(-2.3, 1.6, 0.8);
    vec3 rightCannonBasePos = vec3(2.9, 1.6, 0.8);

    // Направление орудия с учетом прицеливания
    vec3 cannonDirection = vec3(0.0, 0.0, -1.0);
    cannonDirection = rotateX(-aim.y) * cannonDirection;
    cannonDirection = rotateY(-aim.x) * cannonDirection;

    // Мировые позиции орудий
    vec3 cannonStart = playerWorldPos + cannonBasePos;
    vec3 leftCannonStart = playerWorldPos + leftCannonBasePos;
    vec3 rightCannonStart = playerWorldPos + rightCannonBasePos;

    // Выстрел - основное орудие и первая очередь малых пуль
    if (bullet.w < 0.5 && nowSpace) {
        // Основное орудие
        bullet = vec4(cannonStart, 1.0);
        bulletDir = vec4(cannonDirection, 35.0);
        
        // ПЕРВАЯ ОЧЕРЕДЬ малых пуль
        smallBullet1 = vec4(leftCannonStart, 1.0);
        smallBulletDir1 = vec4(cannonDirection, 70.0);
        
        smallBullet2 = vec4(rightCannonStart, 1.0);
        smallBulletDir2 = vec4(cannonDirection, 70.0);
        
        // Запускаем таймер для второй очереди
        smallCannonTimer = vec4(0.15, 0.0, 0.0, 0.0);
    }

    // ВТОРАЯ ОЧЕРЕДЬ малых пуль (через 0.15 секунды)
    if (smallCannonTimer.x > 0.0) {
        smallCannonTimer.x -= iTimeDelta;
        if (smallCannonTimer.x <= 0.0) {

            smallBullet3 = vec4(leftCannonStart, 1.0);
            smallBulletDir3 = vec4(cannonDirection, 70.0);
            
            smallBullet4 = vec4(rightCannonStart, 1.0);
            smallBulletDir4 = vec4(cannonDirection, 70.0);
            
            smallCannonTimer.x = 0.0; 
        }
    }

    // Движение основной пули
    if (bullet.w > 0.5) {
        bullet.xyz += bulletDir.xyz * (bulletDir.w * iTimeDelta);
        if (length(bullet.xyz - playerWorldPos) > 100.0) bullet.w = 0.0;
    }
    
    // Движение малых пуль (первая очередь)
    if (smallBullet1.w > 0.5) {
        smallBullet1.xyz += smallBulletDir1.xyz * (smallBulletDir1.w * iTimeDelta);
        if (length(smallBullet1.xyz - playerWorldPos) > 100.0) smallBullet1.w = 0.0;
    }
    if (smallBullet2.w > 0.5) {
        smallBullet2.xyz += smallBulletDir2.xyz * (smallBulletDir2.w * iTimeDelta);
        if (length(smallBullet2.xyz - playerWorldPos) > 100.0) smallBullet2.w = 0.0;
    }
    
    // Движение малых пуль (вторая очередь)
    if (smallBullet3.w > 0.5) {
        smallBullet3.xyz += smallBulletDir3.xyz * (smallBulletDir3.w * iTimeDelta);
        if (length(smallBullet3.xyz - playerWorldPos) > 100.0) smallBullet3.w = 0.0;
    }
    if (smallBullet4.w > 0.5) {
        smallBullet4.xyz += smallBulletDir4.xyz * (smallBulletDir4.w * iTimeDelta);
        if (length(smallBullet4.xyz - playerWorldPos) > 100.0) smallBullet4.w = 0.0;
    }
    
    // Столкновение пуль с водой
    if (bullet.w > 0.5 && bullet.y <= 0.0){
        bullet.w = 0.0;
        waterSplash = vec4(bullet.xyz, 1.0);
    }
    if (smallBullet1.w > 0.5 && smallBullet1.y <= 0.0) smallBullet1.w = 0.0;
    if (smallBullet2.w > 0.5 && smallBullet2.y <= 0.0) smallBullet2.w = 0.0;
    if (smallBullet3.w > 0.5 && smallBullet3.y <= 0.0) smallBullet3.w = 0.0;
    if (smallBullet4.w > 0.5 && smallBullet4.y <= 0.0) smallBullet4.w = 0.0;

    // Попадание по врагам (все типы пуль)
    // Враг 1
    if (enemy.w > 0.5) {
        if (bullet.w > 0.5 && length(bullet.xyz - enemy.xyz) < 3.0) {
            bullet.w = 0.0;
            enemy.w = 0.0;
            explosion = vec4(enemy.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 0.0, 0.0, 0.0);
        }
        // Первая очередь малых пуль
        if (smallBullet1.w > 0.5 && length(smallBullet1.xyz - enemy.xyz) < 3.0) {
            smallBullet1.w = 0.0;
            enemy.w = 0.0;
            explosion = vec4(enemy.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 0.0, 0.0, 0.0);
        }
        if (smallBullet2.w > 0.5 && length(smallBullet2.xyz - enemy.xyz) < 3.0) {
            smallBullet2.w = 0.0;
            enemy.w = 0.0;
            explosion = vec4(enemy.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 0.0, 0.0, 0.0);
        }
        // Вторая очередь малых пуль
        if (smallBullet3.w > 0.5 && length(smallBullet3.xyz - enemy.xyz) < 3.0) {
            smallBullet3.w = 0.0;
            enemy.w = 0.0;
            explosion = vec4(enemy.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 0.0, 0.0, 0.0);
        }
        if (smallBullet4.w > 0.5 && length(smallBullet4.xyz - enemy.xyz) < 3.0) {
            smallBullet4.w = 0.0;
            enemy.w = 0.0;
            explosion = vec4(enemy.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 0.0, 0.0, 0.0);
        }
    }
    
    // Враг 2 
    if (enemy2.w > 0.5) {
        if (bullet.w > 0.5 && length(bullet.xyz - enemy2.xyz) < 3.0) {
            bullet.w = 0.0;
            enemy2.w = 0.0;
            explosion = vec4(enemy2.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 1.0, 0.0, 0.0);
        }
        if (smallBullet1.w > 0.5 && length(smallBullet1.xyz - enemy2.xyz) < 3.0) {
            smallBullet1.w = 0.0;
            enemy2.w = 0.0;
            explosion = vec4(enemy2.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 1.0, 0.0, 0.0);
        }
        if (smallBullet2.w > 0.5 && length(smallBullet2.xyz - enemy2.xyz) < 3.0) {
            smallBullet2.w = 0.0;
            enemy2.w = 0.0;
            explosion = vec4(enemy2.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 1.0, 0.0, 0.0);
        }
        if (smallBullet3.w > 0.5 && length(smallBullet3.xyz - enemy2.xyz) < 3.0) {
            smallBullet3.w = 0.0;
            enemy2.w = 0.0;
            explosion = vec4(enemy2.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 1.0, 0.0, 0.0);
        }
        if (smallBullet4.w > 0.5 && length(smallBullet4.xyz - enemy2.xyz) < 3.0) {
            smallBullet4.w = 0.0;
            enemy2.w = 0.0;
            explosion = vec4(enemy2.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 1.0, 0.0, 0.0);
        }
    }

    // Враг 3
    if (enemy3.w > 0.5) {
        if (bullet.w > 0.5 && length(bullet.xyz - enemy3.xyz) < 3.0) {
            bullet.w = 0.0;
            enemy3.w = 0.0;
            explosion = vec4(enemy3.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 2.0, 0.0, 0.0);
        }
        if (smallBullet1.w > 0.5 && length(smallBullet1.xyz - enemy3.xyz) < 3.0) {
            smallBullet1.w = 0.0;
            enemy3.w = 0.0;
            explosion = vec4(enemy3.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 2.0, 0.0, 0.0);
        }
        if (smallBullet2.w > 0.5 && length(smallBullet2.xyz - enemy3.xyz) < 3.0) {
            smallBullet2.w = 0.0;
            enemy3.w = 0.0;
            explosion = vec4(enemy3.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 2.0, 0.0, 0.0);
        }
        if (smallBullet3.w > 0.5 && length(smallBullet3.xyz - enemy3.xyz) < 3.0) {
            smallBullet3.w = 0.0;
            enemy3.w = 0.0;
            explosion = vec4(enemy3.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 2.0, 0.0, 0.0);
        }
        if (smallBullet4.w > 0.5 && length(smallBullet4.xyz - enemy3.xyz) < 3.0) {
            smallBullet4.w = 0.0;
            enemy3.w = 0.0;
            explosion = vec4(enemy3.xyz, 1.5);
            newEnemyTimer = vec4(1.0, 2.0, 0.0, 0.0);
        }
    }

    // Таймер появления нового врага
    if (newEnemyTimer.x > 0.0) {
        newEnemyTimer.x -= iTimeDelta * 0.9;
        if (newEnemyTimer.x <= 0.0 && explosion.w < 0.3) {
            float newColorSeed = iTime + random(iTime) * 1000.0;
            int enemyType = int(newEnemyTimer.y);
            
            if (enemyType == 0 && enemy.w < 0.5) {
                enemy.xyz = randomEnemyPosition(iTime + 4.0, playerWorldPos);
                enemy.w = newColorSeed;
            } else if (enemyType == 1 && enemy2.w < 0.5) {
                enemy2.xyz = randomEnemyPosition(iTime + 5.0, playerWorldPos);
                enemy2.w = newColorSeed;
            } else if (enemyType == 2 && enemy3.w < 0.5) {
                enemy3.xyz = randomEnemyPosition(iTime + 6.0, playerWorldPos);
                enemy3.w = newColorSeed;
            } else {
                if (enemy.w < 0.5) {
                    enemy.xyz = randomEnemyPosition(iTime + 7.0, playerWorldPos);
                    enemy.w = newColorSeed;
                } else if (enemy2.w < 0.5) {
                    enemy2.xyz = randomEnemyPosition(iTime + 8.0, playerWorldPos);
                    enemy2.w = newColorSeed;
                } else if (enemy3.w < 0.5) {
                    enemy3.xyz = randomEnemyPosition(iTime + 9.0, playerWorldPos);
                    enemy3.w = newColorSeed;
                }
            }
            newEnemyTimer.x = 0.0;
        }
    }

    // Анимация взрыва
    if (explosion.w > 0.0) {
        explosion.w *= pow(0.85, iTimeDelta * 60.0);
        if (explosion.w < 0.01) explosion.w = 0.0;
    }

    // Анимация водного всплеска
    if (waterSplash.w > 0.0) {
        waterSplash.w *= pow(0.85, iTimeDelta * 60.0);
        if (waterSplash.w < 0.0005) waterSplash.w = 0.0;
    }

    // Запись в буфер
    vec4 outc = texelFetch(iChannel3, uv, 0); 
    if (uv == ivec2(0,0)) outc = bullet;
    else if (uv == ivec2(1,0)) outc = bulletDir;
    else if (uv == ivec2(2,0)) outc = enemy;
    else if (uv == ivec2(3,0)) outc = explosion;
    else if (uv == ivec2(4,0)) outc = waterSplash;
    else if (uv == ivec2(5,0)) outc = enemy2;
    else if (uv == ivec2(6,0)) outc = enemy3;
    else if (uv == ivec2(7,0)) outc = newEnemyTimer;
    // Малые пули
    else if (uv == ivec2(8,0)) outc = smallBullet1;
    else if (uv == ivec2(9,0)) outc = smallBulletDir1;
    else if (uv == ivec2(10,0)) outc = smallBullet2;
    else if (uv == ivec2(11,0)) outc = smallBulletDir2;
    else if (uv == ivec2(12,0)) outc = smallBullet3;
    else if (uv == ivec2(13,0)) outc = smallBulletDir3;
    else if (uv == ivec2(14,0)) outc = smallBullet4;
    else if (uv == ivec2(15,0)) outc = smallBulletDir4;
    else if (uv == ivec2(16,0)) outc = smallCannonTimer;

    fragColor = outc;
}