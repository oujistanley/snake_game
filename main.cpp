// Snake - windowed version using Raylib
// See CMakeLists.txt / the build notes for how to compile.

#include "raylib.h"

#include <algorithm>
#include <deque>
#include <random>
#include <string>
#include <vector>

// ---------- Settings ----------
const int GRID_W   = 30;                 // board size in cells
const int GRID_H   = 20;
const int CELL     = 24;                 // size of one cell in pixels
const int HUD_H    = 60;                 // space above the board for the score
const int SCREEN_W = GRID_W * CELL;
const int SCREEN_H = GRID_H * CELL + HUD_H;

const float START_DELAY = 0.12f;         // seconds between moves
const float MIN_DELAY   = 0.05f;

// ---------- Types ----------
struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

enum class Dir { Left, Right, Up, Down };
enum class State { Playing, Paused, GameOver, Won };

bool isOpposite(Dir a, Dir b) {
    return (a == Dir::Left  && b == Dir::Right) || (a == Dir::Right && b == Dir::Left) ||
           (a == Dir::Up    && b == Dir::Down)  || (a == Dir::Down  && b == Dir::Up);
}

// ---------- Game ----------
class Game {
public:
    Game() : rng(std::random_device{}()) { reset(); }

    void reset() {
        snake.clear();
        snake.push_back({GRID_W / 2,     GRID_H / 2});
        snake.push_back({GRID_W / 2 - 1, GRID_H / 2});
        snake.push_back({GRID_W / 2 - 2, GRID_H / 2});
        lastMoved = Dir::Right;
        queued.clear();
        score = 0;
        timer = 0.0f;
        state = State::Playing;
        spawnFood();
    }

    void handleInput() {
        if (state == State::GameOver || state == State::Won) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) reset();
            return;
        }

        if (IsKeyPressed(KEY_P)) {
            state = (state == State::Paused) ? State::Playing : State::Paused;
        }
        if (state == State::Paused) return;

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    queueDir(Dir::Up);
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  queueDir(Dir::Down);
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  queueDir(Dir::Left);
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) queueDir(Dir::Right);
    }

    void update(float dt) {
        if (state != State::Playing) return;

        timer += dt;
        float delay = currentDelay();
        while (timer >= delay && state == State::Playing) {
            timer -= delay;
            step();
        }
    }

    void draw() const {
        ClearBackground(Color{24, 26, 32, 255});

        // HUD
        DrawText(TextFormat("Score: %d", score), 16, 16, 28, RAYWHITE);
        std::string hs = "High: " + std::to_string(highScore);
        DrawText(hs.c_str(), SCREEN_W - MeasureText(hs.c_str(), 28) - 16, 16, 28, GOLD);

        // Board (checkerboard)
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                Color c = ((x + y) % 2 == 0) ? Color{36, 40, 50, 255} : Color{32, 35, 44, 255};
                DrawRectangle(x * CELL, HUD_H + y * CELL, CELL, CELL, c);
            }
        }

        // Food
        DrawCircle(food.x * CELL + CELL / 2, HUD_H + food.y * CELL + CELL / 2,
                   CELL * 0.38f, Color{235, 70, 70, 255});

        // Snake
        for (size_t i = snake.size(); i-- > 0;) {
            Rectangle r = {(float)(snake[i].x * CELL + 1), (float)(HUD_H + snake[i].y * CELL + 1),
                           (float)(CELL - 2), (float)(CELL - 2)};
            Color c = (i == 0) ? Color{120, 230, 120, 255} : Color{60, 180, 90, 255};
            DrawRectangleRounded(r, 0.35f, 6, c);
        }

        // Overlays
        if (state == State::Paused)
            drawOverlay("PAUSED", "Press P to resume");
        else if (state == State::GameOver)
            drawOverlay("GAME OVER", "Press Enter to play again");
        else if (state == State::Won)
            drawOverlay("YOU WIN!", "Press Enter to play again");
    }

private:
    std::deque<Point> snake;        // snake[0] is the head
    std::deque<Dir> queued;         // buffered turns, so fast key taps aren't lost
    Point food{0, 0};
    Dir lastMoved = Dir::Right;
    State state = State::Playing;
    int score = 0;
    int highScore = 0;
    float timer = 0.0f;
    std::mt19937 rng;

    float currentDelay() const {
        return std::max(MIN_DELAY, START_DELAY - (score / 50) * 0.01f);
    }

    void queueDir(Dir d) {
        // Compare against the last direction already planned (or last moved).
        Dir reference = queued.empty() ? lastMoved : queued.back();
        if (d == reference || isOpposite(d, reference)) return;
        if (queued.size() < 3) queued.push_back(d);
    }

    // Picks a random EMPTY cell, so it can never loop forever.
    bool spawnFood() {
        std::vector<Point> freeCells;
        for (int y = 0; y < GRID_H; y++)
            for (int x = 0; x < GRID_W; x++) {
                Point p{x, y};
                if (std::find(snake.begin(), snake.end(), p) == snake.end())
                    freeCells.push_back(p);
            }
        if (freeCells.empty()) return false;

        std::uniform_int_distribution<size_t> pick(0, freeCells.size() - 1);
        food = freeCells[pick(rng)];
        return true;
    }

    void step() {
        Dir dir = lastMoved;
        if (!queued.empty()) {
            dir = queued.front();
            queued.pop_front();
        }

        Point head = snake.front();
        switch (dir) {
            case Dir::Left:  head.x--; break;
            case Dir::Right: head.x++; break;
            case Dir::Up:    head.y--; break;
            case Dir::Down:  head.y++; break;
        }

        // Wall collision
        if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H) {
            state = State::GameOver;
            return;
        }

        bool ate = (head == food);

        // Self collision (the tail cell is free if the snake isn't growing)
        size_t limit = snake.size() - (ate ? 0 : 1);
        for (size_t i = 0; i < limit; i++) {
            if (snake[i] == head) {
                state = State::GameOver;
                return;
            }
        }

        snake.push_front(head);
        lastMoved = dir;

        if (ate) {
            score += 10;
            highScore = std::max(highScore, score);
            if (!spawnFood()) state = State::Won;
        } else {
            snake.pop_back();
        }
    }

    void drawOverlay(const char* title, const char* hint) const {
        DrawRectangle(0, HUD_H, SCREEN_W, GRID_H * CELL, Color{0, 0, 0, 160});
        int cy = HUD_H + (GRID_H * CELL) / 2;
        DrawText(title, (SCREEN_W - MeasureText(title, 56)) / 2, cy - 50, 56, RAYWHITE);
        DrawText(hint,  (SCREEN_W - MeasureText(hint, 24)) / 2,  cy + 20, 24, LIGHTGRAY);
    }
};

// ---------- Main ----------
int main() {
    InitWindow(SCREEN_W, SCREEN_H, "Snake");
    SetTargetFPS(60);

    Game game;

    while (!WindowShouldClose()) {      // Esc or the close button quits
        game.handleInput();
        game.update(GetFrameTime());

        BeginDrawing();
        game.draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
