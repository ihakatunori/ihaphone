/*This file is released under the license of IHA License Ver 1.0.
The copyright of this file belongs to Katunori Iha (ihakatunori@gmail.com).

[再配布条件]
・商用利用する場合は著作権者の許諾を必要とする。

[利用者条件]
・商用利用する場合は著作権者の許諾を必要とする。 


How to compile

sudo apt install libssl-dev 
gcc -fPIC -shared   iha.c  -o libiha.so -Wall -I./ -lcrypto

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/resource.h>
#include <signal.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
// mdは遅い  #include <sha256.h>
// mdは遅い  #include <sha512.h>
//#include <Keccak-3.2/KeccakNISTInterface.h>

//#include <immintrin.h>
//#ifdef HAVE_X86INTRIN_H
//#include <x86intrin.h>
//#endif

#include <iha.h>

/***************************************************************************/
/* メモリダンプ関数*/
/***************************************************************************/
/*static	void    dump(unsigned char *p,int len)
{
        int     id;

        printf("***");
        for(id=0;id < len;id++){
                printf("%02x",(unsigned char)*p);
                p++;
        }
        printf("***\n");
}*/
/***************************************************************************/
/* ハッシュの計算*/
/***************************************************************************/
static int	sha_first=0;
static void     sha_cal(int sha,unsigned char *data,int len,unsigned char *hash)
{
        EVP_MD_CTX      *mdctx;
        unsigned int    digest_len;


        if(sha_first == 0){
                OpenSSL_add_all_digests();
                sha_first = 1;
        }

	mdctx = EVP_MD_CTX_create();
	if(sha == 256){
        	EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
	}else if(sha == 384){
        	EVP_DigestInit_ex(mdctx, EVP_sha384(), NULL);
	}else if(sha == 512){
        	EVP_DigestInit_ex(mdctx, EVP_sha512(), NULL);
	}else{
		return;
	}
	EVP_DigestUpdate(mdctx, data, len);
	EVP_DigestFinal_ex(mdctx, hash, &digest_len);
	EVP_MD_CTX_destroy(mdctx);
	return;
}
/***************************************************************************/
/* CPUの再現不可能性を持つ乱数発生関数*/
/***************************************************************************/
static	void my_random3(IHA_ENC_CTX *ctx,unsigned char *cond)
{
//	SHA256_CTX 	c1;
	__uint64_t	*p;

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,ctx->random_cond,ctx->iv_len);
//	SHA256_Final(cond,&c1);

	sha_cal(256,ctx->random_cond,ctx->iv_len,cond);

	p = (__uint64_t *)ctx->random_cond;

	*p = (*p) + 1;
}
//#define	HAVE_RDSEED64
//__uint64_t my_random2(unsigned char cond)
//{
//	__uint64_t      x;
//
//#ifndef	HAVE_RDSEED64
//	x = random();
//#else
//	int		rv=0;
//	while(rv == 0){
//        	rv = _rdseed64_step((unsigned long long *)&x);
//	}
//#endif
//        return x;
//}
/***************************************************************************/
/* インディアン処理*/
/***************************************************************************/
static	int	IsLittleEndian()
{
#if defined(__LITTLE_ENDIAN__)
        return 1;
#elif defined(__BIG_ENDIAN__)
        return 0;
#else
        int i = 1;
        return (int)(*(char*)&i);
#endif
}
static __uint16_t swap16(__uint16_t value)
{
    __uint16_t ret;
    ret  = value << 8;
    ret |= value >> 8;
    return ret;
}
/***************************************************************************/
/* ブロック単位の並べ替え処理*/
/***************************************************************************/
static	int combert_block(IHA_ENC_CTX *ctx,int flg,int size,
			unsigned char *dst,unsigned char *src)
{
	int		i;
	__uint16_t	*com;
// サイズ毎に振り分ける
//	if(size == ctx->blk_len){
//		com = ctx->com_else;
//	}else if(size == LAST_BLK_LEN1(ctx)){
	if(size == LAST_BLK_LEN1(ctx)){
		com = ctx->com_last1;
	}else if(size == LAST_BLK_LEN2(ctx)){
		com = ctx->com_last2;
	}else if(size == LAST_BLK_LEN3(ctx)){
		com = ctx->com_last3;
	}else if(size == LAST_BLK_LEN4(ctx)){
		com = ctx->com_last4;
	}else if(size == LAST_BLK_LEN5(ctx)){
		com = ctx->com_last5;
	}else{
		return IHA_RV_CONTRA;
	}
//
// ８ビットの場合
//
	if(ctx->bits == 8){
		if(flg == 0){
			for(i=0;i < size;i++){
				dst[com[i]] = src[i];
//				*(dst + com[i]) = *(src + i);
// 遅い！！			*(dst + *(com +i)) = *(src + i);
			}
		}else{
			for(i=0;i < size;i++){
				dst[i] = src[com[i]];
//				*(dst + i) = *(src + com[i]);
// 遅い！！			*(dst + i) = *(src + *(com + i));
			}
		}
	}
	return IHA_RV_OK;
/*
//
// ８ビット以外の場合は拡張及び圧縮が必要
//
	if(ctx->bits == 1){		// １ビットを８ビットに拡張
		for(i=0;i < size;i++){
			ctx->blk_work1[i*BPB(ctx)] = (*(src + i) & 0x80) >> 7;
			ctx->blk_work1[i*BPB(ctx)+1] = (*(src + i) & 0x40) >> 6;
			ctx->blk_work1[i*BPB(ctx)+2] = (*(src + i) & 0x20) >> 5;
			ctx->blk_work1[i*BPB(ctx)+3] = (*(src + i) & 0x10) >> 4;
			ctx->blk_work1[i*BPB(ctx)+4] = (*(src + i) & 0x08) >> 3;
			ctx->blk_work1[i*BPB(ctx)+5] = (*(src + i) & 0x04) >> 2;
			ctx->blk_work1[i*BPB(ctx)+6] = (*(src + i) & 0x02) >> 1;
			ctx->blk_work1[i*BPB(ctx)+7] =  *(src + i) & 0x01;
		}
	}else if(ctx->bits == 2){	// ２ビットを８ビットに拡張
		for(i=0;i < size;i++){
			ctx->blk_work1[i*BPB(ctx)  ] = (*(src + i) & 0xc0) >> 6;
			ctx->blk_work1[i*BPB(ctx)+1] = (*(src + i) & 0x30) >> 4;
			ctx->blk_work1[i*BPB(ctx)+2] = (*(src + i) & 0x0c) >> 2;
			ctx->blk_work1[i*BPB(ctx)+3] =  *(src + i) & 0x03;
		}
	}else if(ctx->bits == 4){ 	// ４ビットを８ビットに拡張
		for(i=0;i < size;i++){
			ctx->blk_work1[i*BPB(ctx)] = (*(src + i) & 0xf0) >> 4;
			ctx->blk_work1[i*BPB(ctx)+1] = *(src + i) & 0x0f;
		}
	}
// ８ビット単位で変換
	if(flg == 0){
		for(i=0;i < size*BPB(ctx);i++){
			ctx->blk_work2[com[i]] = ctx->blk_work1[i];
		}
	}else{
		for(i=0;i < size*BPB(ctx);i++){
			ctx->blk_work2[i] = ctx->blk_work1[com[i]];
		}
	}
// 縮小する 
	if(ctx->bits == 1){		// ８ビットを１ビットに縮小
		for(i=0;i < size;i++){
			*(dst + i) = (ctx->blk_work2[i * BPB(ctx)   ] << 7) + 
				     (ctx->blk_work2[i * BPB(ctx) + 1] << 6) +
				     (ctx->blk_work2[i * BPB(ctx) + 2] << 5) +
				     (ctx->blk_work2[i * BPB(ctx) + 3] << 4) +
				     (ctx->blk_work2[i * BPB(ctx) + 4] << 3) +
				     (ctx->blk_work2[i * BPB(ctx) + 5] << 2) +
				     (ctx->blk_work2[i * BPB(ctx) + 6] << 1) +
				      ctx->blk_work2[i * BPB(ctx) + 7] ;
		}
	}else if(ctx->bits == 2){	// ８ビットを２ビットに縮小
		for(i=0;i < size;i++){
			*(dst + i) = (ctx->blk_work2[i * BPB(ctx)    ] << 6) + 
				     (ctx->blk_work2[i * BPB(ctx) + 1] << 4) +
				     (ctx->blk_work2[i * BPB(ctx) + 2] << 2) +
				      ctx->blk_work2[i * BPB(ctx) + 3] ;
		}
	}else if(ctx->bits == 4){ 	// ８ビットを４ビットに縮小
		for(i=0;i < size;i++){
			*(dst + i) = (ctx->blk_work2[i * BPB(ctx)] << 4) + 
				ctx->blk_work2[i * BPB(ctx) + 1] ;
		}
	}
	return IHA_RV_OK;
*/
}
/***************************************************************************/
/* 転置処理*/
/***************************************************************************/
static int combert_all(IHA_ENC_CTX *ctx,int len,int flg,unsigned char *dst,
			unsigned char *src,int no)
{
	int	remain,cmblen,rv;

//	lastflg = 0;
	remain=len;
//	for(remain=len; remain > 0;){
		if(no == 0){
			if(remain == LAST_BLK_LEN1(ctx)){
				cmblen = LAST_BLK_LEN1(ctx);
			}else if(remain >= ctx->blk_len){
				cmblen = ctx->blk_len;
			}else{
				return IHA_RV_NOT_SUPPORT;
			}
		}else if (no == 1){
			if(remain == LAST_BLK_LEN2(ctx)){
				cmblen = LAST_BLK_LEN2(ctx);
			}else if(remain >= ctx->blk_len){
				cmblen = ctx->blk_len;
			}else{
				return IHA_RV_NOT_SUPPORT;
			}
		}else if (no == 2){
			if(remain == LAST_BLK_LEN3(ctx)){
				cmblen = LAST_BLK_LEN3(ctx);
			}else if(remain >= ctx->blk_len){
				cmblen = ctx->blk_len;
			}else{
				return IHA_RV_NOT_SUPPORT;
			}
		}else if (no == 3){
			if(remain == LAST_BLK_LEN4(ctx)){
				cmblen = LAST_BLK_LEN4(ctx);
			}else if(remain >= ctx->blk_len){
				cmblen = ctx->blk_len;
			}else{
				return IHA_RV_NOT_SUPPORT;
			}
		}else if (no == 4){
			if(remain == LAST_BLK_LEN5(ctx)){
				cmblen = LAST_BLK_LEN5(ctx);
			}else if(remain >= ctx->blk_len){
				cmblen = ctx->blk_len;
			}else{
				return IHA_RV_NOT_SUPPORT;
			}
		}else{
			return IHA_RV_NOT_SUPPORT;
		}
		if((rv = combert_block(ctx,flg,cmblen,dst,src)) != IHA_RV_OK){
			return rv;
		}
//		if(     ((no == 0 ) && (cmblen == LAST_BLK_LEN1(ctx))) || 
//			((no == 1 ) && (cmblen == LAST_BLK_LEN2(ctx))) ||
//			((no == 2 ) && (cmblen == LAST_BLK_LEN3(ctx))) ||
//			((no == 3 ) && (cmblen == LAST_BLK_LEN4(ctx))) ||
//		       	((no == 4 ) && (cmblen == LAST_BLK_LEN5(ctx))) ){
//			lastflg = 1;
//			break;
//		}
//		remain -= ctx->blk_len;
//		dst += ctx->blk_len;
//		src += ctx->blk_len;
//	}
//	if(lastflg == 0){
//		return IHA_RV_NOT_SUPPORT;
//	}
	return IHA_RV_OK;
}
static char	*my_fgets(char *buf,int size,int fd)
{
	int	i;

	for(i=0;i<size;i++){
		if(read(fd,&buf[i],1) <=  0){
			if(i==0){
				return NULL;
			}else{
				return buf;
			}
		}
		if(buf[i] == 0x0a){
			buf[i] = 0x00;
			return buf;
		}
	}
	return buf;
}
/***************************************************************************/
/* セクションの読み込み*/
/***************************************************************************/
static int readsection(int fd,char *section,int no,__uint16_t *dst)
{
	char		*str,*delimi,*stop;
	char		readbuf[256];
	int		counter,secflg;

	delimi = ", \n\t";
	counter = 0;
	secflg = 0;


	while(1){
		if(my_fgets(readbuf,sizeof(readbuf),fd) == NULL){
			memset(readbuf,0x00,sizeof(readbuf));
			return IHA_RV_INVALID_FORMAT;
		}
		if(readbuf[0] == '#'){
			continue;
		}
		if(secflg == 0){
			if(memcmp(section,readbuf,strlen(section)) == 0){
				secflg = 1;
			}
			continue;
		}
		str = strtok_r(readbuf,delimi,&stop);
		while(1){
			if(str){
				*dst = atoi(str);
				counter++;
				dst++;
				if(counter == no){
					str = strtok_r(NULL,delimi,&stop);
					if(str){
						memset(readbuf,0x00,sizeof(readbuf));
						return IHA_RV_INVALID_FORMAT;
					}
					memset(readbuf,0x00,sizeof(readbuf));
					return IHA_RV_OK;
				}
				str = strtok_r(NULL,delimi,&stop);
				continue;
			}
			while(1){
				if(my_fgets(readbuf,sizeof(readbuf),fd) == NULL){
					return IHA_RV_INVALID_FORMAT;
				}
				if(readbuf[0] == '#'){
					continue;
				}
				break;
			}
			str = strtok_r(readbuf,delimi,&stop);
		}
	}
	
	memset(readbuf,0x00,sizeof(readbuf));
	return IHA_RV_INVALID_FORMAT;
}
/***************************************************************************/
/* ハッシュの共通関数*/
/***************************************************************************/
static	void com_hash(IHA_ENC_CTX *ctx,unsigned char *addr,int len,unsigned char *out)
{
//	SHA256_CTX 		c1;
//	SHA512_CTX 		c2;

	if(ctx->hash_kind == HASH_SHA_256){
//		SHA256_Init(&c1);
//		SHA256_Update(&c1,addr,len);
//		SHA256_Final(out,&c1);

		sha_cal(256,addr,len,out);
	}else if(ctx->hash_kind == HASH_SHA_512){
//		SHA512_Init(&c2);
//		SHA512_Update(&c2,addr, len);
//		SHA512_Final(out,&c2);

		sha_cal(512,addr,len,out);
//	}else if(ctx->hash_kind == HASH_SHA3_256){
//		Hash(256,addr,len*8,out);
//	}else if(ctx->hash_kind  == HASH_SHA3_384){
//		Hash(384,addr,len*8,out);
//	}else if(ctx->hash_kind == HASH_SHA3_512){
//		Hash(512,addr,len*8,out);
	}
}
void *my_malloc(size_t size)
{
	void *p;

	p = malloc((size_t)size);
	if(p){
		memset(p,0x00,size);
//		if(mlock(p,size) == -1){
//			return NULL;
//		}
	}
	return p;
}
void my_free(void *p,size_t size)
{
	if(p == NULL){
		return;
	}
	memset(p,0x00,size);		// 重要！！
//	munlock(p,size);
	free(p);
	p = NULL;
}
/***************************************************************************/
/* 暗号化終了処理*/
/***************************************************************************/
void	finish_cipher(IHA_ENC_CTX *ctx)
{
	if(ctx == NULL){
		return;
	}
	my_free(ctx->com_last1,COM_LAST_LEN1(ctx));
	my_free(ctx->com_last2,COM_LAST_LEN2(ctx));
	my_free(ctx->com_last3,COM_LAST_LEN3(ctx));
	my_free(ctx->com_last4,(COM_LAST_LEN4(ctx)));
	my_free(ctx->com_last5,COM_LAST_LEN5(ctx));
//	my_free(ctx->com_else,(sizeof(__uint16_t)*ctx->blk_len*BPB(ctx)));

	my_free(ctx->enc_iv,ctx->iv_len);
	my_free(ctx->dec_iv,ctx->iv_len);
	my_free(ctx->shared_key,ctx->hash_len+32);
	my_free(ctx->random_cond,ctx->iv_len);
	my_free(ctx->enc_md,ctx->hash_len+32);
	my_free(ctx->dec_md1,ctx->hash_len+32);
	my_free(ctx->dec_md2,ctx->hash_len+32);
//	my_free(ctx->blk_work1,LAST_BLK_LEN5(ctx) * BPB(ctx));
//	my_free(ctx->blk_work2,LAST_BLK_LEN5(ctx) * BPB(ctx));
	my_free(ctx->enc_work, ctx->max_size + LAST_BLK_LEN5(ctx));
	my_free(ctx->dec_work1,ctx->max_size + LAST_BLK_LEN5(ctx));
	my_free(ctx->dec_work2,ctx->max_size + LAST_BLK_LEN5(ctx));

	my_free(ctx,sizeof(IHA_ENC_CTX));

// システム負荷が大きい！！	munlockall();
}
/***************************************************************************/
/* 初期化処理*/
/***************************************************************************/
IHA_ENC_CTX *init_cipher(char *keyfilename,__uint16_t *com[6],int blk_len,
			int max_size,int hash_kind,int bits,int multi)
{
	int			fd;
	int			i,j;
	IHA_ENC_CTX 		*ctx=NULL;
	struct rlimit 		no_core;
//	SHA256_CTX		c1;
	unsigned char		randomword[128];
	FILE			*fp;
// スワップアウトを防ぐためロックをかける！！以下はシステムへの負担が大きい！！
//	if(mlockall(MCL_CURRENT) != 0){
//		fprintf(stderr,"mlockall error no=%d\n",errno);
//		return NULL;
//	}
// COREをはかないようにする！！
	no_core.rlim_cur = 0;
	no_core.rlim_max = 0;
	if(setrlimit(RLIMIT_CORE,&no_core) == -1){
		fprintf(stderr,"setrlimit error\n");
		return NULL;
	}
// 引数をチェックする
	if(((keyfilename == NULL) && (com[0] == NULL))){
		return NULL;
	}
	if((blk_len < 2 ) || (65535 < blk_len)){
		return NULL;
	}
	if((multi < 1) ||  (5 < multi)){
		return NULL;
	}
	if(max_size <= 0){
		return NULL;
	}
//	if((bits != 1) && (bits != 2) && (bits != 4) && (bits != 8)){
	if(bits != 8){
		return NULL;
	}
	if((ctx = (IHA_ENC_CTX *)my_malloc((size_t)sizeof(IHA_ENC_CTX))) == NULL){
		return NULL;
	}
	if(hash_kind == HASH_NONE){
		ctx->hash_len = 0;
	}else if(hash_kind == HASH_SHA_256){
		ctx->hash_len = 32;
	}else if(hash_kind == HASH_SHA_512){
		ctx->hash_len = 64;
//	}else if(hash_kind == HASH_SHA3_256){
//		ctx->hash_len = 32;
//	}else if(hash_kind == HASH_SHA3_384){
//		ctx->hash_len = 48;
//	}else if(hash_kind == HASH_SHA3_512){
//		ctx->hash_len = 64;
	}else{
		finish_cipher(ctx); return NULL;
	}
// 最終ブロック長、ハッシュサイズ、初期化ベクトルサイズ等の設定
	ctx->blk_len = blk_len;
	ctx->max_size = max_size;
	ctx->hash_kind = hash_kind;
	ctx->bits = bits;
	ctx->iv_len = IHA_IV_LEN;
	ctx->multi = multi;
// メモリを確保する。
	if(keyfilename){
		ctx->com_last1 = (__uint16_t *)my_malloc(COM_LAST_LEN1(ctx));
		if(ctx->com_last1 == NULL){
			finish_cipher(ctx); return NULL;
		}
		if(ctx->multi > 1){
			ctx->com_last2 = (__uint16_t *)my_malloc(COM_LAST_LEN2(ctx));
			if(ctx->com_last2 == NULL){
				finish_cipher(ctx); return NULL;
			}
		}
		if(ctx->multi > 2){
			ctx->com_last3 = (__uint16_t *)my_malloc(COM_LAST_LEN3(ctx));
			if(ctx->com_last3 == NULL){
				finish_cipher(ctx); return NULL;
			}
		}
		if(ctx->multi > 3){
			ctx->com_last4 = (__uint16_t *)my_malloc(COM_LAST_LEN4(ctx));
			if(ctx->com_last4 == NULL){
				finish_cipher(ctx); return NULL;
			}
		}
		if(ctx->multi > 4){
			ctx->com_last5 = (__uint16_t *)my_malloc(COM_LAST_LEN5(ctx));
			if(ctx->com_last5 == NULL){
				finish_cipher(ctx); return NULL;
			}
		}
//		ctx->com_else = (__uint16_t *)my_malloc(sizeof(__uint16_t) * ctx->blk_len * BPB(ctx));
//		if(ctx->com_else == NULL){
//			finish_cipher(ctx); return NULL;
//		}
	}
	ctx->shared_key = (unsigned char *)my_malloc(ctx->hash_len+32);
	if(ctx->shared_key == NULL){
		finish_cipher(ctx); return NULL;
	}
///	ctx->random_cond = (unsigned char *)my_malloc(ctx->hash_len);
	ctx->random_cond = (unsigned char *)my_malloc(ctx->iv_len);
	if(ctx->random_cond == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->dec_md2 = (unsigned char *)my_malloc(ctx->hash_len+32);
	if(ctx->dec_md2 == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->dec_md1 = (unsigned char *)my_malloc(ctx->hash_len+32);
	if(ctx->dec_md1 == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->enc_md = (unsigned char *)my_malloc(ctx->hash_len+32);
	if(ctx->enc_md == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->enc_iv = (unsigned char *)my_malloc(ctx->iv_len);
	if(ctx->enc_iv == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->dec_iv = (unsigned char *)my_malloc(ctx->iv_len);
	if(ctx->dec_iv == NULL){
		finish_cipher(ctx); return NULL;
	}
//	ctx->blk_work1 = (unsigned char *)my_malloc(LAST_BLK_LEN5(ctx) * BPB(ctx) * sizeof(__uint16_t));
//	if(ctx->blk_work1 == NULL){
//		finish_cipher(ctx); return NULL;
//	}
//	ctx->blk_work2 = (unsigned char *)my_malloc(LAST_BLK_LEN5(ctx) * BPB(ctx) * sizeof(__uint16_t));
//	if(ctx->blk_work2 == NULL){
//		finish_cipher(ctx); return NULL;
//	}
	ctx->enc_work = (unsigned char *)my_malloc(ctx->max_size + LAST_BLK_LEN5(ctx));
	if(ctx->enc_work == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->dec_work1 = (unsigned char *)my_malloc(ctx->max_size + LAST_BLK_LEN5(ctx));
	if(ctx->dec_work1 == NULL){
		finish_cipher(ctx); return NULL;
	}
	ctx->dec_work2 = (unsigned char *)my_malloc(ctx->max_size + LAST_BLK_LEN5(ctx));
	if(ctx->dec_work2 == NULL){
		finish_cipher(ctx); return NULL;
	}
	if(keyfilename){
// キーテーブルを読み込む
//		fp = fopen(keyfilename,"r");		// NG　データバッファがメモリ上に残る！！！
//							// キーファイルがミエミエになるよ！！
		fd = open(keyfilename,O_RDONLY);
		if(fd < 0 ){
			finish_cipher(ctx); return NULL;
		}
		if((readsection(fd,"[com_last1]",
				LAST_BLK_LEN1(ctx) * BPB(ctx),ctx->com_last1))){
			finish_cipher(ctx);  close(fd); return NULL;
		}
		if(ctx->multi > 1){
			if((readsection(fd,"[com_last2]",
				LAST_BLK_LEN2(ctx) * BPB(ctx),ctx->com_last2))){
				finish_cipher(ctx);  close(fd); return NULL;
			}
		}
		if(ctx->multi > 2){
			if((readsection(fd,"[com_last3]",
				LAST_BLK_LEN3(ctx) * BPB(ctx),ctx->com_last3))){
				finish_cipher(ctx);  close(fd); return NULL;
			}
		}
		if(ctx->multi > 3){
			if((readsection(fd,"[com_last4]",
				LAST_BLK_LEN4(ctx) * BPB(ctx),ctx->com_last4))){
				finish_cipher(ctx);  close(fd); return NULL;
			}
		}
		if(ctx->multi > 4){
			if((readsection(fd,"[com_last5]",
				LAST_BLK_LEN5(ctx) * BPB(ctx),ctx->com_last5))){
				finish_cipher(ctx);  close(fd); return NULL;
			}
		}
//		if((readsection(fd,"[com_else]",ctx->blk_len * BPB(ctx),ctx->com_else))){
//			finish_cipher(ctx);  close(fd); return NULL;
//		}
		close(fd);
	}else{
		if((com[0] == NULL) || (com[5] == NULL)){
			finish_cipher(ctx); return NULL;
		}
		ctx->com_last1 = com[0];
		ctx->com_last2 = com[1];
		ctx->com_last3 = com[2];
		ctx->com_last4 = com[3];
		ctx->com_last5 = com[4];
//		ctx->com_else = com[5];
	}
// テーブルの重複チェックと値の範囲チェック
	for(i=0;i< LAST_BLK_LEN1(ctx) * BPB(ctx);i++){
		for(j=0;j < LAST_BLK_LEN1(ctx) * BPB(ctx);j++){
			if(i != j){
				if(ctx->com_last1[i] == ctx->com_last1[j]){
					finish_cipher(ctx); return NULL;
				}
			}
		}
		if(LAST_BLK_LEN1(ctx) * BPB(ctx) - 1 < ctx->com_last1[i]){
			finish_cipher(ctx); return NULL;
		}
	}

	if(ctx->multi > 1){
		for(i=0;i< LAST_BLK_LEN2(ctx) * BPB(ctx);i++){
			for(j=0;j < LAST_BLK_LEN2(ctx) * BPB(ctx);j++){
				if(i != j){
					if(ctx->com_last2[i] == ctx->com_last2[j]){
						finish_cipher(ctx); return NULL;
					}
				}
			}
			if(LAST_BLK_LEN2(ctx) * BPB(ctx) - 1 < ctx->com_last2[i]){
				finish_cipher(ctx); return NULL;
			}
		}
	}

	if(ctx->multi > 2){
		for(i=0;i< LAST_BLK_LEN3(ctx) * BPB(ctx);i++){
			for(j=0;j < LAST_BLK_LEN3(ctx) * BPB(ctx);j++){
				if(i != j){
					if(ctx->com_last3[i] == ctx->com_last3[j]){
						finish_cipher(ctx); return NULL;
					}
				}
			}
			if(LAST_BLK_LEN3(ctx) * BPB(ctx) - 1 < ctx->com_last3[i]){
				finish_cipher(ctx); return NULL;
			}
		}
	}
	if(ctx->multi > 3){
		for(i=0;i< LAST_BLK_LEN4(ctx) * BPB(ctx);i++){
			for(j=0;j < LAST_BLK_LEN4(ctx) * BPB(ctx);j++){
				if(i != j){
					if(ctx->com_last4[i] == ctx->com_last4[j]){
						finish_cipher(ctx); return NULL;
					}
				}
			}
			if(LAST_BLK_LEN4(ctx) * BPB(ctx) - 1 < ctx->com_last4[i]){
				finish_cipher(ctx); return NULL;
			}
		}
	}
	if(ctx->multi > 4){
		for(i=0;i< LAST_BLK_LEN5(ctx) * BPB(ctx);i++){
			for(j=0;j < LAST_BLK_LEN5(ctx) * BPB(ctx);j++){
				if(i != j){
					if(ctx->com_last5[i] == ctx->com_last5[j]){
						finish_cipher(ctx); return NULL;
					}
				}
			}
			if(LAST_BLK_LEN5(ctx) * BPB(ctx) - 1 < ctx->com_last5[i]){
				finish_cipher(ctx); return NULL;
			}
		}
	}
/*
	for(i=0;i < ctx->blk_len * BPB(ctx);i++){
		for(j=0;j < ctx->blk_len * BPB(ctx);j++){
			if(i != j){
				if(ctx->com_else[i] == ctx->com_else[j]){
					finish_cipher(ctx); return NULL;
				}
			}
		}
		if(ctx->blk_len * BPB(ctx) - 1 < ctx->com_else[i]){
			finish_cipher(ctx); return NULL;
		}
	}
*/
// 共通キーを生成する
	com_hash(ctx,(unsigned char *)ctx->com_last1,COM_LAST_LEN1(ctx),ctx->enc_md);
	memcpy(ctx->shared_key,ctx->enc_md,ctx->hash_len);
// 乱数の内部状態の初期化
        fp = fopen("/dev/urandom","r");
        if(fp == NULL){
                return NULL;
        }
        if(fread(randomword,1,sizeof(randomword),fp) != sizeof(randomword)){
                fclose(fp);
                return NULL;
        }
        fclose(fp);

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,(unsigned char *)randomword,sizeof(randomword));
//	SHA256_Final(ctx->random_cond,&c1);

	sha_cal(256,(unsigned char *)randomword,sizeof(randomword),ctx->random_cond);

	return ctx;
}
static	void	xor256(unsigned char *src1,unsigned char *src2,unsigned char *res)
{
	int		i;
	__uint64_t	*p,*q,*r;

	p = (__uint64_t *)src1;
	q = (__uint64_t *)src2;
	r = (__uint64_t *)res;

	for(i=0;i<4;i++){
		*r = *p ^ *q;
		p += 1;
		q += 1;
		r += 1;
	}
	return;
}
/***************************************************************************/
/* 連鎖XOR_enc*/
/***************************************************************************/
static void chainxor_enc(IHA_ENC_CTX *ctx,unsigned char *dst,unsigned char *src, int iv_len,
		unsigned char *iv,int len)
{
	int			i,cnt;
	__uint64_t		*llp,llp2[4],*llq;
	unsigned char		iv2[32];
//	SHA256_CTX		c1;

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,iv,32);
//	SHA256_Final(iv2,&c1);

	sha_cal(256,iv,32,iv2);

	if(len%iv_len == 0){
		cnt = len/iv_len;
	}else{
		cnt = len/iv_len + 1;
	}
	llp = (__uint64_t *)dst;
	llq = (__uint64_t *)src;

	xor256((unsigned char *)llq,iv2,(unsigned char *)llp2);
	memcpy((unsigned char *)llp,(unsigned char *)llp2,32);
	llq += 4;
	llp += 4;

	for(i=0 ; i < cnt - 1 ; i++){
		xor256((unsigned char *)llp2,(unsigned char *)llq,(unsigned char *)llp);
		xor256((unsigned char *)llp,iv2,(unsigned char *)llp);
		memcpy((unsigned char *)llp2,(unsigned char *)llp,32);
		llp += 4;
		llq += 4;
	}
	return;
}
/***************************************************************************/
/* 連鎖XOR_dec*/
/***************************************************************************/
static void chainxor_dec(IHA_ENC_CTX *ctx,unsigned char *dst,unsigned char *src,int iv_len,
		unsigned char *iv, int len)
{
	int			i,cnt;
	__uint64_t		*llp,*llq;
	unsigned char		iv2[32];
//	SHA256_CTX		c1;

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,iv,32);
//	SHA256_Final(iv2,&c1);

	sha_cal(256,iv,32,iv2);


	if(len%iv_len == 0){
		cnt = len/iv_len;
	}else{
		cnt = len/iv_len + 1;
	}
	llp = (__uint64_t *)dst;
	llq = (__uint64_t *)src;

	xor256((unsigned char *)llq,iv2,(unsigned char *)llp);
	llp += 4;

	for(i=0 ; i < cnt - 1 ; i++){
		xor256((unsigned char *)llq,(unsigned char *)(llq + 4),(unsigned char *)llp);
		xor256((unsigned char *)llp,iv2,(unsigned char *)llp);
		llp += 4;
		llq += 4;
	}
	return;
}
static	void set_dummy(IHA_ENC_CTX *ctx,unsigned char *p,int len)
{

	while(len > 0){
		my_random3(ctx,(unsigned char *)ctx->enc_md);

		if(len <= 32){
			memcpy(p,ctx->enc_md,len);
			break;
		}else{
			memcpy(p,ctx->enc_md,32);
			len -= 32;
			p += 32;
		}
	}
}
/***************************************************************************/
/* 暗号化*/
/***************************************************************************/
int encrypt_data(IHA_ENC_CTX *ctx,int len,unsigned char *bdata,int *alen,unsigned char *adata)
{
	int			j,rvlen;
	__uint16_t		*wp,length,dlen;
//	SHA256_CTX		c1;
// 引数のチェック
	if((bdata == NULL) || (alen == NULL) || (adata == NULL)){
		return IHA_RV_PARAM_ERROR;
	}
	*alen = 0;
// サイズを確認する。
	if((len <= 0) || (ctx->max_size < len)){
		return IHA_RV_NOT_SUPPORT;
	}
	memcpy(adata,bdata,len);
	if(len <= ctx->blk_len){
// ダミーデータ長を設定
		wp = (__uint16_t *)&adata[ctx->blk_len]; 
		dlen = ctx->blk_len - len;
		length = dlen;
		if(IsLittleEndian()){
			length = swap16(length);
		}
		set_dummy(ctx,adata + len,dlen);
		*wp = length;
// 誤り検出符号の挿入
		rvlen = ctx->blk_len + IHA_DUMMY_LEN;
 		if(ctx->hash_len){
			memcpy(adata + ctx->blk_len + IHA_DUMMY_LEN,ctx->shared_key,ctx->hash_len);
			com_hash(ctx,adata,ctx->blk_len + IHA_DUMMY_LEN + ctx->hash_len,ctx->enc_md);
			memcpy(adata + ctx->blk_len + IHA_DUMMY_LEN,ctx->enc_md,ctx->hash_len);
			rvlen += ctx->hash_len;
		}
	}else{
//		return IHA_RV_NOT_SUPPORT;

		if(len % ctx->blk_len){
			dlen = ctx->blk_len - (len % ctx->blk_len);
			wp = (__uint16_t *)&adata[(len/ctx->blk_len + 1)*ctx->blk_len];

			set_dummy(ctx,adata + len,dlen);
		}else{
			dlen = 0;		// ダミーデータなし
			wp = (__uint16_t *)&adata[len];
		}
// ダミーデータ長を設定
		length = dlen;
		if(IsLittleEndian()){
			length = swap16(length);
		}
		*wp = length;
// 誤り検出符号の挿入
		rvlen = len + dlen + IHA_DUMMY_LEN;
 		if(ctx->hash_len){
			memcpy(adata + len + dlen + IHA_DUMMY_LEN,ctx->shared_key,ctx->hash_len);
			com_hash(ctx,adata,len + dlen + IHA_DUMMY_LEN + ctx->hash_len,ctx->enc_md);
			memcpy(adata + len + dlen + IHA_DUMMY_LEN,ctx->enc_md,ctx->hash_len);
			rvlen += ctx->hash_len;
		}

	}
	for(j=0;j <  ctx->multi ; j++){
// IVを生成する。
		my_random3(ctx,(unsigned char *)ctx->enc_iv);
// adataとivのXORを連鎖してとり、workに保存する。
		chainxor_enc(ctx,ctx->enc_work,adata,ctx->iv_len,ctx->enc_iv,rvlen);
// IVを変形して最後部に挿入する。
//		SHA256_Init(&c1);
//		SHA256_Update(&c1,ctx->enc_work,rvlen);
//		SHA256_Final(ctx->enc_md,&c1);

		sha_cal(256,ctx->enc_work,rvlen,ctx->enc_md);

		xor256(ctx->enc_iv,ctx->enc_md,ctx->enc_iv);
		memcpy(ctx->enc_work+rvlen,ctx->enc_iv,ctx->iv_len);
		rvlen += ctx->iv_len;
// 並べ替え。workを並べ替えてadataに格納する。
		if(combert_all(ctx,rvlen,0,adata,ctx->enc_work,j) != IHA_RV_OK){
			return IHA_RV_CONTRA;
		}
	}
	*alen = rvlen;
	return IHA_RV_OK;
}
/***************************************************************************/
/* 復号処理のサイズ*/
/***************************************************************************/
static	int	check_size(IHA_ENC_CTX *ctx,int len)
{
	switch(ctx->multi){
	case 5:
		if((len < LAST_BLK_LEN5(ctx)) || (ctx->max_size + LAST_BLK_LEN5(ctx) < len)){
			return IHA_RV_NOT_SUPPORT;
		}else if(len == LAST_BLK_LEN5(ctx)){
			;
		}else{
			if((len - LAST_BLK_LEN5(ctx))%ctx->blk_len != 0){
				return IHA_RV_NOT_SUPPORT;
			}
		}
		break;
	case 4:
		if((len < LAST_BLK_LEN4(ctx)) || (ctx->max_size + LAST_BLK_LEN4(ctx) < len)){
			return IHA_RV_NOT_SUPPORT;
		}else if(len == LAST_BLK_LEN4(ctx)){
			;
		}else{
			if((len - LAST_BLK_LEN4(ctx))%ctx->blk_len != 0){
				return IHA_RV_NOT_SUPPORT;
			}
		}
		break;
	case 3:
		if((len < LAST_BLK_LEN3(ctx)) || (ctx->max_size + LAST_BLK_LEN3(ctx) < len)){
			return IHA_RV_NOT_SUPPORT;
		}else if(len == LAST_BLK_LEN3(ctx)){
			;
		}else{
			if((len - LAST_BLK_LEN3(ctx))%ctx->blk_len != 0){
				return IHA_RV_NOT_SUPPORT;
			}
		}
		break;
	case 2:
		if((len < LAST_BLK_LEN2(ctx)) || (ctx->max_size + LAST_BLK_LEN2(ctx) < len)){
			return IHA_RV_NOT_SUPPORT;
		}else if(len == LAST_BLK_LEN2(ctx)){
			;
		}else{
			if((len - LAST_BLK_LEN2(ctx))%ctx->blk_len != 0){
				return IHA_RV_NOT_SUPPORT;
			}
		}
		break;
	case 1:
		if((len < LAST_BLK_LEN1(ctx)) || (ctx->max_size + LAST_BLK_LEN1(ctx) < len)){
			return IHA_RV_NOT_SUPPORT;
		}else if(len == LAST_BLK_LEN1(ctx)){
			;
		}else{
			if((len - LAST_BLK_LEN1(ctx))%ctx->blk_len != 0){
				return IHA_RV_NOT_SUPPORT;
			}
		}
		break;
	default:
		return IHA_RV_CONTRA;
	}
	return IHA_RV_OK;
}
/***************************************************************************/
/* 復号*/
/***************************************************************************/
int decrypt_data(IHA_ENC_CTX *ctx,int len,unsigned char *bdata,int *alen,unsigned char *adata)
{
	unsigned char		*p;
	int			j,rv;
	__uint16_t		*wp,dlen;
//	SHA256_CTX		c1;
// 引数のチェック
	if((bdata == NULL) || (alen == NULL) || (adata == NULL)){
		return IHA_RV_PARAM_ERROR;
	}
	*alen = 0;
	rv = IHA_RV_OK;
// サイズを確認する。
	if((rv = check_size(ctx,len)) != IHA_RV_OK){
		return rv;
	}
	j = ctx->multi - 1;
	p = bdata;
	for(; 0 <=  j ; j--){
// bdataを並べ替えてwork1に入れる。
		if(combert_all(ctx,len,1,ctx->dec_work1,p,j) != IHA_RV_OK){
/////////			memset(adata,0x00,len);
/////////			return IHA_RV_NOT_SUPPORT;
			rv = IHA_RV_NOT_SUPPORT;
		}
// IVをwork1の最後部から取り出し、もとに戻す。
		memcpy(ctx->dec_iv,(ctx->dec_work1 + len - ctx->iv_len),ctx->iv_len);
//		SHA256_Init(&c1);
//		SHA256_Update(&c1,ctx->dec_work1,len - ctx->iv_len);
//		SHA256_Final(ctx->dec_md2,&c1);

		sha_cal(256,ctx->dec_work1,len - ctx->iv_len,ctx->dec_md2);

		xor256(ctx->dec_iv,ctx->dec_md2,ctx->dec_iv);
// work1をivと連鎖XORしてwork2に入れる。
		chainxor_dec(ctx,ctx->dec_work2,ctx->dec_work1,ctx->iv_len,ctx->dec_iv,len - ctx->iv_len);
		if(j!=0){
			len -= ctx->iv_len;
			p = ctx->dec_work2;
		}
	}
// 誤り検出符号の確認（改ざんされていないか？）
	if(ctx->hash_len){
		memcpy(ctx->dec_md1,ctx->dec_work2 + len - ctx->hash_len - ctx->iv_len , ctx->hash_len);
		memcpy(ctx->dec_work2 + len - ctx->hash_len - ctx->iv_len,
			ctx->shared_key,ctx->hash_len);
		com_hash(ctx,ctx->dec_work2,len  - ctx->iv_len,ctx->dec_md2);
		if(memcmp(ctx->dec_md2,ctx->dec_md1,ctx->hash_len) != 0){
//////////////			memset(adata,0x00,len);
//////////////			return IHA_RV_CHECKSUM_ERROR;
			rv = IHA_RV_CHECKSUM_ERROR;
		}
	}
// ダミーデータ長を取得する。
	p = ctx->dec_work2;
	p += len - LAST_BLK_LEN1(ctx);
	wp =(__uint16_t *)&p[ctx->blk_len];
	dlen = *wp;
	if(IsLittleEndian()){
		dlen = swap16(dlen);
	}
	if(ctx->blk_len <= dlen){
///////		memset(adata,0x00,len);
///////		return IHA_RV_NOT_SUPPORT;
		dlen = 0;
		rv = IHA_RV_NOT_SUPPORT;
	}
// データ長を設定する。
	*alen = len - dlen - IHA_DUMMY_LEN - ctx->hash_len - ctx->iv_len;
	if(*alen < 0){
		*alen = 0;
	}
// 全データをadataにコピーする。
	memcpy(adata,ctx->dec_work2,*alen);
////////////////	return IHA_RV_OK;
	return rv;
}
