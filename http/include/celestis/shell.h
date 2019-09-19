
#ifndef __CELESTIS_SHELL_H__
#define __CELESTIS_SHELL_H__

#include <string>
#include <vector>
#include <list>

class Shell : public IShell
{
public:
    Shell(TermUI &term);
    ~Shell();
    void process();
    virtual void setTitle(const std::string &str);
    virtual void setStatus(const std::string &str);
    virtual void writeLine(const std::string &str);
    virtual bool exec(const std::string &str);
    virtual void exit();
    virtual void refresh();
private:
    std::string applyEnv(const std::string &str);
    std::vector<std::string> parse(const std::string &str);
    std::string title;
    std::string status;
    std::list<std::string> buffer;
    TermUI &term;
    bool done;
};

#endif

