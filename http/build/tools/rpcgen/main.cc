
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "lexer.h"
#include "parser.h"
#include "cppgen.h"

using namespace std;

int
process(const char *file)
{
    CRPCParser parse;

    /*
    CRPCLexer lex;
    lex.open(file);
    while (1) {
	Token tok = lex.getToken();
	tok.print();
	if (tok.tok == T_EOF)
	    break;
    }
    */

    parse.open(file);
    unique_ptr<FileNode> node = parse.parse();

    cout << "**********" << endl;
    cout << "Node Graph" << endl;
    cout << "**********" << endl;
    node->print();
    cout << endl;

    cout << "***************" << endl;
    cout << "Code Generation" << endl;
    cout << "***************" << endl;
    CPPGen gen;
    gen.compile(std::move(node));

    return 0;
}

int
main(int argc, const char *argv[])
{
    if (argc != 2) {
	cout << "Usage: crpcgen [INPUT]" << endl;
	return 1;
    }

    return process(argv[1]);
}

