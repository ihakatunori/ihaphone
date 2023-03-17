/*This file is released under the license of IHA License Ver 1.0.
The copyright of this file belongs to Katunori Iha (ihakatunori@gmail.com).

[再配布条件]
　・商用利用する場合は著作権者の許諾を必要とする。

[利用者条件]
　・商用利用する場合は著作権者の許諾を必要とする。


*/
#ifndef __IHA_ENC__
#define __IHA_ENC__
//
// 定数の定義
//
#define	IHA_DUMMY_LEN		2		// ダミーデータサイズ域長（２バイト固定）
#define	IHA_IV_LEN		32		// 初期化ベクトルサイズ（３２バイト固定）

#define	HASH_NONE		0		// ハッシュなし
#define	HASH_SHA_256		20256		// SHA2の２５６ビットハッシュ関数
#define	HASH_SHA_512		20512		// SHA2の５１２ビットハッシュ関数
#define	HASH_SHA3_256		30256		// SHA3の２５６ビットハッシュ関数
#define	HASH_SHA3_384		30384		// SHA3の３８４ビットハッシュ関数
#define	HASH_SHA3_512		30512		// SHA3の５１２ビットハッシュ関数
//
// コンテキストの定義
//
typedef struct __iha_enc_ctx{
__uint16_t	*com_last1;			// 最終ブロックの並べ替えテーブル1
__uint16_t	*com_last2;			// 最終ブロックの並べ替えテーブル2
__uint16_t	*com_last3;			// 最終ブロックの並べ替えテーブル3
__uint16_t	*com_last4;			// 最終ブロックの並べ替えテーブル4
__uint16_t	*com_last5;			// 最終ブロックの並べ替えテーブル5
//__uint16_t	*com_else;			// 最終ブロック以外の並べ替えテーブル
int		blk_len;			// 閾値（最終ブロック以外のブロック長）
int		hash_len;			// ハッシュ値のサイズ（バイト）
int		hash_kind;			// ハッシュ関数の種別
int		multi;				// ラウンド数（２〜５）
int		iv_len;				// 初期化ベクトルサイズ（バイト）
int		max_size;			// 暗号化できる最大サイズ（バイト）
int		bits;				// 転置の最小単位（ビット数 8固定）
__uint64_t	com_mask;			// comを保護する
unsigned char	*shared_key;			// 共通キー
unsigned char	*random_cond;			// 乱数の内部状態
unsigned char	*enc_iv;			// 初期化ベクトル
unsigned char	*dec_iv;			// 初期化ベクトル
unsigned char	*enc_md;			// ハッシュ値計算ワーク域1
unsigned char	*dec_md1;			// ハッシュ値計算ワーク域1
unsigned char	*dec_md2;			// ハッシュ値計算ワーク域2
//unsigned char	*blk_work1;			// ブロック転置用作業域1
//unsigned char	*blk_work2;			// ブロック転置用作業域2
unsigned char	*enc_work;			// 暗号化用作業域
unsigned char	*dec_work1;			// 復号化用作業域1
unsigned char	*dec_work2;			// 復号化用作業域2
} IHA_ENC_CTX;

#define LAST_BLK_LEN1(x)	(x->blk_len + IHA_DUMMY_LEN + x->hash_len + x->iv_len)
#define LAST_BLK_LEN2(x)	(x->blk_len + IHA_DUMMY_LEN + x->hash_len + x->iv_len*2)
#define LAST_BLK_LEN3(x)	(x->blk_len + IHA_DUMMY_LEN + x->hash_len + x->iv_len*3)
#define LAST_BLK_LEN4(x)	(x->blk_len + IHA_DUMMY_LEN + x->hash_len + x->iv_len*4)
#define LAST_BLK_LEN5(x)	(x->blk_len + IHA_DUMMY_LEN + x->hash_len + x->iv_len*5)

#define	BPB(x)				(8/(x->bits))

#define	COM_LAST_LEN1(x)	(sizeof(__uint16_t)*(LAST_BLK_LEN1(x) * BPB(x)))
#define	COM_LAST_LEN2(x)	(sizeof(__uint16_t)*(LAST_BLK_LEN2(x) * BPB(x)))
#define	COM_LAST_LEN3(x)	(sizeof(__uint16_t)*(LAST_BLK_LEN3(x) * BPB(x)))
#define	COM_LAST_LEN4(x)	(sizeof(__uint16_t)*(LAST_BLK_LEN4(x) * BPB(x)))
#define	COM_LAST_LEN5(x)	(sizeof(__uint16_t)*(LAST_BLK_LEN5(x) * BPB(x)))
//
// プロットタイプ宣言
//
IHA_ENC_CTX * init_cipher(char *file,__uint16_t *com[6],int blk_len,
			int max_size,int hash_kind,int bits,int multi);
// 	暗号化実行コンテキストへのポインタを返す。異常時はNULLを返す。
// 	fileにはキーテーブルファイル名を指定する。
//	comは並べ替えテーブルへのポインタ。ユーザが展開した場合。
//		com[0] ---> com_last1
//		com[1] ---> com_last2
//		com[2] ---> com_last3
//		com[3] ---> com_last4
//		com[4] ---> com_last5
//		com[5] ---> com_else
// 	blk_lenは最終ブロック以外のブロックのサイズをバイト単位で指定する。
// 	max_sizeは暗号化出来る最大サイズをバイト単位で指定する。
// 	hash_kindはハッシュ関数の種類を指定する。
// 	bitsは転置の最小単位でビット数。8に固定。
//      multiはラウンド数で１〜５を指定する。

int encrypt_data(IHA_ENC_CTX *ctx,int len,unsigned char *bdata,int *alen,unsigned char *adata);
// 	ctxは暗号化実行コンテキストへのポインタ。
// 	lenには平文の長さを設定すること。
// 	bdataには平文を設定すること。
// 	alenには暗号化後のデータ長が格納される。
// 	adataには暗号化後のデータが格納される。
//	  バッファは最低でもlen＋blk_len + 2 + hash_len + 32*multiバイト以上確保すること。

int decrypt_data(IHA_ENC_CTX *ctx,int len,unsigned char *bdata,int *alen,unsigned char *adata);
// 	ctxは暗号化実行コンテキストへのポインタ。
// 	lenには復号するデータのデータ長を設定する。
// 	bdataには復号するデータを設定する。
// 	alenには復号した平文のデータ長が格納される。
// 	adataには復号した平文のデータが格納される。lenバイト以上確保すること。

void finish_cipher(IHA_ENC_CTX *ctx);
// 	ctxは暗号化実行コンテキストへのポインタ。

//
// リターンコードの定義
//
#define	IHA_RV_OK		 0		// 正常終了
#define	IHA_RV_CHECKSUM_ERROR	-1		// 誤り検出符号エラー（チェックサムエラー）
#define IHA_RV_NOT_SUPPORT	-2		// 未サポートサイズ 
#define	IHA_RV_MEMORY_ERROR	-3		// メモリ確保エラー
#define	IHA_RV_NO_KEY_FILE	-4		// キーファイルなし
#define	IHA_RV_INVALID_FORMAT	-5		// キーファイルフォーマットエラー
#define	IHA_RV_PARAM_ERROR	-6		// パラメータエラー
#define	IHA_RV_SYSTEM_ERROR	-7		// システム関連エラー
#define	IHA_RV_CONTRA		-100		// 内部矛盾（バグ）
#endif
