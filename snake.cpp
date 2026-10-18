// Змейка для терминала (Linux/macOS). Управление: WASD или стрелки, P — пауза, Q — выход.
// Сборка:  g++ -std=c++20 -O2 snake.cpp -o snake
// Запуск:  ./snake

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

namespace {

constexpr int kWidth = 40;   // игровое поле в клетках
constexpr int kHeight = 20;

enum class Dir { Up, Down, Left, Right };

struct Pos {
    int x = 0, y = 0;
    bool operator==(const Pos& o) const { return x == o.x && y == o.y; }
};

termios g_origTermios{};
bool g_termiosSaved = false;

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &g_origTermios) == 0) {
        g_termiosSaved = true;
        termios raw = g_origTermios;
        raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON) | static_cast<tcflag_t>(ECHO));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
}

void disableRawMode() {
    if (g_termiosSaved) tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_origTermios);
}

// Читает нажатую клавишу без блокировки; поддерживает escape-последовательности стрелок.
int readKey() {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        timeval tv{0, 0};
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) return -1;

        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) != 1) return -1;
        if (c == 27) {  // возможная escape-последовательность: ESC [ A/B/C/D
            unsigned char seq[2];
            timeval t2{0, 20000};  // 20 мс
            fd_set fds2;
            FD_ZERO(&fds2);
            FD_SET(STDIN_FILENO, &fds2);
            if (select(STDIN_FILENO + 1, &fds2, nullptr, nullptr, &t2) > 0 &&
                read(STDIN_FILENO, seq, 1) == 1 && seq[0] == '[' &&
                read(STDIN_FILENO, seq, 1) == 1) {
                switch (seq[0]) {
                    case 'A': return 'w';
                    case 'B': return 's';
                    case 'C': return 'd';
                    case 'D': return 'a';
                }
            }
            return -1;
        }
        return std::tolower(c);
}

Pos move(const Pos& p, Dir d) {
    Pos r = p;
    switch (d) {
        case Dir::Up: --r.y; break;
        case Dir::Down: ++r.y; break;
        case Dir::Left: --r.x; break;
        case Dir::Right: ++r.x; break;
    }
    return r;
}

}  // namespace

int main() {
    enableRawMode();
    std::atexit(disableRawMode);

    std::mt19937 rng(std::random_device{}());

    std::deque<Pos> snake{{10, 10}, {9, 10}, {8, 10}};
    Dir dir = Dir::Right, nextDir = Dir::Right;
    Pos food{5, 5};
    long score = 0;
    bool paused = false;

    auto placeFood = [&] {
        std::uniform_int_distribution<int> dx(0, kWidth - 1), dy(0, kHeight - 1);
        do {
            food = {dx(rng), dy(rng)};
        } while (std::find(snake.begin(), snake.end(), food) != snake.end());
    };
    placeFood();

    std::cout << "\x1b[2J"  // очистить экран
              << "\x1b[?25l";  // спрятать курсор
    std::atexit([] { std::cout << "\x1b[?25h" << std::flush; });

    auto render = [&](const std::string& status) {
        std::string out;
        out.reserve((kWidth + 6) * (kHeight + 6));
        out += "\x1b[H";  // домой
        out += "  \x1b[1mЗМЕЙКА\x1b[0m  Счёт: " + std::to_string(score) + "  " + status + "\n";
        out += "  +" + std::string(kWidth, '-') + "+\n";
        for (int y = 0; y < kHeight; ++y) {
            out += "  |";
            for (int x = 0; x < kWidth; ++x) {
                Pos p{x, y};
                if (p == snake.front()) out += "\x1b[92m@\x1b[0m";       // голова — зелёная
                else if (std::find(snake.begin(), snake.end(), p) != snake.end())
                    out += "\x1b[32mo\x1b[0m";                             // тело
                else if (p == food) out += "\x1b[91m*\x1b[0m";            // еда — красная
                else out += ' ';
            }
            out += "|\n";
        }
        out += "  +" + std::string(kWidth, '-') + "+\n";
        out += "  WASD/стрелки — движение, P — пауза, Q — выход\n";
        std::cout << out << std::flush;
    };

    int speedMs = 130;
    while (true) {
        int key = readKey();
        if (key == 'q' || key == 3 /*Ctrl+C*/) break;
        if (key == 'p') paused = !paused;
        if (!paused) {
            if ((key == 'w' || key == '1') && dir != Dir::Down) nextDir = Dir::Up;
            else if ((key == 's' || key == '2') && dir != Dir::Up) nextDir = Dir::Down;
            else if ((key == 'a' || key == '3') && dir != Dir::Right) nextDir = Dir::Left;
            else if ((key == 'd' || key == '4') && dir != Dir::Left) nextDir = Dir::Right;
        }

        if (paused) {
            render("[ПАУЗА]");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        dir = nextDir;
        Pos head = move(snake.front(), dir);

        bool collision = head.x < 0 || head.y < 0 || head.x >= kWidth || head.y >= kHeight ||
                         std::find(snake.begin(), snake.end(), head) != snake.end();
        if (collision) {
            render("GAME OVER! Нажми любую букву...");
            int k = readKey();
            if (k == -1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            break;
        }

        snake.push_front(head);
        if (head == food) {
            ++score;
            speedMs = std::max(60, speedMs - 3);  // ускорение с каждой съеденной едой
            placeFood();
        } else {
            snake.pop_back();
        }

        render("");
        std::this_thread::sleep_for(std::chrono::milliseconds(speedMs));
    }

    std::cout << "\x1b[0m\nКонец игры. Твой счёт: " << score << std::endl;
    return 0;
}
