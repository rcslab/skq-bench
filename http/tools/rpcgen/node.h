
#define CALLFLAG_SERVER		1
#define CALLFLAG_CLIENT		2
#define CALLFLAG_ONEWAY		4
#define CALLFLAG_ASYNC		8

struct Node {
    Node() {}
    virtual ~Node() {}
    virtual void print(int depth = 0);
    Tokens			token;
};

struct TypeNode : Node {
    std::string			type;
    int				dimensions;
};

struct EnumValue : Node {
    std::string			name;
    int64_t			value;
};

struct EnumNode : Node {
    std::string			name;
    std::vector<EnumValue*>	values;
};

struct FieldNode : Node {
    FieldNode() {}
    virtual ~FieldNode() {}
    virtual void print(int depth = 0);
    std::unique_ptr<TypeNode>	type;
    std::string			name;
    int64_t			value;
};

struct CallNode : Node {
    CallNode() {}
    virtual ~CallNode() {}
    virtual void print(int depth = 0);
    int				flags;
    std::string			name;
    std::unique_ptr<TypeNode>	retType;
    std::vector<std::unique_ptr<TypeNode>>	paramType;
    std::vector<std::string>	paramName;
};

struct ServiceNode : Node {
    ServiceNode() {}
    virtual ~ServiceNode() {}
    virtual void print(int depth = 0);
    std::string			name;
    int64_t			value;
    std::vector<std::unique_ptr<CallNode>>	calls;
};

struct StructNode : Node {
    StructNode() {}
    virtual ~StructNode() {}
    virtual void print(int depth = 0);
    std::string			name;
    int64_t			value;
    std::vector<std::unique_ptr<FieldNode>>	fields;
};

struct FileNode : Node {
    FileNode() {}
    virtual ~FileNode() {}
    virtual void print(int depth = 0);
    std::vector<std::unique_ptr<Node>>	nodes;
};

