#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include <SDL3/SDL.h>
#include "circle.h"
#include "rectangle.h"

constexpr int WINDOW_WIDTH = 700, WINDOW_HEIGHT = 800, SLICES = 3;
constexpr float SHIP_SIZE = 50.0f, RADIUS = 10.0f, GRID_SIZE = 20.0f;
constexpr float SHOT_SPEED = 500.0f, ENEMY_SHOT_SPEED = 100.0f;
constexpr float SHOT_PERIOD = 0.3f, ENEMY_SHOT_PERIOD = 3.0f;
constexpr float ACCELERATION = 400.0f, BRAKING = 800.0f, MAX_SPEED = 500.0f;
constexpr SDL_Color GREEN{0, 180, 0, 255}, ORANGE{255, 145, 0, 255};
constexpr SDL_Color RED{230, 30, 30, 255}, MAGENTA{220, 0, 190, 255};
constexpr SDL_Color BLUE{30, 90, 240, 255};

enum class State { Playing, Won, Lost };

struct Enemy {
    Rectangle ship;
    int slice = 0;
    float visibleTime = 0.0f;
    float fireTime = 0.0f;
};

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
float scale = 1.0f, width = WINDOW_WIDTH, height = WINDOW_HEIGHT;
bool quit = false, firing = false, rightDown = false;
State state = State::Playing;
Rectangle player;
std::vector<Rectangle> grid;
std::vector<Enemy> enemies;
std::vector<Circle> playerShots, enemyShots;
std::mt19937 rng{std::random_device{}()};
glm::vec2 mouseDelta{0.0f};
float shotTime = 0.0f, spawnTime = 0.0f, mouseIdleTime = 0.0f;

float randomBetween(float low, float high) {
    std::uniform_real_distribution<float> dist(low, high);
    return dist(rng);
}

void readWindowSize() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(renderer, &w, &h);
    width = static_cast<float>(w) / scale;
    height = static_cast<float>(h) / scale;
}

void initGrid() {
    grid.clear();
    const int rows = static_cast<int>(std::ceil(height / (2.0f * GRID_SIZE))) + 2;
    for (int row = 0; row < rows; ++row) {
        for (int side = 0; side < 2; ++side) {
            Rectangle square;
            square.width = square.height = GRID_SIZE;
            square.pos = glm::vec2(side ? width - GRID_SIZE / 2.0f : GRID_SIZE / 2.0f,
                                   height - GRID_SIZE / 2.0f - row * 2.0f * GRID_SIZE);
            square.color = SDL_Color{235, 235, 235, 255};
            grid.push_back(square);
        }
    }
}

void spawnEnemy(int slice) {
    const float sliceWidth = width / SLICES;
    const float lo = std::max(SHIP_SIZE / 2.0f, slice * sliceWidth + SHIP_SIZE / 2.0f);
    const float hi = std::min(width - SHIP_SIZE / 2.0f,
                              (slice + 1) * sliceWidth - SHIP_SIZE / 2.0f);
    const float x = lo <= hi // pozitia unui inamic, daca low <= high atunci exista loc pt nava in culoar
                        ? randomBetween(lo, hi)
                        : std::clamp((slice + 0.5f) * sliceWidth, SHIP_SIZE / 2.0f, // inamicul nu incae pe culoar
                                     std::max(SHIP_SIZE / 2.0f, width - SHIP_SIZE / 2.0f));
    Enemy enemy;
    enemy.ship = Rectangle(SHIP_SIZE, SHIP_SIZE, glm::vec2(x, -SHIP_SIZE / 2.0f), ORANGE);
    enemy.ship.vel = glm::vec2(0.0f, randomBetween(50.0f, 150.0f));
    enemy.slice = slice;
    enemies.push_back(enemy);
}

void resetGame() {
    state = State::Playing;
    player = Rectangle(SHIP_SIZE, SHIP_SIZE,
                       glm::vec2(width / 2.0f, height - SHIP_SIZE / 2.0f), GREEN);
    player.vel = glm::vec2(0.0f);
    enemies.clear();
    playerShots.clear();
    enemyShots.clear();
    mouseDelta = glm::vec2(0.0f);
    firing = rightDown = false;
    shotTime = spawnTime = mouseIdleTime = 0.0f;
    for (int slice = 0; slice < SLICES; ++slice) spawnEnemy(slice);
    initGrid();
}

//folosita indiferent de victorie sau lose
void finish(State result) {
    state = result;
    player.vel = glm::vec2(0.0f);
    player.color = result == State::Won ? BLUE : RED;
    enemies.clear();
    playerShots.clear();
    enemyShots.clear();
    firing = rightDown = false;
}

void firePlayer() {
    Circle shot(RADIUS, player.pos, RED);
    shot.vel = glm::vec2(0.0f, -SHOT_SPEED);
    playerShots.push_back(shot);
}

