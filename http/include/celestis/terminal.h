
#ifndef __CELESTIS_TERM_TERMINAL_H__
#define __CELESTIS_TERM_TERMINAL_H__

enum KeyCode
{
    KEYCODE_UP = 257,
    KEYCODE_DOWN,
    KEYCODE_RIGHT,
    KEYCODE_LEFT,
};

class Terminal
{
public:
    Terminal(int fd);
    ~Terminal();
    void init();
    int getHeight();
    int getWidth();
    int getTerminal();
    void showConfig();
    void setTitle(const std::string &str);
    void setNormal();
    void setBold();
    void setColorNormal();
    void setColorInverted();
    void clearScreen();
    void writeHR(int row);
    void writeLine(int row, const std::string &str);
    void setCursor(int row, int col);
    int readKey();
private:
    void writeRequest(const std::string &str);
    std::string readResponse(char term);
    std::vector<std::string> parseResponse(const std::string &str);
    int fd;
    int height;
    int width;
    std::string term;
};

#endif

