/*This file is released under the license of IHA License Ver 1.0.
The copyright of this file belongs to Katunori Iha (ihakatunori@gmail.com).

[再配布条件]
　・商用利用する場合は著作権者の許諾を必要とする。

[利用者条件]
　・利用者条件はありません。 


How to compile

sudo apt install libssl-dev libgmp-dev
gcc ihakeygen.c -o ihakeygen -lcrypto -lgmp -Wall

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <gmp.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

//#include <immintrin.h>
//#ifdef HAVE_X86INTRIN_H
//#include <x86intrin.h>
//#endif

//#define HAVE_RDSEED64   1
//__uint64_t my_random2(void)
//{
//        __uint64_t      x;
//
//#ifndef HAVE_RDSEED64
//        x = random();
//#else
//        int             rv=0;
//        while(rv == 0){
//                rv = _rdseed64_step((unsigned long long *)&x);
//        }
//#endif
//        return x;
//}

static int      sha_first=0;
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
                printf("naibu error\n");
                return;
        }
        EVP_DigestUpdate(mdctx, data, len);
        EVP_DigestFinal_ex(mdctx, hash, &digest_len);
        EVP_MD_CTX_destroy(mdctx);
        return;
}

int my_random(unsigned char *password,mpz_t N,mpz_t e)
{
	struct	timespec	ts;
//	SHA256_CTX		c1;
	unsigned char		*p,hash[32],hashdata[65],tmp[3];
	__int64_t		*nsecp;
	__uint64_t		*tscp;
	int			rv,i;

	mpz_init(e);

	clock_gettime(CLOCK_REALTIME, &ts);
	tscp = (__uint64_t *)(password+100);
	nsecp =(__int64_t *)(password+108);
//	*tscp = my_random2();
	*tscp = random();
	*nsecp = ts.tv_nsec;

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,password,116);
//	SHA256_Final(hash,&c1);

	sha_cal(256,password,116,hash);

  
	p = hash;
	hashdata[0] = 0x00;
	for(i=0;i<32;i++){
		sprintf((char *)tmp,"%02x",*p);
		strcat((char *)hashdata,(char *)tmp);
		p++;
	}

	mpz_init_set_str(e,(char *)hashdata,16);
//	mpz_out_str (stdout, 10, e);
//	printf("\n");
//	printf("mod=");
	mpz_mod(e,e,N);
//	mpz_out_str (stdout, 10, e);
//	printf("\n");

	rv = (int)mpz_get_ui(e);

	mpz_clear(e);

	return rv;
}

int put_number(FILE *fp,unsigned char *password,int no)
{
	int		i,ari,add,k,j,len,rand;
	unsigned int	*com;
	char		tmpbuf[128],buf[4096];
	mpz_t		N,e;

	mpz_init(N);
//	mpz_init(e);

	add = 0;
	sprintf(tmpbuf,"%d",no);
	mpz_set_str(N,tmpbuf, 10);

	com = (unsigned int *)malloc(no*4+4);
	if(com == NULL ){
		printf("memory error\n");
		return -1;
	}

	for(i=0;i<no+1;i++){
		com[i] = -1;
	}

	while(1){
		rand = my_random(password,N,e)%no;
		for(j=0;j<no;j++){
			if(com[j] == -1){
				ari = 0;
				for(k=0;k<j;k++){
					if(com[k] == rand){
						ari = 1;
						break;
					}
				}
				if(ari == 0){
					com[j] = rand;
					add++;
				}
			}
		}
		if(add == no){
			break;
		}
	}

	buf[0] = 0x00;
	for(i=0;i<no;i++){
		if(strlen(buf) >= 100){
			strcat(buf,"\n");
			fwrite(buf,strlen(buf),1,fp);
			buf[0] = 0x00;
		}
		sprintf(tmpbuf,"%d,",com[i]);
		strcat(buf,tmpbuf);
        }
	len = strlen(buf);
	if(len){
		buf[len-1] = '\n';
		buf[len] = 0x00;
        	fwrite(buf,strlen(buf),1,fp);
	}

	free(com);

//      mpz_clear(e);
        mpz_clear(N);

	return 0;
}

int main(int argc, char **argv) {
	unsigned char	randomword[100+17];
	FILE		*fp;
	int		threshold,hashsize,bitsno,multi,iv_len;
	char		buf[121];
	int		i;


	if(argc != 6){
		printf("usage : %s THRESHOLD hashsize bitsno multi filename\n",argv[0]);
		return 0;
	}
	threshold = atoi(argv[1]);
	hashsize = atoi(argv[2]);
	bitsno = atoi(argv[3]);
	multi = atoi(argv[4]);
	iv_len = 32;

	if((threshold < 100) || (65535 < threshold)){
		printf("threshold is from 100 to 65535\n");
		return 0;
	}
	if((hashsize !=0 ) && (hashsize != 32 ) && (hashsize != 48) && (hashsize != 64)){
		printf("hash size is 32 or 48 or 64\n");
		return 0;
	}
	if(bitsno != 8){
		printf("bits no is 8\n");
		return 0;
	}
	if((multi < 2) || (5 < multi)){
		printf("multi is from 2 to 5\n");
		return 0;
	}

	fp = fopen("/dev/urandom","r");
	if(fp == NULL){
		printf("urandom open error\n");
		return -1;
	}
	if(fread(randomword,1,sizeof(randomword),fp) != sizeof(randomword)){
		printf("urandom read error\n");
		fclose(fp);
		return -1;
	}
	fclose(fp);


/*
// 乱数の初期化
#ifndef HAVE_RDSEED64
        srandom((unsigned int)time(NULL));
#endif

	printf("enter secret words(max 100 bytes) ---> ");
	tcgetattr(STDIN_FILENO, &term);
	save = term;
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &term);
	memset(password,0x00,sizeof(password));
	for(i = 0; i < sizeof(password) - 1; i++) {
		tmp = fgetc(stdin);
		if (tmp < 0 || iscntrl(tmp)) {
			fprintf(stderr, "\n");
			break;
		}
		password[i] = tmp;
		if(i==100){
			break;
		}
	}
//	password[i] = 0;
//	printf("%s\n", password);
	tcsetattr(STDIN_FILENO, TCSANOW, &save);
*/



	fp = fopen(argv[5],"w");
	if(fp == NULL){
		printf("fopen error filename=%s\n",argv[5]);
		return 0;
	}
 
	fwrite("[com_last1]\n",12,1,fp);
	put_number(fp,randomword,(threshold + 2 + hashsize + iv_len)*8/bitsno);

	if(multi >= 2){
		fwrite("[com_last2]\n",12,1,fp);
		put_number(fp,randomword,(threshold + 2 + hashsize + iv_len*2)*8/bitsno);
	}
	if(multi >= 3){
		fwrite("[com_last3]\n",12,1,fp);
		put_number(fp,randomword,(threshold + 2 + hashsize + iv_len*3)*8/bitsno);
	}
	if(multi >= 4){
		fwrite("[com_last4]\n",12,1,fp);
		put_number(fp,randomword,(threshold + 2 + hashsize + iv_len*4)*8/bitsno);
	}
	if(multi >= 5){
		fwrite("[com_last5]\n",12,1,fp);
		put_number(fp,randomword,(threshold + 2 + hashsize + iv_len*5)*8/bitsno);
	}

	fwrite("[com_else]\n",11,1,fp);
	put_number(fp,randomword,threshold*8/bitsno);

	fwrite("[aes_gcm]\n",10,1,fp);

	for(i=0;i<60;i++){
		sprintf(&buf[i*2],"%02x",randomword[i]);
	}
	buf[120] = '\n';
	
	fwrite(buf,121,1,fp);


	fclose(fp);

	printf("Successfuly done!!\n");
	return 0;
}
