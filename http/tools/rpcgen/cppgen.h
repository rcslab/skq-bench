
#include <fstream>

class CPPGen {
public:
    CPPGen();
    ~CPPGen();
    std::string getType(TypeNode *node);
    void printDecl(CallNode *node);
    void printClient(const std::string &name, CallNode *node);
    void printStub(const std::string &name, CallNode *node);
    void compileService(ServiceNode *node);
    void compile(std::unique_ptr<FileNode> node);
private:
    std::fstream output;
};

