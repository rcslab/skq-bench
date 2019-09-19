
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/configuration.h>
#include <celestis/terminal.h>

using namespace std;

CVString bgTextColor = CVString("shell.bgtextcolor", "47");
CVString fgTextColor = CVString("shell.fgtextcolor", "30");
CVString bgTitleColor = CVString("shell.bgtitlecolor", "44");
CVString fgTitleColor = CVString("shell.fgtitlecolor", "37");
CVString bgStatusColor = CVString("shell.bgstatuscolor", "45");
CVString fgStatusColor = CVString("shell.fgstatuscolor", "37");

Terminal::Terminal(int fd)
    : fd(fd)
{
}

Terminal::~Terminal()
{
}

void
Terminal::init()
{
    int status;
    std::string resp;

    if (isatty(fd)) {
        struct winsize ws;
        struct termios t;
        char ttypath[64];

        status = ioctl(fd, TIOCGWINSZ, &ws);
        if (status == 0) {
            height = ws.ws_row;
            width = ws.ws_col;
        } else {
            // VT100 defaults
            height = 24;
            width = 80;
        }

        tcgetattr(fd, &t);
        cfmakeraw(&t);
        tcsetattr(fd, TCSAFLUSH, &t);

        status = ttyname_r(fd, &ttypath[0], 64);
        if (status != 0) {
            term = "unknown";
            return;
        }
        if(strncmp(&ttypath[0], "/dev/pts/", 9) != 0) {
            // Probably a console
            term = "console";
            return;
        }
    }

    // Application cursor keys (DECCKM)
    writeRequest("\033[?1h");
    // Application keypad (DECKPAM)
    writeRequest("\033=");

    // Hide cursor (DECTCEM)
    writeRequest("\033[?25l");
    writeRequest("\033[1;1H"); // Goto Origin
    // Show cursor (DECTCEM)
    writeRequest("\033[?25h");

    // Send Device Atrtibutes
    writeRequest("\033[c");
    resp = readResponse('c');
    std::vector<std::string> presp = parseResponse(resp.substr(3,resp.length()-2));
    switch (stoi(presp[0])) {
        case 1:
            term = "VT100";
            break;
        case 6:
            term = "VT102";
            break;
        case 62:
            term = "VT220";
            break;
        case 63:
            term = "VT320";
            break;
        case 64:
            term = "VT420";
            break;
    }

    if (term == "VT100" || term == "VT102") {
        return;
    }

    // Send Secondary Device Attributes (VT220 and up)
    writeRequest("\033[>0c");
    resp = readResponse('c');
    presp = parseResponse(resp.substr(3,resp.length()-2));

    switch (stoi(presp[0])) {
        case 0:
            term = "VT100";
            break;
        case 1:
            term = "VT220";
            break;
        case 2:
            term = "VT240";
            break;
        case 18:
            term = "VT330";
            break;
        case 19:
            term = "VT340";
            break;
        case 24:
            term = "VT320";
            break;
        case 41:
            term = "VT420";
            break;
        case 61:
            term = "VT510";
            break;
        case 64:
            term = "VT520";
            break;
        case 65:
            term = "VT525";
            break;
    }
    if (stoi(presp[1]) >= 95) {
        // Most likely an xterm
        term = "xterm";
    }

    // Only valid for xterm
    writeRequest("\033[18t");
    resp = readResponse('t');
    presp = parseResponse(resp.substr(2,resp.length()-2));
    height = stoi(presp[1]);
    width = stoi(presp[2]);
}

int
Terminal::getHeight()
{
    return height;
}

int
Terminal::getWidth()
{
    return width;
}

void
Terminal::showConfig()
{
    cout << "Terminal Type: " << term << "\n\r";
    cout << "Height x Width: " << height << "x" << width << "\n\r";
}

void
Terminal::setTitle(const std::string &str)
{
    std::string cmd = "\033]2;";

    cmd += "\033[" + bgTitleColor.get() + "m\033[" + fgTitleColor.get() + "m";
    cmd += str + "\033\\";

    writeRequest(cmd);
}

