
#ifndef __CELESTIS_TERM_TERMUI_H__
#define __CELESTIS_TERM_TERMUI_H__

class TermUI
{
public:
    TermUI(int fd);
    ~TermUI();
    void init();
    void setTitle(const std::string &str);
    void setStatus(const std::string &str);
    void writeLine(const std::string &str);
    std::string readLine();
private:
    Terminal term;
    std::vector<std::string> log;
    std::vector<std::string> history;
};

#endif

