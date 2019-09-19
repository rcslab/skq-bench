
#include <string>
#include <vector>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/terminal.h>
#include <celestis/termui.h>

using namespace std;

TermUI::TermUI(int fd)
    : term(fd)
{
}

TermUI::~TermUI()
{
}

void
TermUI::init()
{
    term.init();
    term.clearScreen();
    term.writeHR(0);
    term.writeHR(term.getHeight() - 1);
    term.writeLine(term.getHeight(), "> ");
}

void
TermUI::setTitle(const std::string &str)
{
    term.setTitle(str);

    // Set title
    term.setColorInverted();
    term.writeLine(0, str);
    term.setColorNormal();
}

void
TermUI::setStatus(const std::string &str)
{
    term.setColorInverted();
    term.writeLine(term.getHeight() - 1, str);
    term.setColorNormal();
}

void
TermUI::writeLine(const std::string &str)
{
    log.insert(log.begin(), str);

    // XXX: Use VT220 scroll if possible

    int line = term.getHeight() - 2;
    for (auto& m : log) {
        term.writeLine(line, m);
        line--;
        if (line == 1)
            break;
    }
}

std::string
TermUI::readLine()
{
    int key;
    int cursor = 1;
    string buf = "";
    string savedbuf;
    int hcursor = 0;

    term.writeLine(term.getHeight(), "> ");

    while ((key = term.readKey()) != '\r') {
        switch (key) {
        case '\b':
        case '\x7F':
            if (cursor > 1) {
                buf.erase(cursor - 2, 1);
                cursor--;
            }
            break;
        case '\t':
            break;
        case KEYCODE_UP:
            if (hcursor < history.size()) {
                hcursor++;
                if (hcursor == 1) {
                    savedbuf = buf;
                }
                buf = history[hcursor - 1];
                cursor = buf.length() + 1;
            }
            break;
        case KEYCODE_DOWN:
            if (hcursor > 0) {
                hcursor--;
                if (hcursor != 0) {
                    buf = history[hcursor - 1];
                } else {
                    buf = savedbuf;
                }
                cursor = buf.length() + 1;
            }
            break;
        case KEYCODE_RIGHT:
            if (cursor <= buf.length()) {
                cursor++;
            }
            break;
        case KEYCODE_LEFT:
            if (cursor > 1) {
                cursor--;
            }
            break;
        default:
            char c = key;
            buf.insert(cursor - 1, &c, 1);
            cursor++;
            break;
        }
        term.writeLine(term.getHeight(), "> " + buf);
        term.setCursor(term.getHeight(), 2 + cursor);
    }

    history.insert(history.begin(), buf);

    return buf;
}

