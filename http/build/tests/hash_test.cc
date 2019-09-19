
#include <string>
#include <iostream>

#include <celestis/debug.h>
#include <celestis/hash.h>

using namespace std;

int
main(int argc, const char *argv[])
{
    Hash h;

    h = hash_data((const void *)"abc", 4);
    ASSERT (hash_to_string(h) == "b6874dcb3d30155729cce243b8121be1a9bf3d7135ec2c7f9e016fbcbe7a853");
}

