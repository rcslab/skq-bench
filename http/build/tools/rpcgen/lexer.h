
enum Tokens {
    T_EOF,
    // Type Keywords
    T_SERVICE,
    T_STRUCT,
    T_ENUM,
    T_VERSION,
    // Modifiers
    T_DIRECTION,
    T_ONEWAY,
    T_ASYNC,
    T_CLIENT,
    T_SERVER,
    T_BOTH,
    // Literal
    T_INTEGER,
    T_IDENTIFIER,
    // Basic Types
    T_VOID,
    T_BOOL,
    T_INT8,
    T_INT16,
    T_INT32,
    T_INT64,
    T_UINT8,
    T_UINT16,
    T_UINT32,
    T_UINT64,
    T_STRING,
    T_ARRAY,
    T_MAP,
    // Syntax
    T_OPENCURLY,
    T_CLOSECURLY,
    T_OPENSQUARE,
    T_CLOSESQUARE,
    T_OPENARROW,
    T_CLOSEARROW,
    T_OPENPAREN,
    T_CLOSEPAREN,
    T_SEMICOLON,
    T_COMMA,
    T_SET,
};

struct Token {
    Token() : tok(T_EOF), str(""), line(-1) { }
    Token(Tokens tok, std::string str, int line)
	: tok(tok), str(str), line(line) { }
    void print();
    Tokens		tok;
    std::string		str;
    int			line;
};

class CRPCLexer {
public:
    CRPCLexer();
    ~CRPCLexer();
    void open(const std::string &path);
    void close();
    Tokens peekToken();
    Token getToken();
private:
    std::string input;
    std::string::iterator it;
    int line;
};