void fireEnemy(const Enemy &enemy) {
    glm::vec2 aim = player.pos - enemy.ship.pos;
    const float distance = glm::length(aim);
    aim = distance > 0.0001f ? aim / distance : glm::vec2(0.0f, 1.0f);
    constexpr float angle15 = 0.2617993878f;
    for (int side = -1; side <= 1; ++side) {
        const float a = side * angle15, c = std::cos(a), s = std::sin(a); // directtia de la inamic spre player
        Circle shot(RADIUS, enemy.ship.pos, MAGENTA);
        shot.vel = ENEMY_SHOT_SPEED * glm::vec2(aim.x * c - aim.y * s,
                                                aim.x * s + aim.y * c);
        enemyShots.push_back(shot);
    }
}

void repulse() {
    for (Circle &shot: enemyShots) {
        glm::vec2 away = shot.pos - player.pos;
        const float distance = glm::length(away);
        if (distance > 0.0001f) {
            shot.vel += 10.0f * away / distance;
        } else {
            const float speed = glm::length(shot.vel);
            if (speed > 0.0001f) shot.vel -= 10.0f * shot.vel / speed;
        }
    }
}

bool hits(const Circle &circle, const Rectangle &rect) { // coliziuni intre proiectile si nava
    const float dx = std::max(std::abs(circle.pos.x - rect.pos.x) - rect.width / 2.0f, 0.0f);
    const float dy = std::max(std::abs(circle.pos.y - rect.pos.y) - rect.height / 2.0f, 0.0f);
    return dx * dx + dy * dy <= circle.radius * circle.radius;
}

bool hits(const Rectangle &a, const Rectangle &b) { // coliziuni intre nava si nava
    return std::abs(a.pos.x - b.pos.x) <= (a.width + b.width) / 2.0f &&
           std::abs(a.pos.y - b.pos.y) <= (a.height + b.height) / 2.0f;
}

void processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) quit = true;
        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            readWindowSize();
            initGrid();
            player.pos.x = std::clamp(player.pos.x, SHIP_SIZE / 2.0f,
                                      std::max(SHIP_SIZE / 2.0f, width - SHIP_SIZE / 2.0f));
            player.pos.y = std::clamp(player.pos.y, SHIP_SIZE / 2.0f,
                                      std::max(SHIP_SIZE / 2.0f, height - SHIP_SIZE / 2.0f));
            for (Enemy &enemy: enemies) { // in caz de redimensionare fereasta
                const float sliceWidth = width / SLICES;
                const float lo = enemy.slice * sliceWidth + SHIP_SIZE / 2.0f;
                const float hi = (enemy.slice + 1) * sliceWidth - SHIP_SIZE / 2.0f;
                enemy.ship.pos.x = std::clamp(enemy.ship.pos.x, lo, hi);
            }
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) quit = true;
            if (event.key.key == SDLK_R && state != State::Playing) resetGame();
        }
        if (state != State::Playing) continue;
        if (event.type == SDL_EVENT_MOUSE_MOTION)
            mouseDelta += glm::vec2(event.motion.xrel, event.motion.yrel) / scale;
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event.button.button == SDL_BUTTON_LEFT && !firing) {
                firing = true;
                firePlayer();
                shotTime = 0.0f;
            }
            if (event.button.button == SDL_BUTTON_RIGHT && !rightDown) {
                rightDown = true;
                repulse();
            }
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            firing = false;
            shotTime = 0.0f;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
            rightDown = false;
    }
}

void movePlayer(float dt) {
    const float motion = glm::length(mouseDelta);
    if (motion > 0.0001f) {
        player.vel += mouseDelta * ACCELERATION * dt;
        mouseIdleTime = 0.0f;
    } else {
        mouseIdleTime += dt;
        // delay mic inainte de franare pt ca ev. de la mouse nu vin la fiecare cadru
        if (mouseIdleTime > 0.05f) {
            const float speed = glm::length(player.vel);
            if (speed > 0.0f) player.vel *= std::max(0.0f, 1.0f - BRAKING * dt / speed);
        }
    }
    mouseDelta = glm::vec2(0.0f);
    const float speed = glm::length(player.vel);
    if (speed > MAX_SPEED) player.vel *= MAX_SPEED / speed;
    player.pos += player.vel * dt;
    const float minEdge = SHIP_SIZE / 2.0f;
    const float maxX = std::max(minEdge, width - minEdge);
    const float maxY = std::max(minEdge, height - minEdge);
    // oprim viteza pe axa care a atins marginea ferestrei
    if (player.pos.x < minEdge || player.pos.x > maxX) {
        if (player.pos.x < minEdge) player.pos.x = minEdge;
        if (player.pos.x > maxX) player.pos.x = maxX;
        player.vel.x = 0.0f;
    }
    if (player.pos.y < minEdge || player.pos.y > maxY) {
        if (player.pos.y < minEdge) player.pos.y = minEdge;
        if (player.pos.y > maxY) player.pos.y = maxY;
        player.vel.y = 0.0f;
    }
}

