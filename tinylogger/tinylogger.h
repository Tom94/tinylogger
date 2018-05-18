// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the BSD 3-Clause License within the LICENSE.md file.

#pragma once

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <set>

#ifndef _WIN32
#   include <sys/ioctl.h>
#   include <unistd.h>
#endif

namespace tlog {
      ///////////////////////////////////////
     /// Shared functions and interfaces ///
    ///////////////////////////////////////

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

    enum class ESeverity {
        None,
        Info,
        Debug,
        Warning,
        Error,
        Success,
        Progress,
    };

    class IOutput {
    public:
        // No need to be beyond microseconds accuracy
        using duration_t = std::chrono::microseconds;

        virtual void writeLine(ESeverity severity, const std::string& line) = 0;
        virtual void writeProgress(uint64_t current, uint64_t total, duration_t duration) = 0;
    };


      ///////////////////////////////
     /// IOutput implementations ///
    ///////////////////////////////

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

    class ConsoleOutput : public IOutput {
    public:
        ~ConsoleOutput() {
            if (mSupportsAnsiControlSequences) {
                std::cout << ansi::RESET;
            }
        }

        static std::shared_ptr<ConsoleOutput>& global() {
            static auto consoleOutput = std::shared_ptr<ConsoleOutput>(new ConsoleOutput());
            return consoleOutput;
        }

        void writeLine(ESeverity severity, const std::string& line) override {
            std::string textOut;

            if (severity != ESeverity::None) {
                using namespace std::chrono;

                // Display time format
                const auto currentTime = system_clock::to_time_t(system_clock::now());

                char timeStr[10];
                if (std::strftime(timeStr, 10, "%H:%M:%S ", localtime(&currentTime)) == 0) {
                    throw std::runtime_error{"Could not render local time."};
                }

                textOut += timeStr;
            }

            // Color for severities
            if (mSupportsAnsiControlSequences) {
                switch (severity) {
                    case ESeverity::None:                                   break;
                    case ESeverity::Success:  textOut += ansi::GREEN;       break;
                    case ESeverity::Info:     textOut += ansi::CYAN;        break;
                    case ESeverity::Warning:  textOut += ansi::BOLD_YELLOW; break;
                    case ESeverity::Debug:    textOut += ansi::BOLD_CYAN;   break;
                    case ESeverity::Error:    textOut += ansi::RED;         break;
                    case ESeverity::Progress: textOut += ansi::CYAN;        break;
                }
            }

            // Severity itself
            switch (severity) {
                case ESeverity::None:                             break;
                case ESeverity::Success:  textOut += "SUCCESS  "; break;
                case ESeverity::Info:     textOut += "INFO     "; break;
                case ESeverity::Warning:  textOut += "WARNING  "; break;
                case ESeverity::Debug:    textOut += "DEBUG    "; break;
                case ESeverity::Error:    textOut += "ERROR    "; break;
                case ESeverity::Progress: textOut += "PROGRESS "; break;
            }

            if (mSupportsAnsiControlSequences && severity != ESeverity::None) {
                textOut += ansi::RESET;
            }

            textOut += line;

            if (mSupportsAnsiControlSequences) {
                textOut += ansi::ERASE_TO_END_OF_LINE + ansi::RESET;
            }

            // Make sure there is a linebreak in the end. We don't want duplicates!
            if (mSupportsAnsiControlSequences && severity == ESeverity::Progress) {
                textOut += ansi::LINE_BEGIN;
            } else if (textOut.empty() || textOut.back() != '\n') {
                textOut += '\n';
            }

            auto& stream = severity == ESeverity::Warning || severity == ESeverity::Error ? std::cerr : std::cout;
            stream << textOut << std::flush;
        }

        void writeProgress(uint64_t current, uint64_t total, duration_t duration) override {
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
            writeLine(ESeverity::Progress, string{"["} + body + "] " + label);
        }

    private:
        ConsoleOutput() {
            mSupportsAnsiControlSequences = enableAnsiControlSequences();
            if (mSupportsAnsiControlSequences) {
                std::cout << ansi::RESET;
            }
        }

        static bool enableAnsiControlSequences() {
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

        bool mSupportsAnsiControlSequences;
    };


      /////////////////////////////////////////
     /// Logger stuff for managing outputs ///
    /////////////////////////////////////////

    class Logger {
    public:
        Logger() {
#ifdef NDEBUG
            hideType(ESeverity::Debug);
#endif
        }

        Logger(std::set<std::shared_ptr<IOutput>> outputs) : Logger() {
            mOutputs = outputs;
        }

        static std::unique_ptr<Logger>& global() {
            static auto logger = std::unique_ptr<Logger>(new Logger({ConsoleOutput::global()}));
            return logger;
        }

        void log(ESeverity severity, const std::string& line) {
            if (mHiddenTypes.count(severity)) {
                return;
            }

            for (auto& output : mOutputs) {
                output->writeLine(severity, line);
            }
        }

        template <typename T>
        void logProgress(uint64_t current, uint64_t total, T duration) {
            if (mHiddenTypes.count(ESeverity::Progress)) {
                return;
            }

            IOutput::duration_t dur = std::chrono::duration_cast<IOutput::duration_t>(duration);
            for (auto& output : mOutputs) {
                output->writeProgress(current, total, dur);
            }
        }

        void addOutput(std::shared_ptr<IOutput>& output)    { mOutputs.insert(output); }
        void removeOutput(std::shared_ptr<IOutput>& output) { mOutputs.erase(output); }

        void hideType(ESeverity severity) { mHiddenTypes.insert(severity); }
        void showType(ESeverity severity) { mHiddenTypes.erase(severity); }

    private:
        std::set<ESeverity> mHiddenTypes;
        std::set<std::shared_ptr<IOutput>> mOutputs;
    };

    class LogStream {
    public:
        LogStream(Logger* logger, ESeverity severity)
        : mLogger{logger}, mSeverity{severity} {}

        LogStream(LogStream&& other) = default;
        ~LogStream() {
            mLogger->log(mSeverity, mText.str());
        }

        LogStream& operator=(LogStream&& other) = default;

        template <typename T>
        LogStream& operator<<(const T& elem) {
            mText << elem;
            return *this;
        }

    private:
        Logger* mLogger;
        ESeverity mSeverity;
        std::ostringstream mText;
    };

    inline LogStream log(ESeverity severity) {
        return LogStream{Logger::global().get(), severity};
    }

    inline LogStream none()    { return log(ESeverity::None); }
    inline LogStream info()    { return log(ESeverity::Info); }
    inline LogStream debug()   { return log(ESeverity::Debug); }
    inline LogStream warning() { return log(ESeverity::Warning); }
    inline LogStream error()   { return log(ESeverity::Error); }
    inline LogStream success() { return log(ESeverity::Success); }

    template <typename T>
    void progress(uint64_t current, uint64_t total, T duration) {
        Logger::global()->logProgress(current, total, duration);
    }
}
