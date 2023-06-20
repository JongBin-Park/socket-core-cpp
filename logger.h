#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>
#include <fmt/color.h>
#include <fmt/chrono.h>

typedef enum _LogLevel
{
    LOG_LEVEL_OFF=0,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} LogLevel;

class Logger
{
private:
    LogLevel m_level;

public:
    Logger(LogLevel level = LOG_LEVEL_INFO) : m_level(level) {}
    ~Logger() {}

    void setLevel(LogLevel level) { m_level = level; }

    template<typename... Args>
    void writeLog(LogLevel level, std::string str, Args... args)
    {
        if(m_level <= level)
        {
            std::string content = "";
            std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();

            switch(level)
            {
                case LOG_LEVEL_DEBUG: 
                    content = fmt::format(fg(fmt::color::sky_blue), "[{:%Y-%m-%d %H:%M:%S}] [D] " + str, t, args...);
                    break;
                case LOG_LEVEL_INFO: 
                    content = fmt::format(fg(fmt::color::lime_green), "[{:%Y-%m-%d %H:%M:%S}] [I] " + str, t, args...);
                    break;
                case LOG_LEVEL_WARN: 
                    content = fmt::format(fg(fmt::color::green_yellow), "[{:%Y-%m-%d %H:%M:%S}] [W] " + str, t, args...);
                    break;
                case LOG_LEVEL_ERROR: 
                    content = fmt::format(fg(fmt::color::indian_red), "[{:%Y-%m-%d %H:%M:%S}] [E] " + str, t, args...);
                    break;
                case LOG_LEVEL_OFF:
                    return;
            }

            std::cout << content << std::endl;
        }
    }

    template<typename... Args>
    void debug(std::string str, Args... args)
    {
        writeLog(LOG_LEVEL_DEBUG, str, args...);
    }
    template<typename... Args>
    void info(std::string str, Args... args)
    {
        writeLog(LOG_LEVEL_INFO, str, args...);
    }
    template<typename... Args>
    void warn(std::string str, Args... args)
    {
        writeLog(LOG_LEVEL_WARN, str, args...);
    }
    template<typename... Args>
    void error(std::string str, Args... args)
    {
        writeLog(LOG_LEVEL_ERROR, str, args...);
    }
};

#endif /* __LOGGER_H__ */