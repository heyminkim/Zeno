/*
 *
 *      Author:
 */



#include "hashfunction.h"
#include<math.h>
#include<cstring>
#include<iostream>
using namespace std;

string  HashFunc::sha1(const char* key){
	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;
	// cout<<">>>>"<<endl;
	EVP_DigestInit(mdctx, EVP_sha1());
	EVP_DigestUpdate(mdctx, (const void*) key, strlen(key));
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_free(mdctx);

	return std::string((char*)md_value, (size_t)md_len);
}

string HashFunc::md5(const char* key){
	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;

	EVP_DigestInit(mdctx, EVP_md5());
	EVP_DigestUpdate(mdctx, (const void*) key, sizeof(key));
	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_free(mdctx);

	return std::string((char*)md_value, (size_t)md_len);
}

/**
 * Murmurhash to make all benchmarks use the same hash. 
 */
uint64_t HashFunc::MurmurHash64A ( const void* key, int32_t len, uint32_t seed )
{
	const uint64_t m = 0xc6a4a7935bd1e995;
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t* data = (const uint64_t*)key;
	const uint64_t* end = data + (len/8);

	while(data != end)
	{
		uint64_t k = *data++;

		k *= m; 
		k ^= k >> r; 
		k *= m; 

		h ^= k;
		h *= m; 
	}

	const unsigned char* data2 = (const unsigned char*)data;

	switch(len & 7)
	{
		case 7: h ^= (uint64_t)data2[6] << 48;
		case 6: h ^= (uint64_t)data2[5] << 40;
		case 5: h ^= (uint64_t)data2[4] << 32;
		case 4: h ^= (uint64_t)data2[3] << 24;
		case 3: h ^= (uint64_t)data2[2] << 16;
		case 2: h ^= (uint64_t)data2[1] << 8;
		case 1: h ^= (uint64_t)data2[0];
						h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}