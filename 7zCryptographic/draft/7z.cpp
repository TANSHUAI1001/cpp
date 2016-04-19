/* 
 * 实现功能：7z压缩算法验证口令的正确性
 * 加密算法：AES-256
 * 散列算法：SHA256
 * 主要流程：初始化 -> 读取密文的第一块 -> 解密
 * 时    间：2015年12月6日
 * 作    者：孙 凛
 */

// #define DEBUG

#include <string.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <assert.h>
// #include <malloc.h>
#include <mm_malloc.h>//In the OSX system envirenment, modified by tanshuai

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned long uint64;

#define GetUi32(p) (*(const uint32 *)(p))
#define SetUi32(p, d) *(uint32 *)(p) = (d);

#define xtime(x) ((((x) << 1) ^ (((x) & 0x80) != 0 ? 0x1B : 0)) & 0xFF)
#define Ui32(a0, a1, a2, a3) ((uint32)(a0) | ((uint32)(a1) << 8) | ((uint32)(a2) << 16) | ((uint32)(a3) << 24))


/****************************************************************************************************
************************************用户输入密码散列处理的过程分析***********************************
*****************************************************************************************************/
// 编码方式：UTF-8 -> UCS-2
void stringToUnicode(char *src, char *rst)
{
	int len = strlen((char *)src);
	int i = 0;
	for (i = 0; i < len; i++)
	{
		rst[2*i] = src[i];
		rst[2*i+1] = '\0';
	}
}

// 请求用户输入密码
void getTextPassword(char *&pwd, uint32 &pwdLen){
	char password[20];
	printf("Please enter password!\n");
	scanf("%s", password);	// 得到口令
	
	pwdLen = strlen(password)*2;
	pwd = (char *)malloc((strlen(password)*2+1)*sizeof(char)); // 多申请1的空间存放'\0'
	stringToUnicode(password, pwd);	// UTF-8 —> UCS-2
}