void animate(float dt) {
    if (state != State::Playing) return;
    const float gridCycle = static_cast<float>(grid.size() / 2) * 2.0f * GRID_SIZE;
    for (Rectangle &square: grid) {
        square.pos.y += 100.0f * dt;
        if (square.pos.y > height + GRID_SIZE / 2.0f) square.pos.y -= gridCycle;
    }
    movePlayer(dt);
    if (firing) {
        shotTime += dt;
        while (shotTime >= SHOT_PERIOD) {
            firePlayer();
            shotTime -= SHOT_PERIOD;
        }
    }
    // dt pastreaza viteza de miscare independenta de numarul de cadre pe secunda
    for (Circle &shot: playerShots) shot.pos += shot.vel * dt;
    for (Circle &shot: enemyShots) shot.pos += shot.vel * dt;
    for (Enemy &enemy: enemies) {
        enemy.ship.pos += enemy.ship.vel * dt;
        if (enemy.ship.pos.y + SHIP_SIZE / 2.0f >= 0.0f) {
            const float before = enemy.visibleTime;
            enemy.visibleTime += dt;
            if (enemy.visibleTime >= 1.0f) {
                enemy.fireTime += dt;
                if (before < 1.0f || enemy.fireTime >= ENEMY_SHOT_PERIOD) {
                    fireEnemy(enemy);
                    enemy.fireTime = 0.0f;
                }
            }
        }
    }
    //verificare coliziuni
    for (const Enemy &enemy: enemies) {
        if (hits(player, enemy.ship)) {
            finish(State::Lost);
            return;
        }
    }
    for (const Circle &shot: enemyShots) {
        if (hits(shot, player)) {
            finish(State::Lost);
            return;
        }
    }
    bool destroyedEnemy = false;
    for (size_t i = 0; i < playerShots.size();) {
        bool hit = false;
        for (size_t j = 0; j < enemies.size(); ++j) {
            if (hits(playerShots[i], enemies[j].ship)) {
                enemies.erase(enemies.begin() + j);
                playerShots.erase(playerShots.begin() + i);
                destroyedEnemy = true;
                hit = true;
                break;
            }
        }
        if (!hit) ++i;
    }
    if (enemies.empty() && destroyedEnemy) {
        finish(State::Won);
        return;
    }
    for (int i = static_cast<int>(enemies.size()) - 1; i >= 0; --i) {
        if (enemies[i].ship.pos.y - SHIP_SIZE / 2.0f > height)
            enemies.erase(enemies.begin() + i);
    }
    for (int i = static_cast<int>(playerShots.size()) - 1; i >= 0; --i) {
        if (playerShots[i].pos.y + playerShots[i].radius < 0.0f)
            playerShots.erase(playerShots.begin() + i);
    }
    for (int i = static_cast<int>(enemyShots.size()) - 1; i >= 0; --i) {
        const Circle &shot = enemyShots[i];
        if (shot.pos.x + shot.radius < 0.0f || shot.pos.x - shot.radius > width ||
            shot.pos.y + shot.radius < 0.0f || shot.pos.y - shot.radius > height)
            enemyShots.erase(enemyShots.begin() + i);
    }
    // inamici noi odata la 3sec
    spawnTime += dt;
    if (spawnTime >= 3.0f) {
        spawnTime -= 3.0f;
        for (int slice = 0; slice < SLICES; ++slice) {
            bool occupied = false;
            for (const Enemy &enemy : enemies) {
                if (enemy.slice == slice) occupied = true;
            }
            if (!occupied) spawnEnemy(slice);
        }
    }
}

void drawFrame() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    for (Rectangle &square: grid) square.draw(renderer);
    for (Enemy &enemy: enemies) enemy.ship.draw(renderer);
    for (Circle &shot: playerShots) shot.draw(renderer);
    for (Circle &shot: enemyShots) shot.draw(renderer);
    player.draw(renderer);
    SDL_RenderPresent(renderer);
}

bool initWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL initialization failed: %s", SDL_GetError());
        return false;
    }
    scale = SDL_GetDisplayContentScale(1);
    if (scale <= 0.0f) scale = 1.0f;
    if (!SDL_CreateWindowAndRenderer("Space Shotter",
                                     static_cast<int>(scale * WINDOW_WIDTH), static_cast<int>(scale * WINDOW_HEIGHT),
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_SetWindowMinimumSize(window, static_cast<int>(scale * SLICES * SHIP_SIZE),
                             static_cast<int>(scale * SHIP_SIZE));
    readWindowSize();
    if (!SDL_SetWindowRelativeMouseMode(window, true))
        SDL_Log("Relative mouse mode failed: %s", SDL_GetError());
    SDL_HideCursor();
    return true;
}

void cleanup() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    if (!initWindow()) {
        cleanup();
        return 1;
    }
    resetGame();
    Uint64 previous = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    while (!quit) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const float dt = std::min(static_cast<float>(now - previous) / frequency, 0.05f);
        previous = now;
        processEvents();
        animate(dt);
        drawFrame();
        SDL_Delay(16);
    }
    cleanup();
    return 0;
}
