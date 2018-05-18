#include "tinylogger/tinylogger.h"

#include <chrono>
#include <thread>

int main() {
    tlog::none()
        << "===========================================\n"
        << "===== tinylogger demo =====================\n"
        << "===========================================";

    tlog::info()
        << "This is an info message meant to let you know "
        << 42 <<
        " is the Answer to the Ultimate Question of Life, the Universe, and Everything";

    tlog::debug() << "You can only see this message if this demo has been compiled in debug mode.";

    tlog::warning() << "This " << "is " << "a " << "warning!";

    tlog::error()
        << "This is an error message that is so long, it likely does not fit within a single line. "
        << "The purpose of this message is to make sure line breaks do not break anything (get it?). "
        << "This sentence only exists to make this message even longer.";

    tlog::info() << "Demonstrating a progress bar...";

    static const int N = 100;
    auto now = std::chrono::steady_clock::now();
    for (int i = 1; i <= N; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        tlog::progress(i, N, std::chrono::steady_clock::now() - now);
    }

    tlog::success() << "The demo application terminated successfully. Hooray!";
}
