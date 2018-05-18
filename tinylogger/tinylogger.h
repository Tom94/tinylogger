// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the BSD 3-Clause License within the LICENSE.md file.

#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_set>

#ifndef _WIN32
#   include <sys/ioctl.h>
#   include <unistd.h>
#endif

namespace tlog {
    inline std::string padTo(std::string str, size_t length, const char paddingChar = ' ') {
        if (length > str.size()) {
            str.insert(0, length - str.size(), paddingChar);
        }
        return str;
    }

    template <typename T>
    std::string durationToString(T dur)
    {
        using namespace std;
        using namespace std::chrono;
        using day_t = duration<long long, std::ratio<3600 * 24>>;

        auto d = duration_cast<day_t>(dur);
        auto h = duration_cast<hours>(dur -= d);
        auto m = duration_cast<minutes>(dur -= h);
        auto s = duration_cast<seconds>(dur -= m);

        if (d.count() > 0) {
            return
                to_string(d.count()) + "d" +
                padTo(to_string(h.count()), 2, '0') + "h" +
                padTo(to_string(m.count()), 2, '0') + "m" +
                padTo(to_string(s.count()), 2, '0') + "s";
        } else if (h.count() > 0) {
            return
                to_string(h.count()) + "h" +
                padTo(to_string(m.count()), 2, '0') + "m" +
                padTo(to_string(s.count()), 2, '0') + "s";
        } else if (m.count() > 0) {
            return
                to_string(m.count()) + "m" +
                padTo(to_string(s.count()), 2, '0') + "s";
        } else {
            return to_string(s.count()) + "s";
        }
    }

    namespace ansi {
        const std::string ESC = "\033";

        const std::string RESET = ESC + "[0;37m";
        const std::string LINE_BEGIN = ESC + "[0G";
        const std::string ERASE_TO_END_OF_LINE = ESC + "[K";

        const std::string BLACK   = ESC + "[0;30m";
        const std::string RED     = ESC + "[0;31m";
        const std::string GREEN   = ESC + "[0;32m";
        const std::string YELLOW  = ESC + "[0;33m";
        const std::string BLUE    = ESC + "[0;34m";
        const std::string MAGENTA = ESC + "[0;35m";
        const std::string CYAN    = ESC + "[0;36m";
        const std::string WHITE   = ESC + "[0;37m";

        const std::string BOLD_BLACK   = ESC + "[1;30m";
        const std::string BOLD_RED     = ESC + "[1;31m";
        const std::string BOLD_GREEN   = ESC + "[1;32m";
        const std::string BOLD_YELLOW  = ESC + "[1;33m";
        const std::string BOLD_BLUE    = ESC + "[1;34m";
        const std::string BOLD_MAGENTA = ESC + "[1;35m";
        const std::string BOLD_CYAN    = ESC + "[1;36m";
        const std::string BOLD_WHITE   = ESC + "[1;37m";

        const std::string HIDE_CURSOR = ESC + "[?25l";
        const std::string SHOW_CURSOR = ESC + "[?25h";
    }

    class Logger {
    public:
        enum ESeverity {
            None,
            Info,
            Debug,
            Warning,
            Error,
            Success,
            Progress,
        };

        enum EColor : unsigned char {
            Black = 30,
            Red = 31,
            Green = 32,
            Yellow = 33,
            Blue = 34,
            Magenta = 35,
            Cyan = 36,
            White = 37,
        };

        ~Logger() {
            std::cout << ansi::RESET;
        }

        static std::unique_ptr<Logger>& Instance() {
            static auto pLog = createLogger();
            return pLog;
        }


        std::string wrapInColor(ESeverity severity, std::string string) {
            // switch (severity) {
            //     case None:                                                                break;
            //     case Success:  textOut += ansi::GREEN        "SUCCESS  " ansi::RESET; break;
            //     case Info:     textOut += ansi::CYAN         "INFO     " ansi::RESET; break;
            //     case Warning:  textOut += ansi::BOLD_YELLOW  "WARNING  " ansi::RESET; break;
            //     case Debug:    textOut += ansi::BOLD_CYAN    "DEBUG    " ansi::RESET; break;
            //     case Error:    textOut += ansi::RED          "ERROR    " ansi::RESET; break;
            //     case Progress: textOut += ansi::CYAN         "PROGRESS " ansi::RESET; break;
            // }

            return "";
        }

