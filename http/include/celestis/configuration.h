
#ifndef __CELESTIS_CONFIGURATION_H__
#define __CELESTIS_CONFIGURATION_H__

#include <string>
#include <sstream>

#include <celestis/debug.h>

enum ConfigVariableType
{
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_NUMBER,
};

class Configuration;

class ConfigVariable
{
public:
    ConfigVariable(ConfigVariableType type);
    ConfigVariable(const std::string &name, ConfigVariableType type);
    ConfigVariableType getType();
    virtual std::string getString() = 0;
    virtual bool setString(const std::string &str) = 0;
protected:
    ConfigVariableType type;
    friend class Configuration;
};

template<typename _T>
class ConfigVariableT : public ConfigVariable
{
public:
    ConfigVariableT(_T init,
                    _T min = std::numeric_limits<_T>::min(),
                    _T max = std::numeric_limits<_T>::max())
        : ConfigVariable(CONFIG_TYPE_NUMBER), value(init), min(min), max(max)
    {
    }
    ConfigVariableT(const std::string &name,
                    _T init,
                    _T min = std::numeric_limits<_T>::min(),
                    _T max = std::numeric_limits<_T>::max())
        : ConfigVariable(name, CONFIG_TYPE_NUMBER), value(init), min(min), max(max)
    {
    }
    ~ConfigVariableT() {
    }
    _T get() {
        return value;
    }
    bool set(_T val) {
        if (val >= min && val <= max) {
            value = val;
            return true;
        }
        return false;
    }
    virtual std::string getString()
    {
        return std::to_string(value);
    }
    virtual bool setString(const std::string &str)
    {
        _T val;
        std::stringstream ss(str);

        ss >> val;
        if (val >= min && val <= max) {
            value = val;
            return true;
        }

        return false;
    }
private:
    _T value;
    _T min;
    _T max;
};

template<>
class ConfigVariableT<std::string> : public ConfigVariable
{
public:
    ConfigVariableT(const std::string &init)
        : ConfigVariable(CONFIG_TYPE_STRING), value(init)
    {
    }
    ConfigVariableT(const std::string &name,
                    const std::string &init)
        : ConfigVariable(name, CONFIG_TYPE_STRING), value(init)
    {
    }
    ~ConfigVariableT() {
    }
    std::string get() {
        return value;
    }
    bool set(const std::string &val) {
        value = val;
        return true;
    }
    virtual std::string getString()
    {
        return value;
    }
    virtual bool setString(const std::string &str)
    {
        value = str;
        return true;
    }
private:
    std::string value;
};

template<>
class ConfigVariableT<bool> : public ConfigVariable
{
public:
    ConfigVariableT(bool init)
        : ConfigVariable(CONFIG_TYPE_STRING), value(init)
    {
    }
    ConfigVariableT(const std::string &name,
                    bool init)
        : ConfigVariable(name, CONFIG_TYPE_STRING), value(init)
    {
    }
    ~ConfigVariableT() {
    }
    bool get() {
        return value;
    }
    bool set(bool val) {
        value = val;
        return true;
    }
    virtual std::string getString()
    {
        return (value ? "TRUE" : "FALSE");
    }
    virtual bool setString(const std::string &str)
    {
        if (str == "TRUE" || str == "1") {
            value = true;
        } else if (str == "FALSE" || str == "0") {
            value = false;
        } else {
            MSG("Attempting to set a bool '%s' to an unrecognized value.",
                str.c_str());
            return false;
        }
        return true;
    }
private:
    bool value;
};

typedef ConfigVariableT<int64_t> CVInteger;
typedef ConfigVariableT<double> CVFloat;
typedef ConfigVariableT<bool> CVBool;
typedef ConfigVariableT<std::string> CVString;

class Configuration
{
public:
    Configuration();
    void add(const std::string &name, ConfigVariable *cv);
    void remove(const std::string &name);
    ConfigVariable *get(const std::string &name);
    std::unordered_map<std::string, ConfigVariable*>::iterator begin();
    std::unordered_map<std::string, ConfigVariable*>::iterator end();
private:
    std::unordered_map<std::string, ConfigVariable*> conf;
};

extern Configuration *globalConfig;

#endif /* __CELESTIS_CONFIGURATION_H__ */

