
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <iomanip>

#include <celestis/debug.h>
#include <celestis/command.h>
#include <celestis/terminal.h>
#include <celestis/termui.h>
#include <celestis/shell.h>

using namespace std;

extern std::unordered_map<std::string, Command*> *cmds;

Shell::Shell(TermUI &term)
    : title(), status(), term(term), done(false)
{
    Debug_AddOutput(this, [this](const std::string &l){ this->writeLine(l); });
}

Shell::~Shell()
{
    Debug_RemoveOutput(this);
}

void
Shell::process()
{
    while (!done) {
        std::string l = term.readLine();
        std::vector<std::string> p = parse(l);

        if (p.size() == 0) {
            continue;
        }

        term.writeLine("> " + l);

        // Find Command
        auto it = cmds->find(p[0]);
        if (it == cmds->end()) {
            term.writeLine("Unknown command '" + p[0] + "'");
            continue;
        }

        LOG("Running \"%s\"", l.c_str());
        it->second->func(this, p);
    }
}

void
Shell::setTitle(const std::string &str)
{
    title = str;
    term.setTitle(str);
}

void
Shell::setStatus(const std::string &str)
{
    status = str;
    term.setStatus(str);
}

void
Shell::writeLine(const std::string &str)
{
    buffer.push_back(str);
    term.writeLine(str);
}

bool
Shell::exec(const std::string &str)
{
    std::vector<std::string> p = parse(str);

    if (p.size() == 0) {
        return true;
    }

    // Ignore comments
    if (p[0][0] == '#') {
        return true;
    }

    term.writeLine("> " + str);

    // Find Command
    auto it = cmds->find(p[0]);
    if (it == cmds->end()) {
        term.writeLine("Unknown command '" + p[0] + "'");
        return false;
    }

    LOG("Running \"%s\"", str.c_str());
    it->second->func(this, p);

    return true;
}

void
Shell::exit()
{
    done = true;
}

void
Shell::refresh()
{
    term.setTitle(title);
    term.setStatus(status);
}

std::string
Shell::applyEnv(const std::string &str)
{
    std::string rval;

    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '$') {
        } else {
            //rval.append(str[i]);
        }
    }

    return rval;
}

std::vector<std::string>
Shell::parse(const std::string &str)
{
    int start = -1;
    bool inString = false;
    std::vector<std::string> rval;

    for (size_t i = 0; i < str.length(); i++) {
        // Start string with first non-space
        if (start == -1) {
            if (str[i] == '"') {
                inString = true;
                start = i + 1;
                continue;
            } else if (str[i] != ' ') {
                start = i;
                continue;
            } else {
                continue;
            }
        }

        // Ignore escaped characters
        if (str[i] == '\\') {
            i++;
            continue;
        }

        if (inString) {
            if (str[i] == '"') {
                rval.push_back(str.substr(start, i - start));
                inString = false;
                start = -1;
            }
        } else {
            if (str[i] == '"') {
                rval.push_back(str.substr(start, i - start));
                start = i + 1;
                inString = true;
                continue;
            }
            if (str[i] == ' ') {
                rval.push_back(str.substr(start, i - start));
                start = -1;
            }
        }
    }
    if (start != -1) {
        rval.push_back(str.substr(start, str.length() - start));
    }

    return rval;
}