        void Log(ESeverity severity, const std::string& text) {
            if (mHiddenTypes.count(severity)) {
                return;
            }

            std::string textOut;

            if (severity != None) {
                using namespace std::chrono;

                // Display time format
                const auto currentTime = system_clock::to_time_t(system_clock::now());

                char timeStr[10];
                if (std::strftime(timeStr, 10, "%H:%M:%S ", localtime(&currentTime)) == 0) {
                    throw std::runtime_error{"Could not render local time."};
                }

                textOut += timeStr;
            }

            switch (severity) {
                case None:                                                               break;
                case Success:  textOut += ansi::GREEN       + "SUCCESS  " + ansi::RESET; break;
                case Info:     textOut += ansi::CYAN        + "INFO     " + ansi::RESET; break;
                case Warning:  textOut += ansi::BOLD_YELLOW + "WARNING  " + ansi::RESET; break;
                case Debug:    textOut += ansi::BOLD_CYAN   + "DEBUG    " + ansi::RESET; break;
                case Error:    textOut += ansi::RED         + "ERROR    " + ansi::RESET; break;
                case Progress: textOut += ansi::CYAN        + "PROGRESS " + ansi::RESET; break;
            }

            // Reset after each message
            textOut += text + ansi::ERASE_TO_END_OF_LINE + ansi::RESET;

            // Make sure there is a linebreak in the end. We don't want duplicates!
            if (severity == Progress) {
                textOut += ansi::LINE_BEGIN;
            } else if (textOut.empty() || textOut.back() != '\n') {
                textOut += '\n';
            }

            auto& stream = severity == Warning || severity == Error ? std::cerr : std::cout;
            stream << textOut << std::flush;
        }

        template <typename T>
        void LogProgress(uint64_t current, uint64_t total, T duration) {
            using namespace std;

            double fraction = (double)current / total;

            // Percentage display. Looks like so:
            //  69%
            int percentage = (int)std::round(fraction * 100);
            string percentageStr = padTo(to_string(percentage) + "%", 4);

            // Fraction display. Looks like so:
            // ( 123/1337)
            string totalStr = to_string(total);
            string fractionStr = padTo(to_string(current) + "/" + totalStr, totalStr.size() * 2 + 1);

            // Time display. Looks like so:
            //     3s/17m03s
            auto projectedDuration = duration * (1 / fraction);
            auto projectedDurationStr = durationToString(projectedDuration);
            string timeStr = padTo(durationToString(duration) + "/" + projectedDurationStr, projectedDurationStr.size() * 2 + 1);

            // Put the label together. Looks like so:
            //  69% ( 123/1337)     3s/17m03s
            string label = percentageStr + " (" + fractionStr + ") " + timeStr;

            // Build the progress bar itself. Looks like so:
            // [=================>                         ]
            int usableWidth = max(0, consoleWidth()
                - 2 // The surrounding [ and ]
                - 1 // Space between progress bar and label
                - (int)label.size() // Label itself
                - 18 // Prefix
            );

            int numFilledChars = (int)round(usableWidth * fraction);

            string body(usableWidth, ' ');
            if (numFilledChars > 0) {
                for (int i = 0; i < numFilledChars; ++i)
                    body[i] = '=';
                if (numFilledChars < usableWidth) {
                    body[numFilledChars] = '>';
                }
            }

            // Put everything together. Looks like so:
            // [=================>                         ]  69% ( 123/1337)     3s/17m03s
            string message = string{"["} + body + "] " + label;

            Log(Progress, message);
        }

    private:
        Logger() {
#ifdef NDEBUG
            hideType(Debug);
#endif
            mCanHandleControlCharacters = enableControlCharacters();
        }

        static std::unique_ptr<Logger> createLogger() {
            auto pLogger = std::unique_ptr<Logger>(new Logger());

            std::cout << ansi::RESET;
            pLogger->Log(Debug, "Program runs in debug mode.");

            return pLogger;
        }

        static int consoleWidth() {
#ifdef _WIN32
            ansi::SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
            winsize size;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
            return size.ws_col;
#endif
        }

        void hideType(ESeverity severity) { mHiddenTypes.insert(severity); }
        void showType(ESeverity severity) { mHiddenTypes.erase(severity); }

        static bool enableControlCharacters() {
#ifdef _WIN32
            // Set output mode to handle virtual terminal sequences
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut == INVALID_HANDLE_VALUE) {
                return false;
            }
            DWORD dwMode = 0;
            if (!GetConsoleMode(hOut, &dwMode)) {
                return false;
            }
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (!SetConsoleMode(hOut, dwMode)) {
                return false;
            }
#endif

            return true;
        }

        bool mCanHandleControlCharacters;
        std::unordered_set<typename std::underlying_type<ESeverity>::type> mHiddenTypes;
    };

    class LogStream {
    public:
        LogStream(Logger::ESeverity severity) : mSeverity{severity} {}
        LogStream(LogStream&& other) = default;
        ~LogStream() {
            Logger::Instance()->Log(mSeverity, mText.str());
        }

        LogStream& operator=(LogStream&& other) = default;

        template <typename T>
        LogStream& operator<<(const T& elem) {
            mText << elem;
            return *this;
        }

    private:
        Logger::ESeverity mSeverity;
        std::ostringstream mText;
    };

    inline LogStream Log(Logger::ESeverity severity) {
        return LogStream{severity};
    }

    inline LogStream None()    { return Log(Logger::None); }
    inline LogStream Info()    { return Log(Logger::Info); }
    inline LogStream Debug()   { return Log(Logger::Debug); }
    inline LogStream Warning() { return Log(Logger::Warning); }
    inline LogStream Error()   { return Log(Logger::Error); }
    inline LogStream Success() { return Log(Logger::Success); }

    template <typename T>
    void Progress(uint64_t current, uint64_t total, T duration) {
        Logger::Instance()->LogProgress(current, total, duration);
    }
}
