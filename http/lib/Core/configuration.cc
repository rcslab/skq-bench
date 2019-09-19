
#include <string>
#include <vector>
#include <unordered_map>

#include <celestis/debug.h>
#include <celestis/command.h>
#include <celestis/configuration.h>

extern void Registry_Init();

ConfigVariable::ConfigVariable(ConfigVariableType type)
    : type(type)
{
}

ConfigVariable::ConfigVariable(const std::string &name, ConfigVariableType type)
    : type(type)
{
    Registry_Init();
    LOG("Registering configuration variable: %s", name.c_str());
    globalConfig->add(name, this);
}

ConfigVariableType
ConfigVariable::getType()
{
    return type;
}

Configuration::Configuration()
{
}

void
Configuration::add(const std::string &name, ConfigVariable *cv)
{
    conf.emplace(name, cv);
}

void
Configuration::remove(const std::string &name)
{
    conf.erase(name);
}

ConfigVariable*
Configuration::get(const std::string &name)
{
    return conf[name];
}

std::unordered_map<std::string, ConfigVariable*>::iterator
Configuration::begin()
{
    return conf.begin();
}

std::unordered_map<std::string, ConfigVariable*>::iterator
Configuration::end()
{
    return conf.end();
}


void
cmd_get(IShell *s, std::vector<std::string> args)
{
    if (args.size() != 2) {
        s->writeLine("Specify a configuration variable");
        s->writeLine("Usage: get CONFIG_VARIABLE");
        return;
    }

    ConfigVariable *cv = globalConfig->get(args[1]);
    if (cv == nullptr) {
        s->writeLine("Variable name invalid");
        return;
    }

    s->writeLine(cv->getString());
}
DECLCMD(get, "Get a configuration parameter", "");

void
cmd_set(IShell *s, std::vector<std::string> args)
{
    if (args.size() != 3) {
        s->writeLine("Specify a configuration variable");
        s->writeLine("Usage: set CONFIG_VARIABLE VALUE");
        return;
    }

    ConfigVariable *cv = globalConfig->get(args[1]);
    if (cv == nullptr) {
        s->writeLine("Variable name invalid");
        return;
    }

    cv->setString(args[2]);
}
DECLCMD(set, "Get a configuration parameter", "");

void
cmd_config(IShell *s, std::vector<std::string> args)
{
    s->writeLine("Configuration parameters");
    for (auto &c : *globalConfig) {
        if (c.second->getType() == CONFIG_TYPE_STRING) {
            s->writeLine(c.first + " = \"" + c.second->getString() + "\"");
        } else {
            s->writeLine(c.first + " = " + c.second->getString());
        }
    }
}
DECLCMD(config, "List configuration parameters", "");

