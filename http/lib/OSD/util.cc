#include <iostream>

#include <celestis/OSD/util.h>


std::string
tab_prepend(std::stringstream &s, int tab_spacing)
{
    std::string line;
    std::string fin = "";
    std::string spacing = "";
    for(int i = 0; i < tab_spacing; i++) {
        spacing += '\t';
    }
    while(getline(s, line)) {
        fin += spacing + line + '\n';
    }

    return fin;
}

