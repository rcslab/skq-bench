
#ifndef __CELESTIS_COMMAND_H__
#define __CELESTIS_COMMAND_H__

class IShell
{
public:
    virtual void setTitle(const std::string &str) = 0;
    virtual void setStatus(const std::string &str) = 0;
    virtual void writeLine(const std::string &str) = 0;
    virtual bool exec(const std::string &str) = 0;
    virtual void exit() = 0;
    virtual void refresh() = 0;
protected:
    IShell() {}
};

class Command
{
public:
    Command(const std::string &name,
            void (*func)(IShell *s, std::vector<std::string> args),
            const std::string &desc,
            const std::string &help);
    ~Command();
    std::string name;
    void (*func)(IShell *s, std::vector<std::string> args);
    std::string desc;
    std::string help;
};

#define DECLCMD(_name, _desc, _help) \
    Command CMD_##_name = Command(#_name, cmd_##_name, _desc, _help)

#endif

