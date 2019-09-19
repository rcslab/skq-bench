
#include <string>
#include <memory>

namespace Celestis { namespace RPC {

class Message {
public:
    enum Types {
	MSG_TYPE_UINT8 = 0x10,
	MSG_TYPE_UINT16 = 0x11,
	MSG_TYPE_UINT32 = 0x12,
	MSG_TYPE_UINT64 = 0x13,
	MSG_TYPE_INT8 = 0x18,
	MSG_TYPE_INT16 = 0x19,
	MSG_TYPE_INT32 = 0x1A,
	MSG_TYPE_INT64 = 0x1B,
	MSG_TYPE_STR = 0x20,
	MSG_TYPE_BIN = 0x21,
    };
    const static int HEADER_SIZE = 8;
    const static int DEFAULT_SIZE = 1024;
    // Constructor
    Message();
    ~Message();
    void clear();
    void reserve(size_t size = 1024);
    uint32_t prepareBuffer();
    uint8_t *data() const;
    uint32_t size() const;
    void seal();
    void unseal();
    std::string dump() const;
    // Appending Data
    void appendU8(uint8_t d);
    void appendU16(uint16_t d);
    void appendU32(uint32_t d);
    void appendU64(uint64_t d);
    void appendS8(int8_t d);
    void appendS16(int16_t d);
    void appendS32(int32_t d);
    void appendS64(int64_t d);
    void appendStr(const std::string &str);
    void appendBin(const std::string &bin);
    // Reading Data
    uint8_t readU8();
    uint16_t readU16();
    uint32_t readU32();
    uint64_t readU64();
    int8_t readS8();
    int16_t readS16();
    int32_t readS32();
    int64_t readS64();
    std::string readStr();
    std::string readBin();
    Types peekType();
    uint32_t peekSize();
private:
    void resize(size_t size = 1024);
    std::unique_ptr<uint8_t[]> buf;
    uint32_t bufSz;
    uint32_t msgSz;
    uint32_t off;
};

}; };

