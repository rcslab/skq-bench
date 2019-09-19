
#include <unistd.h>

#include <string>
#include <vector>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/terminal.h>
#include <celestis/termui.h>

using namespace std;

int
main(int argc, const char *argv[])
{
    string str;
    TermUI t = TermUI(0);

    t.init();
    t.setTitle("Test Console UI");
    t.setStatus("Status Bar");
    while ((str = t.readLine()) != "exit") {
        t.writeLine(str);
    }
}

