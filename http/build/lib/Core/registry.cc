
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/pstats.h>
#include <celestis/command.h>
#include <celestis/configuration.h>

bool initialized = false;

// Configuration
Configuration *globalConfig = nullptr;

// Pstats
std::map<std::string, PerfSource*> *stats = nullptr;

// Commands
std::unordered_map<std::string, Command*> *cmds = nullptr;

void
Registry_Init()
{
    if (initialized)
        return;

    initialized = true;

    stats = new std::map<std::string, PerfSource*>();
    cmds = new std::unordered_map<std::string, Command*>();
    globalConfig = new Configuration();
}

