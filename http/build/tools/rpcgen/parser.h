
#include "node.h"

class CRPCParser {
public:
    CRPCParser();
    ~CRPCParser();
    void open(const std::string &path);
    void close();
    void error(Token tok);
    void eat(Tokens tok);
    std::unique_ptr<TypeNode> parseType();
    std::unique_ptr<FieldNode> parseField();
    std::unique_ptr<StructNode> parseStruct();
    uint64_t parseFlags();
    std::unique_ptr<CallNode> parseCall();
    std::unique_ptr<ServiceNode> parseService();
    std::unique_ptr<FileNode> parse();
private:
    CRPCLexer lex;
};

