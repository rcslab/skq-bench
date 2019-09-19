
#include <iostream>
#include <string>
#include <unordered_map>

#include <celestis/debug.h>
#include <celestis/configuration.h>

#include "test.h"

using namespace std;

ConfigVariableT<std::string> testString = ConfigVariableT<string>("test.string", "Default String");
ConfigVariableT<int> testInteger = ConfigVariableT<int>("test.integer", 1);
ConfigVariableT<float> testFloat = ConfigVariableT<float>("test.float", 1.0);

int
main(int argc, const char *argv[])
{
    ConfigVariableT<string> *ts;
    ConfigVariableT<int> *ti;
    ConfigVariableT<float> *tf;

    // Test string object
    TEST_ASSERT(testString.get() == "Default String");
    ts = (ConfigVariableT<string>*)(globalConfig->get("test.string"));
    TEST_ASSERT(ts->get() == "Default String");
    testString.set("Foo");
    TEST_ASSERT(ts->get() == "Foo");

    // Test integer
    testInteger.set(5);
    ti = (ConfigVariableT<int>*)(globalConfig->get("test.integer"));
    TEST_ASSERT(ti->get() == 5);

    // Test float
    tf = (ConfigVariableT<float>*)(globalConfig->get("test.float"));
    tf->set(3.14);
    TEST_ASSERT(testFloat.get() < 4 || testFloat.get() > 3);

    for (auto &t : *globalConfig) {
        cout << "Config: " << t.first << endl;
    }
}

