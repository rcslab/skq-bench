
#undef NDEBUG
#include <cassert>

#include <string>
#include <iostream>

#include <celestis/rpc/message.h>
#include <celestis/rpc/serializationexception.h>

using namespace std;
using namespace Celestis::RPC;

int
main(int argc, const char *argv[])
{
    string test = "foo";
    string test2;
    Message msg;
    try {
	msg.appendU8(150);
	msg.appendS8(-50);
	msg.appendU16(35000);
	msg.appendS16(-50);
	msg.appendU32(5);
	msg.appendS32(5);
	msg.appendU64(42);
	msg.appendS64(42);
	msg.appendStr("Hello");
	msg.appendStr(test);

	msg.seal();
	msg.unseal();

	assert(msg.readU8() == 150);
	assert(msg.readS8() == -50);
	assert(msg.readU16() == 35000);
	assert(msg.readS16() == -50);
	assert(msg.readU32() == 5);
	assert(msg.readS32() == 5);
	assert(msg.readU64() == 42);
	assert(msg.readS64() == 42);
	assert(msg.readStr() == "Hello");
	test2 = msg.readStr();
	assert(test.size() == test2.size());
    } catch (SerializationException &e) {
	cout << "Caught: " << e.what() << endl;
	return 1;
    }
}

