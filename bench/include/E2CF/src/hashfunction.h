#ifndef HASHFUNCTION_H
#define HASHFUNCTION_H

#include<string>
#include<openssl/evp.h>

using namespace std;

class HashFunc{
public:
	HashFunc();
	~HashFunc();
	static std::string sha1(const char* key);
	static std::string md5(const char* key);
	static uint64_t MurmurHash64A (const void* key, int32_t len, uint32_t seed);
};

#endif //HASHFUNCTION_H