void
Terminal::clearScreen()
{
    writeRequest("\033[?25l");
    writeRequest("\033[1;1H"); // Goto Origin
    setColorNormal();
    for (int i = 0; i < height; i++) {
        writeRequest("\033[K\n\r");
    }
    writeRequest("\033[?25h");
}

void
Terminal::setNormal()
{
    writeRequest("\033[0m");
}

void
Terminal::setBold()
{
    writeRequest("\033[1m");
}

void
Terminal::setColorNormal()
{
    writeRequest("\033[" + bgTextColor.get() + "m\033[" + fgTextColor.get() + "m");
}

void
Terminal::setColorInverted()
{
    writeRequest("\033[" + bgTitleColor.get() + "m\033[" + fgTitleColor.get() + "m");
}

void
Terminal::writeHR(int row)
{
    string str;

    str.append(width, ' ');

    writeRequest("\033[?25l");
    setColorInverted();
    writeRequest("\033[" + to_string(row) + ";1H");
    writeRequest("\033[K" + str);
    setColorNormal();
    writeRequest("\033[?25h");
}

void
Terminal::writeLine(int row, const std::string &str)
{
    writeRequest("\033[?25l");
    writeRequest("\033[" + to_string(row) + ";1H");
    writeRequest("\033[K" + str);
    writeRequest("\033[?25h");
}

void
Terminal::writeRequest(const std::string &str)
{
    int status = ::write(fd, str.c_str(), str.size());
    if (status == -1) {
        throw exception();
    }
}

void
Terminal::setCursor(int row, int col)
{
    writeRequest("\033[" + to_string(row) + ";" + to_string(col) + "H");
}

/*
 * Reading Keys:
 *
 * Encodings:
 * SS2: ESC-N <char>
 * SS3: ESC-O <char>
 * CSI: ESC-[ <number> ~
 *
 * Key              Esc     Value
 * ------------------------------
 * Cursor Up:       SS3     A
 * Cursor Down:     SS3     B
 * Cursor Right:    SS3     C
 * Cursor Left:     SS3     D
 * Home:            SS3     H
 * End:             SS3     F
 * F1:              SS3     P       (CSI 11)
 * F2:              SS3     Q       (CSI 12)
 * F3:              SS3     R       (CSI 13)
 * F4:              SS3     S       (CSI 14)
 * F5:              CSI     15
 * F6:              CSI     17
 * F7:              CSI     18
 * F8:              CSI     19
 * F9:              CSI     20
 * F10:             CSI     21
 * F11:             CSI     23
 * F12:             CSI     24
 *
 */

char
readByte(int fd)
{
    int status;
    char c;

    do {
        status = ::read(fd, &c, 1);
        if (status == 1) {
            return c;
        }
        if (status < 0) {
            throw exception();
        }
    } while (status == 0);

    NOT_REACHED();
}

std::unordered_map<char, int> SSSG3 = {
    { 'A',      KEYCODE_UP },
    { 'B',      KEYCODE_DOWN },
    { 'C',      KEYCODE_RIGHT },
    { 'D',      KEYCODE_LEFT },
};

int
Terminal::readKey()
{
    char c;

    c = readByte(fd);
    if (c != '\033')
        return c;

    c = readByte(fd);
    if (c == 'O') {
        c = readByte(fd);
        return SSSG3[c];
    }

    return '?';
}

std::string
Terminal::readResponse(char terminator)
{
    int status;
    char c;
    string buf = "";

    do {
        status = ::read(fd, &c, 1);
        if (status == 1) {
            buf.append(&c, 1);
            if (c != '\033')
                return buf;
        }
        if (status < 0) {
            throw exception();
        }
    } while (status == 0);

    while (c != terminator) {
        status = ::read(fd, &c, 1);
        if (status == 1) {
            buf.append(&c, 1);
        }
        if (status < 0) {
            throw exception();
        }
    }

    return buf;
}

std::vector<std::string>
Terminal::parseResponse(const std::string &str)
{
    size_t start = 0;
    std::vector<std::string> rval;

    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == ';') {
            rval.push_back(str.substr(start, i - start));
            start = i + 1;
        }
    }
    rval.push_back(str.substr(start, str.length() - start - 1));

    return rval;
}

