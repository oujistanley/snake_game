// Snake - windowed version using Raylib
// See CMakeLists.txt / the build notes for how to compile.

#include "raylib.h"

#include <algorithm>
#include <deque>
#include <random>
#include <string>
#include <vector>

// ---------- Settings ----------
const int GRID_W = 30;                 // board size in cells
const int GRID_H = 20;
const int CELL = 24;                 // size of one cell in pixels
const int HUD_H = 60;                 // space above the board for the score
const int SCREEN_W = GRID_W * CELL;
const int SCREEN_H = GRID_H * CELL + HUD_H;

// ---------- Difficulty levels ----------
// Smaller delay = faster snake. Every 5 pieces of food the delay drops by
// speedUpStep, until it reaches minDelay.
struct Difficulty {
    const char* name;
    float startDelay;      // seconds between moves at the start
    float minDelay;        // the fastest the snake can ever get
    float speedUpStep;     // how much faster after every 5 food
    int   points;          // score for each piece of food
};

const Difficulty DIFFICULTIES[] = {
    {"Easy",   0.16f, 0.090f, 0.005f,  5},
    {"Normal", 0.12f, 0.050f, 0.010f, 10},
    {"Hard",   0.09f, 0.035f, 0.010f, 15},
};
const int DIFFICULTY_COUNT = 3;

// ---------- Types ----------
struct Point {
    int x, y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

enum class Dir { Left, Right, Up, Down };
enum class State { Menu, Playing, Paused, GameOver, Won };

bool isOpposite(Dir a, Dir b) {
    return (a == Dir::Left && b == Dir::Right) || (a == Dir::Right && b == Dir::Left) ||
        (a == Dir::Up && b == Dir::Down) || (a == Dir::Down && b == Dir::Up);
}

// ---------- Game ----------
class Game {
public:
    Game() : rng(std::random_device{}()) {
        reset();
        state = State::Menu;             // start on the difficulty menu
    }

    // Starts a fresh round on the currently selected difficulty.
    void reset() {
        snake.clear();
        snake.push_back({ GRID_W / 2,     GRID_H / 2 });
        snake.push_back({ GRID_W / 2 - 1, GRID_H / 2 });
        snake.push_back({ GRID_W / 2 - 2, GRID_H / 2 });
        lastMoved = Dir::Right;
        queued.clear();
        score = 0;
        foodEaten = 0;
        timer = 0.0f;
        state = State::Playing;
        spawnFood();
    }

    void handleInput() {
        if (state == State::Menu) {
            handleMenuInput();
            return;
        }

        if (state == State::GameOver || state == State::Won) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) reset();
            if (IsKeyPressed(KEY_M)) state = State::Menu;
            return;
        }

        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_SPACE)) {
            state = (state == State::Paused) ? State::Playing : State::Paused;
        }
        if (state == State::Paused) {
            if (IsKeyPressed(KEY_M)) state = State::Menu;
            return;
        }

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    queueDir(Dir::Up);
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  queueDir(Dir::Down);
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  queueDir(Dir::Left);
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) queueDir(Dir::Right);
    }

    // Called when the window loses focus, so the snake doesn't keep
    // moving while you're looking at something else.
    void pauseIfPlaying() {
        if (state == State::Playing) state = State::Paused;
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
        ClearBackground(Color{ 24, 26, 32, 255 });

        if (state == State::Menu) {
            drawMenu();
            return;
        }

        // HUD
        DrawText(TextFormat("Score: %d", score), 16, 16, 28, RAYWHITE);

        const char* levelName = DIFFICULTIES[level].name;
        DrawText(levelName, (SCREEN_W - MeasureText(levelName, 24)) / 2, 20, 24, SKYBLUE);

        std::string hs = "High: " + std::to_string(highScores[level]);
        DrawText(hs.c_str(), SCREEN_W - MeasureText(hs.c_str(), 28) - 16, 16, 28, GOLD);

        // Board (checkerboard)
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                Color c = ((x + y) % 2 == 0) ? Color{ 36, 40, 50, 255 } : Color{ 32, 35, 44, 255 };
                DrawRectangle(x * CELL, HUD_H + y * CELL, CELL, CELL, c);
            }
        }

        // Food
        DrawCircle(food.x * CELL + CELL / 2, HUD_H + food.y * CELL + CELL / 2,
            CELL * 0.38f, Color{ 235, 70, 70, 255 });

        // Snake
        for (size_t i = snake.size(); i-- > 0;) {
            Rectangle r = { (float)(snake[i].x * CELL + 1), (float)(HUD_H + snake[i].y * CELL + 1),
                           (float)(CELL - 2), (float)(CELL - 2) };
            Color c = (i == 0) ? Color{ 120, 230, 120, 255 } : Color{ 60, 180, 90, 255 };
            DrawRectangleRounded(r, 0.35f, 6, c);
        }

        // Overlays
        if (state == State::Paused)
            drawOverlay("PAUSED", "P or Space: resume", "M: change difficulty");
        else if (state == State::GameOver)
            drawOverlay("GAME OVER", "Enter: play again", "M: change difficulty");
        else if (state == State::Won)
            drawOverlay("YOU WIN!", "Enter: play again", "M: change difficulty");
    }

