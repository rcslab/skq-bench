
#include <string>
#include <vector>
#include <memory>
#include <iomanip>
#include <iostream>

#include "lexer.h"
#include "node.h"

using namespace std;

void
Node::print(int depth)
{
    for (int i = 0; i < depth; i++)
	cout << " ";
    cout << token << endl;
}

void
FieldNode::print(int depth)
{
    cout << setw(depth) << setfill(' ') << "";
    cout << "FIELD: " << type->type << " " << name << endl;
}

void
StructNode::print(int depth)
{
    cout << setw(depth) << setfill(' ') << "";
    cout << "STRUCT: " << name << endl;
    for (auto &it : fields) {
	it->print(depth + 1);
    }
}

void
CallNode::print(int depth)
{
    cout << setw(depth) << setfill(' ') << "";
    cout << "CALL: " << name << endl;
    cout << setw(depth) << setfill(' ') << "";
    cout << " FLAGS: " << flags << endl;
    cout << setw(depth) << setfill(' ') << "";
    cout << " RETURN: " << retType->type << endl;
    for (auto &it : paramType) {
	cout << setw(depth) << setfill(' ') << "";
	cout << " ARG: " << it->type << endl;
    }
}

void
ServiceNode::print(int depth)
{
    cout << setw(depth) << setfill(' ') << "";
    cout << "SERVICE: " << name << endl;
    for (auto &it : calls) {
	it->print(depth + 1);
    }
}

void
FileNode::print(int depth)
{
    cout << setw(depth) << setfill(' ') << "";
    cout << "FILE:" << endl;
    for (auto &it : nodes) {
	it->print(depth + 1);
    }
}