// 对UCS-2编码方式的Password进行sha256散列处理，最后生成32字节的数据
// 注：单文件情况下
void calculateDigest(char *pwd, uint32 pwdLen, uint8 *key, uint32 keyLen){ // keyLen = 32
	SHA256_CTX ctx;
	
	SHA256_Init(&ctx);
	const uint32 NumCyclesPower = 19;
	const uint32 numRounds = (uint64)(1 << NumCyclesPower);	// NumCyclesPower的值是19，这个数字来源于Props的第一字节
	uint8 temp[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	// for(uint32 round = 0; round < numRounds; round++){
	for(uint32 round = 0; round < numRounds; round++){
		// SHA256_Update(&ctx, Salt, SaltSize); // SaltSize = 0, 第一句实际上什么也没做 ?!
		SHA256_Update(&ctx, pwd, pwdLen);
		SHA256_Update(&ctx, temp, 8);
		for(int i=0; i<8; i++)
			if(++(temp[i])!=0)
				break;
	}
	SHA256_Final(key, &ctx);
}

/****************************************************************************************************
**************************************AES解密以及验证口令的正确性************************************
*****************************************************************************************************/
#define gb0(x) ( (x)          & 0xFF)
#define gb1(x) (((x) >> ( 8)) & 0xFF)
#define gb2(x) (((x) >> (16)) & 0xFF)
#define gb3(x) (((x) >> (24)) & 0xFF)

static uint32 T[256 * 4];
static uint32 D[256 * 4];
static uint8 InvS[256];

static uint8 Rcon[11] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36 };
static uint8 Sbox[256] = {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

/**********************************初始化*****************************/
// 初始化第一部分：AES表的初始化
void AesGenTables(void){
	uint32 i;
	for(i=0; i<256; i++)
		InvS[Sbox[i]] = (uint8)i;
	for(i=0; i<256; i++){
		{
			uint32 a1 = Sbox[i];
			uint32 a2 = xtime(a1);
			uint32 a3 = a2 ^ a1;
			
			T[        i] = Ui32(a2, a1, a1, a3); // Ui32是一个宏
			T[0x100 + i] = Ui32(a3, a2, a1, a1);
			T[0x200 + i] = Ui32(a1, a3, a2, a1);
			T[0x300 + i] = Ui32(a1, a1, a3, a2);
		}
		{
			uint32 a1 = InvS[i];
			uint32 a2 = xtime(a1);
			uint32 a4 = xtime(a2);
			uint32 a8 = xtime(a4);
			uint32 a9 = a8 ^ a1;
			uint32 aB = a8 ^ a2 ^ a1;
			uint32 aD = a8 ^ a4 ^ a1;
			uint32 aE = a8 ^ a4 ^ a2;
			D[        i] = Ui32(aE, a9, aD, aB);
			D[0x100 + i] = Ui32(aB, aE, a9, aD);
			D[0x200 + i] = Ui32(aD, aB, aE, a9);
			D[0x300 + i] = Ui32(a9, aD, aB, aE);
		}
	}
}

// 初始化第二部分：_aes[]的初始化，即RoundKey和iv的生成，以及传递到_aes中
#define AES_NUM_IVMRK_WORDS ((1 + 1 + 15) * 4) // 17*4 = 68
uint32 _aes[AES_NUM_IVMRK_WORDS + 3]; // _aes长度为71

#ifdef DEBUG
const uint32 _aesLen = AES_NUM_IVMRK_WORDS + 3;
#endif

/* // 初始化RoundKey，iv以及_aes
void Init(){
	CalculateDigest();
	// 设置_aes数组 Aes_SetKey_Enc 或者 Aes_SetKey_Dec
	// 设置_iv数组	AesCbc_Init
} */

// generate Enc_key， 其中w 就是_aes
void Aes_SetKey_Enc(uint32 *w, const uint8 *key, uint32 keySize){ // key是之前sha256产生的32位散列值
	uint32 i, wSize;
	wSize = keySize + 28; // 60 = 32 + 28
	keySize /= 4;
	w[0] = ((uint32)keySize / 2) + 3;// w[0], _aes[6] = 4+3
	w += 4;
	
	for(i=0; i<keySize; i++, key += 4){ // _aes[10~13] <-- key
		// w[i] = *(uint32 *key);	// 可能存在大小端问题
		w[i] = GetUi32(key);	// 可能存在大小端问题
	}
		
	for(; i<wSize; i++){ //_aes[14~70]的填充
		uint32 t = w[i-1];
		uint32 rem = i%keySize;
		if (rem == 0)
			t = Ui32(Sbox[gb1(t)] ^ Rcon[i / keySize], Sbox[gb2(t)], Sbox[gb3(t)], Sbox[gb0(t)]);
		else if (keySize > 6 && rem == 4)
			t = Ui32(Sbox[gb0(t)], Sbox[gb1(t)], Sbox[gb2(t)], Sbox[gb3(t)]);
		w[i] = w[i - keySize] ^ t;
	}
}

// generate Dec_key
void Aes_SetKey_Dec(uint32 *w, const uint8 *key, uint32 keySize){
	uint32 i, num;
	Aes_SetKey_Enc(w, key, keySize);
	num = keySize + 20;
	w += 8;
	for (i = 0; i < num; i++){
		uint32 r = w[i];
		w[i] =
			D[        Sbox[gb0(r)]] ^
			D[0x100 + Sbox[gb1(r)]] ^
			D[0x200 + Sbox[gb2(r)]] ^
			D[0x300 + Sbox[gb3(r)]];
	}
}

// iv数据的传递, _iv长度为16字节，填充了_aes的前4个元素(4个uint32)
void AesCbc_Init(uint32 *p, const uint8 *iv){
	uint32 i;
	for(i=0; i<4; i++)
		p[i] = GetUi32(iv + i * 4);	// 可能存在大小端问题
}

/*********************************Aes解密过程**********************************/
#define AES_BLOCK_SIZE 16
// AES-256 的解密算法
void Aes_Decode(const uint32 *w, uint32 *dest, const uint32 *src);

/* 
 * 加密模式：CBC	
 * 加密算法：AES-256
 * 功    能：对输入的密文块镜像解密。
 */
void AesCbc_Decode(uint32 *p, uint8 *data, uint32 numBlocks){
	uint32 in[4], out[4];
	for(; numBlocks != 0; numBlocks--, data += AES_BLOCK_SIZE){
		in[0] = GetUi32(data);
		in[1] = GetUi32(data + 4);
		in[2] = GetUi32(data + 8);
		in[3] = GetUi32(data + 12);
		
		Aes_Decode(p + 4, out, in);
		
		SetUi32(data,      p[0] ^ out[0]);
		SetUi32(data + 4,  p[1] ^ out[1]);
		SetUi32(data + 8,  p[2] ^ out[2]);
		SetUi32(data + 12, p[3] ^ out[3]);
		
		p[0] = in[0];
		p[1] = in[1];
		p[2] = in[2];
		p[3] = in[3];
	}
}

#define HD(i, x, s) (D + (x << 8))[gb ## x(s[(i - x) & 3])]
#define HD4(m, i, s, p) m[i] = \
    HD(i, 0, s) ^ \
    HD(i, 1, s) ^ \
    HD(i, 2, s) ^ \
    HD(i, 3, s) ^ w[p + i];
#define HD16(m, s, p) \
    HD4(m, 0, s, p); \
    HD4(m, 2, s, p); \
    HD4(m, 3, s, p); \
    HD4(m, 1, s, p);

#define FD(i, x) InvS[gb ## x(m[(i - x) & 3])]
#define FD4(i) dest[i] = Ui32(FD(i, 0), FD(i, 1), FD(i, 2), FD(i, 3)) ^ w[i];
void Aes_Decode(const uint32 *w, uint32 *dest, const uint32 *src){
	uint32 s[4];
	uint32 m[4];
	uint32 numRounds2 = w[0];
	w += 4 + numRounds2 * 8;

	s[0] = src[0] ^ w[0];
	s[1] = src[1] ^ w[1];
	s[2] = src[2] ^ w[2];
	s[3] = src[3] ^ w[3];
	for (;;){
		w -= 8;
		HD16(m, s, 4);
		if (--numRounds2 == 0)
		  break;
		HD16(s, m, 0);
	}
	FD4(0); FD4(1); FD4(2); FD4(3);
}

/*********************************验证口令的正误**********************************/
// 验证依据非常简单：看明文第一字节的值是否为0. 若为0则密码正确；否则密码错误。

/*
 * 功能：7z验证口令
 * 流程：（1）读取输入
 *	   	 （2）用户输入密码
 *		 （3）生成AES表
 *		 （4）根据1，2里的输入构造_aes数组
 *		 （5）AES解密
 *		 （6）验证第一字节是否为0
 *		 （7）输出结果
 *
 */
int main(int argc, char *argv[]){
	// 从iv_buffer.dat中读取输入信息，格式为：iv数据（16字节）*密文数据（不定长，不大于131072字节）
	uint32 cipherLen;
	uint8 iv_arr[16], cipher[131072]; // 用来存放iv数组信息，一块密文最长为：1<<17 = 131072
	FILE *pfd = fopen("iv_buffer.dat", "rb");
	assert(pfd!=NULL);
	fread(iv_arr, sizeof(uint8), 16, pfd); // 读取iv，一共16字节
	// 压缩后的数据大于这个值时，只会读取131072字节，当小于这个值时，有多少就读多少
	fseek(pfd, 1L, SEEK_CUR); //跳开星号字符(ASCII: 0x2a)
	fread(cipher, sizeof(uint8), 131072, pfd); // 读取第一块密文(最长为 131072 Bytes)
	cipherLen = strlen((char *)cipher);
	fclose(pfd);
	pfd = NULL;
	

#ifdef DEBUG
printf("iv: ");
for(int i=0; i<16; i++)
	printf("%.2x ", iv_arr[i]);
printf("\n");
printf("cipherLen: %d\n", cipherLen);
for(int i=0; i<cipherLen; i++)
	printf("%.2x ", cipher[i]);

printf("\n");
#endif	

	char *pwd;
	uint32 pwdLen;
	getTextPassword(pwd, pwdLen); // UTF-8 -> UCS-2
	
	AesGenTables(); // 
	
#ifdef DEBUG
printf("Password(UCS-2): ");
for(int i=0; i<pwdLen; i++)
	printf("%.2x ", pwd[i]);
printf("\n");
#endif

	uint32 keyLen = 32, offset = 2;
	uint8 key[keyLen];
	calculateDigest(pwd, pwdLen, key, keyLen);	// Roundkey(32Bytes)
	
#ifdef DEBUG
printf("Roundkey: ");
for(int i=0; i<keyLen; i++)
	printf("%.2x", key[i]);
printf("\n");
#endif

	Aes_SetKey_Dec(_aes + offset + 4, key, keyLen); // 设置_aes数组 Aes_SetKey_Enc 或者 Aes_SetKey_Dec
	AesCbc_Init(_aes + offset, iv_arr); // 设置_iv数组 AesCbc_Init

#ifdef DEBUG
printf("_aes: ");
for(int i=0; i<_aesLen; i++)
	printf("%.8x ", _aes[i]);
printf("\n");
#endif
	
		AesCbc_Decode(_aes + offset, cipher, 1);

#ifdef DEBUG
printf("After decode, cipher:\n");
for(int i=0; i<cipherLen; i++)
	printf("%.2x ", cipher[i]);
printf("\n");
#endif	
	
	if(cipher[0] == 0x00)
		printf("Correct password!\n");
	else
		printf("Error password!\n");
	
	return 0;
}