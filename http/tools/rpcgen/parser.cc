
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "lexer.h"
#include "parser.h"

using namespace std;

CRPCParser::CRPCParser()
{
}

CRPCParser::~CRPCParser()
{
}

void
CRPCParser::open(const std::string &path)
{
    lex.open(path);
}

void
CRPCParser::close()
{
    lex.close();
}

void
CRPCParser::error(Token tok)
{
    cout << "Unexpected token on line " << tok.line << endl;
    cout << "Encountered: " << tok.str << endl;
}

void
CRPCParser::eat(Tokens tok)
{
    Token t = lex.getToken();

    if (t.tok != tok) {
	cout << "Unexpected token on line " << t.line << endl;
	cout << "Encountered: " << t.str << endl;
    }
}

unique_ptr<TypeNode>
CRPCParser::parseType()
{
    unique_ptr<TypeNode> node(new TypeNode);
    Token type = lex.getToken();

    switch (type.tok) {
	case T_VOID: {
	    node->token = type.tok;
	    node->type = type.str;
	    break;
	}
	case T_BOOL: case T_STRING:
	case T_INT32: case T_INT64:
	case T_UINT32: case T_UINT64:
	case T_IDENTIFIER: {
	    node->token = type.tok;
	    node->type = type.str;
	    // Parse array
	    // End with a comma, identifier, or close paren.
	    break;
	}
	case T_ARRAY:
	case T_MAP: {
	    error(type);
	    return nullptr;
	}
	default:
	    error(type);
	    return nullptr;
    }

    return node;
}

unique_ptr<FieldNode>
CRPCParser::parseField()
{
    unique_ptr<FieldNode> node(new FieldNode);

    node->type = parseType();

    Token name = lex.getToken();
    if (name.tok != T_IDENTIFIER) {
	error(name);
	return node;
    }
    node->name = name.str;

    eat(T_SEMICOLON);

    return node;
}

unique_ptr<StructNode>
CRPCParser::parseStruct()
{
    unique_ptr<StructNode> node(new StructNode);
    Token name = lex.getToken();

    if (name.tok != T_IDENTIFIER) {
	error(name);
	return node;
    }
    node->name = name.str;

    eat(T_OPENCURLY);

    while (lex.peekToken() != T_CLOSECURLY) {
	node->fields.push_back(parseField());
    }

    eat(T_CLOSECURLY);

    return node;
}

uint64_t
CRPCParser::parseFlags()
{
    uint64_t flags = 0;
    Token flag;

    eat(T_OPENSQUARE);

    do {
	flag = lex.getToken();

	switch (flag.tok) {
	    case T_DIRECTION: {
		Token dir;

		eat(T_OPENPAREN);
		dir = lex.getToken();
		if (dir.tok == T_CLIENT) {
		    flags |= CALLFLAG_CLIENT;
		} else if (dir.tok == T_SERVER) {
		    flags |= CALLFLAG_SERVER;
		} else if (dir.tok == T_BOTH) {
		    flags |= CALLFLAG_SERVER | CALLFLAG_CLIENT;
		} else {
		    error(dir);
		}
		eat(T_CLOSEPAREN);
		break;
	    }
	    case T_ONEWAY: {
		flags |= CALLFLAG_ONEWAY;
		break;
	    }
	    case T_ASYNC: {
		flags |= CALLFLAG_ASYNC;
		break;
	    }
	    default: {
		error(flag);
		return 0;
	    }
	}
    } while (lex.peekToken() != T_CLOSESQUARE);

    eat(T_CLOSESQUARE);

    return flags;
}

unique_ptr<CallNode>
CRPCParser::parseCall()
{
    unique_ptr<CallNode> node(new CallNode);
    Token name;

    if (lex.peekToken() == T_OPENSQUARE) {
	node->flags = parseFlags();
    } else {
	node->flags = CALLFLAG_SERVER;
    }

    node->retType = parseType();

    name = lex.getToken();
    if (name.tok != T_IDENTIFIER) {
	error(name);
	return nullptr;
    }
    node->name = name.str;

    eat(T_OPENPAREN);

    while (lex.peekToken() != T_CLOSEPAREN) {
	Token field;

	node->paramType.push_back(parseType());

	field = lex.getToken();
	if (field.tok != T_IDENTIFIER) {
	    error(field);
	    return nullptr;
	}

	node->paramName.push_back(field.str);

	if (lex.peekToken() == T_COMMA)
	    eat(T_COMMA);
    }

    eat(T_CLOSEPAREN);
    eat(T_SEMICOLON);

    return node;
}

unique_ptr<ServiceNode>
CRPCParser::parseService()
{
    unique_ptr<ServiceNode> node(new ServiceNode);
    Token name = lex.getToken();

    if (name.tok != T_IDENTIFIER) {
	error(name);
	return nullptr;
    }
    node->name = name.str;

    eat(T_OPENCURLY);

    while (lex.peekToken() != T_CLOSECURLY) {
	node->calls.push_back(parseCall());
    }

    eat(T_CLOSECURLY);

    return node;
}

unique_ptr<FileNode>
CRPCParser::parse()
{
    unique_ptr<FileNode> node(new FileNode);

    while (1) {
	Token tok = lex.getToken();

	if (tok.tok == T_EOF)
	    return node;

	switch (tok.tok) {
	    case T_SERVICE:
		node->nodes.push_back(parseService());
		break;
	    case T_STRUCT:
		node->nodes.push_back(parseStruct());
	    case T_ENUM:
		break;
	    case T_EOF: {
		return node;
	    }
	    default: {
		error(tok);
		return nullptr;
	    }
	}
    }
}