private:
    std::deque<Point> snake;        // snake[0] is the head
    std::deque<Dir> queued;         // buffered turns, so fast key taps aren't lost
    Point food{ 0, 0 };
    Dir lastMoved = Dir::Right;
    State state = State::Menu;
    int level = 1;                  // index into DIFFICULTIES (starts on Normal)
    int score = 0;
    int foodEaten = 0;
    int highScores[DIFFICULTY_COUNT] = { 0, 0, 0 };   // one high score per level
    float timer = 0.0f;
    std::mt19937 rng;

    float currentDelay() const {
        const Difficulty& d = DIFFICULTIES[level];
        return std::max(d.minDelay, d.startDelay - (foodEaten / 5) * d.speedUpStep);
    }

    void handleMenuInput() {
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))
            level = (level + DIFFICULTY_COUNT - 1) % DIFFICULTY_COUNT;
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))
            level = (level + 1) % DIFFICULTY_COUNT;

        if (IsKeyPressed(KEY_ONE)) { level = 0; reset(); }
        else if (IsKeyPressed(KEY_TWO)) { level = 1; reset(); }
        else if (IsKeyPressed(KEY_THREE)) { level = 2; reset(); }
        else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) reset();
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
                Point p{ x, y };
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
            score += DIFFICULTIES[level].points;
            foodEaten++;
            highScores[level] = std::max(highScores[level], score);
            if (!spawnFood()) state = State::Won;
        }
        else {
            snake.pop_back();
        }
    }

    void drawMenu() const {
        const char* title = "SNAKE";
        DrawText(title, (SCREEN_W - MeasureText(title, 72)) / 2, 50, 72, Color{ 120, 230, 120, 255 });

        const char* sub = "Choose difficulty";
        DrawText(sub, (SCREEN_W - MeasureText(sub, 28)) / 2, 140, 28, RAYWHITE);

        for (int i = 0; i < DIFFICULTY_COUNT; i++) {
            const Difficulty& d = DIFFICULTIES[i];
            bool selected = (i == level);

            float y = 200.0f + i * 70.0f;
            Rectangle box = { SCREEN_W / 2.0f - 220.0f, y, 440.0f, 56.0f };
            DrawRectangleRounded(box, 0.3f, 8,
                selected ? Color{ 60, 180, 90, 255 } : Color{ 36, 40, 50, 255 });

            Color text = selected ? Color{ 15, 20, 15, 255 } : RAYWHITE;
            DrawText(TextFormat("%d   %s", i + 1, d.name), (int)box.x + 20, (int)y + 14, 28, text);

            const char* info = TextFormat("%d pts   Best: %d", d.points, highScores[i]);
            DrawText(info, (int)(box.x + box.width) - MeasureText(info, 20) - 20, (int)y + 18, 20, text);
        }

        const char* hint1 = "W / S or arrows to choose, Enter to start";
        const char* hint2 = "Press 1, 2 or 3 to start straight away   |   Esc to quit";
        DrawText(hint1, (SCREEN_W - MeasureText(hint1, 22)) / 2, 440, 22, LIGHTGRAY);
        DrawText(hint2, (SCREEN_W - MeasureText(hint2, 18)) / 2, 475, 18, GRAY);
    }

    void drawOverlay(const char* title, const char* hint, const char* hint2) const {
        DrawRectangle(0, HUD_H, SCREEN_W, GRID_H * CELL, Color{ 0, 0, 0, 160 });
        int cy = HUD_H + (GRID_H * CELL) / 2;
        DrawText(title, (SCREEN_W - MeasureText(title, 56)) / 2, cy - 60, 56, RAYWHITE);
        DrawText(hint, (SCREEN_W - MeasureText(hint, 24)) / 2, cy + 10, 24, LIGHTGRAY);
        DrawText(hint2, (SCREEN_W - MeasureText(hint2, 24)) / 2, cy + 44, 24, LIGHTGRAY);
    }
};

// ---------- Main ----------
int main() {
    InitWindow(SCREEN_W, SCREEN_H, "Snake");
    SetTargetFPS(60);

    Game game;

    while (!WindowShouldClose()) {      // Esc or the close button quits
        if (!IsWindowFocused()) game.pauseIfPlaying();   // auto-pause

        game.handleInput();
        game.update(GetFrameTime());

        BeginDrawing();
        game.draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
