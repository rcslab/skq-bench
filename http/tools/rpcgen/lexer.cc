
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <memory>

#include "lexer.h"

using namespace std;

static std::map<std::string, Tokens> tokenMap = {
    { "service", T_SERVICE },
    { "struct", T_STRUCT },
    { "enum", T_ENUM },
    { "version", T_VERSION },
    { "direction", T_DIRECTION },
    { "oneway", T_ONEWAY },
    { "async", T_ASYNC },
    { "client", T_CLIENT },
    { "server", T_SERVER },
    { "both", T_BOTH },
    { "void", T_VOID },
    { "bool", T_BOOL },
    { "int8", T_INT8 },
    { "int16", T_INT16 },
    { "int32", T_INT32 },
    { "int64", T_INT64 },
    { "uint8", T_UINT8 },
    { "uint16", T_UINT16 },
    { "uint32", T_UINT32 },
    { "uint64", T_UINT64 },
    { "string", T_STRING },
    { "array", T_ARRAY },
    { "map", T_MAP }
};

void
Token::print() {
    const char *tokens[] = {
	"T_EOF",
	// Type
	"T_SERVICE",
	"T_STRUCT",
	"T_ENUM",
	"T_VERSION",
	// Modifiers
	"T_DIRECTION",
	"T_ONEWAY",
	"T_ASYNC",
	"T_CLIENT",
	"T_SERVER",
	"T_BOTH",
	// Literals
	"T_INTEGER",
	"T_IDENTIFIER",
	// Basic Types
	"T_VOID",
	"T_BOOL",
	"T_INT8",
	"T_INT16",
	"T_INT32",
	"T_INT64",
	"T_UINT8",
	"T_UINT16",
	"T_UINT32",
	"T_UINT64",
	"T_STRING",
	"T_ARRAY",
	"T_MAP",
	// Syntax
	"T_OPENCURLY",
	"T_CLOSECURLY",
	"T_OPENSQUARE",
	"T_CLOSESQUARE",
	"T_OPENARROW",
	"T_CLOSEARROW",
	"T_OPENPAREN",
	"T_CLOSEPAREN",
	"T_SEMICOLON",
	"T_COMMA",
	"T_SET",
    };

    cout << tokens[tok] << " " << line << " " << str << endl;
}

CRPCLexer::CRPCLexer()
    : input(), it(), line()
{
}

CRPCLexer::~CRPCLexer()
{
}

void
CRPCLexer::open(const string &path)
{
    fstream fd;
    stringstream sstr;

    fd.open(path, ios_base::in | ios_base::out);
    sstr << fd.rdbuf();
    input = sstr.str();
    fd.close();

    it = input.begin();
    line = 0;
}

void
CRPCLexer::close()
{
}

Tokens
CRPCLexer::peekToken()
{
    string::iterator orig = it;
    Token tok = getToken();

    it = orig;

    return tok.tok;
}

Token
CRPCLexer::getToken()
{
again:
    if (it == input.end()) {
	return Token(T_EOF, "", line);
    }
    switch (*it) {
	case ' ': case '\t': {
	    it++;
	    goto again;
	}
	case '/': {
	    it++;
	    if (*it != '/') {
		cout << "Unknown token!" << endl;
	    }
	    while ((*it != '\n') && (it != input.end())) {
		it++;
	    }
	    goto again;
	}
	case '\n': {
	    line++;
	    it++;
	    goto again;
	}
	case '=': {
	    it++;
	    return Token(T_SET, "=", line);
	}
	case '{': {
	    it++;
	    return Token(T_OPENCURLY, "{", line);
	}
	case '}': {
	    it++;
	    return Token(T_CLOSECURLY, "}", line);
	}
	case '[': {
	    it++;
	    return Token(T_OPENSQUARE, "[", line);
	}
	case ']': {
	    it++;
	    return Token(T_CLOSESQUARE, "]", line);
	}
	case '<': {
	    it++;
	    return Token(T_OPENARROW, "<", line);
	}
	case '>': {
	    it++;
	    return Token(T_CLOSEARROW, ">", line);
	}
	case '(': {
	    it++;
	    return Token(T_OPENPAREN, "(", line);
	}
	case ')': {
	    it++;
	    return Token(T_CLOSEPAREN, ")", line);
	}
	case ',': {
	    it++;
	    return Token(T_COMMA, ",", line);
	}
	case ';': {
	    it++;
	    return Token(T_SEMICOLON, ";", line);
	}
	default: {
	    string::iterator start = it;
	    while (((*it >= 'a' && *it <= 'z') ||
		    (*it >= 'A' && *it <= 'Z') ||
		    (*it >= '0' && *it <= '9') ||
		    (*it == '_'))) {
		it++;
	    }

	    string str(start, it);

	    if (str[0] >= '0' && str[0] <= '9') {
		return Token(T_INTEGER, str, line);
	    }

	    map<string,Tokens>::iterator mit = tokenMap.find(str);
	    if (mit == tokenMap.end()) {
		return Token(T_IDENTIFIER, str, line);
	    }

	    return Token(mit->second, str, line);
	}
    }
}

