
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <celestis/debug.h>
#include <celestis/command.h>

using namespace std;

extern void Registry_Init();
extern std::unordered_map<std::string, Command*> *cmds;

void
cmd_exit(IShell *s, std::vector<std::string> args)
{
    s->exit();
}
DECLCMD(exit, "Exit", "");

void
cmd_echo(IShell *s, std::vector<std::string> args)
{
    std::string val;

    for (auto &v : args) {
        val += v + " ";
    }

    s->writeLine(val);    
}
DECLCMD(echo, "Echo", "");

void
cmd_help(IShell *s, std::vector<std::string> args)
{
    s->writeLine("Available commands:");
    for (auto &c : *cmds) {
        stringstream str;

        str << std::setiosflags(std::ios::left) << std::setw(20);
        str << c.first << c.second->desc;
        s->writeLine(str.str());
    }
}
DECLCMD(help, "Print this message", "");

void
cmd_refresh(IShell *s, std::vector<std::string> args)
{
    s->refresh();
}
DECLCMD(refresh, "Refresh screen", "");

void
cmd_exec(IShell *s, std::vector<std::string> args)
{
    fstream script;
    std::string line;

    if (args.size() != 2) {
        s->writeLine("Usage: exec SCRIPT");
        return;
    }

    script = fstream(args[1], ios_base::in);
    while (getline(script, line)) {
        if (!s->exec(line)) {
            s->writeLine("Encountered an error while executing the script");
            return;
        }
    }
}
DECLCMD(exec, "Execute a shell script", "");

Command::Command(const std::string &name,
                 void (*func)(IShell *s, std::vector<std::string> args),
                 const std::string &desc,
                 const std::string &help)
    : name(name), func(func), desc(desc), help(help)
{
    LOG("Registering command: %s", name.c_str());
    Registry_Init();
    (*cmds)[name] = this;
}

Command::~Command()
{
}

