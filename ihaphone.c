/*
This file is released under the license of IHA License Ver 1.0.
The copyright of this file belongs to Katunori Iha (ihakatunori@gmail.com).

How to compile

sudo apt install libgtk-3-dev libasound2-dev libjpeg-dev libssl-dev libgmp-dev

gcc -c iha.c -o iha.o -Wall -I./
gcc -Wall -D_REENTRANT iha_phone.c iha.o -o iha_phone -I./ 
	`pkg-config --cflags --libs gtk+-3.0` 
	-lasound  -lpthread -ljpeg -lX11 -lcrypto -lgmp -lm -lz
*/
/*****************************************************************************************/
/* インクルード*/
/*****************************************************************************************/
#define	_GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/videodev2.h>
#include <alsa/asoundlib.h>
#include <endian.h>
#include <signal.h>
#include <termios.h>
#include <libintl.h>
#include <cairo/cairo.h>
#include <jpeglib.h>
#include <jerror.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>		// XInitThreads
#include <pthread.h>
#include <setjmp.h>
#include <math.h>
#include <gmp.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <zlib.h>
#include <assert.h>
#include <limits.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>


#include "iha.h"
/*****************************************************************************************/
/* 定数定義*/
/*****************************************************************************************/
#define			PORT_NUM		55123
#define			INIT_MAX_SEND_FPS	3
#define			WIDTH			1280	//3840	//1920	//1280
#define			HEIGHT			720	//2160	//1080	//720
#define			MAX_JPEG_SIZE		3840*2160*3/3
#define			SECOND_BUF_SIZE		1024*300
#define			MIN_JPEG_SIZE		11346	// 720pで白紙、画質０のJPGファイルサイズ
#define			EDIT_BUF_SIZE		3840*2160*4
#define			FILE_BUF_SIZE		1024*20
#define			READ_BUF_SIZE		2048
#define			SCAN_LINE_SIZE		7680*3
#define			CMD_BUF_SIZE		4096*2
#define			SOUND_RATE		4000
#define			CHUNK_NO		9
#define			SOUND_CH_NO		1
#define			MAX_KEY_LIST		10000
#define			MAX_KEY_NAME_LEN	127
#define			MAX_CLI			49
#define			CLICK_NO		100
#define			SESS_TIME_OUT		60*2	// セッション時のタイムアウト（６０秒）
#define			DENY_TIME_OUT		10*2	// 切断してから着信を無視する時間（１０秒）
#define			CALL_TIME_OUT		60*2	// 発信の継続時間（６０秒）
#define			MAX_SOUND_THREADS	25
#define			MENU_HEIGHT		30

#define			V4L2_PALETTE_COUNT_MAX 21
#define 		MMAP_BUFFERS            4
#define 		MIN_MMAP_BUFFERS        2
//
// 安全素数とその原始元の１個
//
#define SPSTR  (\
"1907970075244390738074680429695291736693569947499401773947418826735289797870050537063680498355149002"\
"4430349595495070972576218631122414882881192021690454220696074466616936422119528953843684539025016866"\
"3932838805192055137154390912666527533007309292687539092257043362517857366624699975402375462954490293"\
"2592333031373306435315565397399219262014386064390200751747230290568382725050515719675946083500634044"\
"9597766065626902082396082556701234418990892795664601199805798854863010763738099351982658238978188813"\
"5705408653045219655801758081251164080554609057468028203308718724654081055323215860189611391296030471"\
"1084431467456719677663089258585472715073115637651710083182486471100976148903135628565417841548817431"\
"4603390960273794738505535596033185561454090008145637865906837031726769698000118775099549109035010841"\
"7050917991562167972281070161305972518044872048331306383715094854938415738549894606070722584737978176"\
"6864221343545269894430283536440371873753853978382595118331664161343236956603676768977222879187734209"\
"6898232608902615003151542416546211133752743115489066632737492144627683356451977679763387550354866509"\
"3914556482031482248883127023777039667707976559857333357013727342079099064400455741830654320379350833"\
"236245819348824064783585692924881021978332974949906122664421376034687815379012379"\
)

#define GENSTR (\
"1837465690982672516374549593837763623552224424253347484987654325678909876544339847650548547736234464"\
"6464"\
)

#define BASE 			10
//
// 送信データ種別
//
#define PRO_SEND_VOICE			0x00
#define	PRO_SEND_IMG			0x01
#define PRO_SEND_IMG_SUB		0x02
#define PRO_SEND_VOICE_NO_CRYPTO	0x10
#define	PRO_SEND_IMG_NO_CRYPTO		0x11
#define PRO_SEND_IMG_SUB_NO_CRYPTO	0x12
#define	PRO_SEND_SESSION		0x30
//
// イベント
//
#define	CMD_START_SESSION	0x20
#define CMD_SEND_REQ		0x21
#define CMD_RESP_REQ		0x22
#define CMD_SEND_A		0x30
#define CMD_RESP_A		0x31
#define CMD_SEND_B		0x32
#define CMD_RESP_B		0x33
#define CMD_SEND_X		0x34
#define CMD_RESP_X		0x35
#define CMD_SEND_Y		0x36
#define CMD_RESP_Y		0x37
#define CMD_SEND_KEY		0x40
#define CMD_RESP_KEY		0x41
#define CMD_TIMEOUT		0x50
#define CMD_AUTH_NG		0x51
#define CMD_ACCEPT		0x61
//
// ステータス
//
#define	STATUS_IDLE		0
//#define	STATUS_WAIT_REQ		1
#define	STATUS_WAIT_RESP_REQ	2
#define	STATUS_WAIT_ACCEPT	1
#define	STATUS_WAIT_A		3
#define	STATUS_WAIT_RESP_A	4
#define	STATUS_WAIT_B		5
#define	STATUS_WAIT_RESP_B	6
#define	STATUS_WAIT_X		7
#define	STATUS_WAIT_RESP_X	8
#define	STATUS_WAIT_Y		9
#define	STATUS_WAIT_RESP_Y	10
#define	STATUS_WAIT_KEY		11
#define	STATUS_WAIT_RESP_KEY	12
/*****************************************************************************************/
/* 構造体定義*/
/*****************************************************************************************/
typedef struct _keylist_t
{
	struct	_keylist_t	*next;
	int			Kind;
	int			SaveKind;
#define 		KIND_ALLOW	0
#define 		KIND_DENY	1
#define 		KIND_HOT	2
#define 		KIND_CALLING	3
	int			DenyCnt;		// Deny Counter
	int			CallCnt;		// Calling Counter
	EVP_CIPHER_CTX 		*en;
	EVP_CIPHER_CTX 		*ed;
	IHA_ENC_CTX		*ctx;
	pthread_mutex_t		m_ctx_enc;
//	pthread_mutex_t		m_ctx_dec;
	unsigned char		aes_key[60];
	char			Ip[64];
	char			Name[MAX_KEY_NAME_LEN+1];
	char			KeyFileName[513];	// file len(255) + dir len(255) + /(1) + /(1) + null(1)
} KEYLIST;

typedef struct _szoom_t
{
	int			used;			// 0 --- 空き　1 --- 使用中
	int			startx;
	int			starty;
} SZOOM;

typedef struct _recvlist_t
{
	struct	_recvlist_t	*next;
	int			background;		// 0 -- forgrand
							// 1 -- background
	int			startx;
	int			starty;
	int			disp_no;		// 表示域番号０~
	int			DispMark;		// 次に表示する印
	struct	timespec	last_recv_time;		// 音声または画像データの最終受信時刻
	struct	timespec	last_recv_time2;	// 画像データの最終受信時刻
	char			*bufp;
	char			*image_buf;
	char			*image_buf2;		// セカンドバッファ
	int			total;
	int			total2;
	int			ImageRecv;		// JPG受信状態
							// 0   -- 初期状態
							// 1   -- 受信完了
							// 2   -- 描画完了
							// 100 -- 受信データなし
	unsigned long		SurfWritedCounter;	// Surfへ書き込み回数（セッション間）
	unsigned long		TotalSurfWritedCounter;	// Surfへ書き込み回数（トータル）
	int			added;
	cairo_surface_t		*surf;
	unsigned char		*writep;		// PCM recv data buffer pointer
	unsigned char		*readp;			// PCM recv data buffer pointer
	unsigned char		*pcm_buf;		// PCM recv data buffer
	unsigned char		*tmp_pcm_buf;		// PCM recv data buffer
	pthread_mutex_t		m_write;
	pthread_t		tid;			// SOUND WRIET threadのTID
	int			exit_thread_flg;	// スレッド終了指示フラグ
	int			sleepflg;		// バッファリング用
	int			init_complete;		// 初期化完了フラグ
	snd_pcm_t		*handle;		// SOUND書き込み用
	int			recflg;
	struct	timespec	recv_time;		// 着信時間情報
	FILE			*wave_fp;
	size_t 			data_chunk_pos;
	int			ch;			// 0 --- main ch
							// 1 --- sub ch
	char			Ip[64];
	
} RECVLIST;

typedef struct _sendlist_t
{
	int			used;			// 0 --- empty
							// 1 --- used
	int			Sts;			// 0 --- OK
							// 1 --- NG
	int			sock;
	unsigned char		crypto;			// 送信データの種別
	unsigned long		image;			// imageの送信回数
	struct	addrinfo	hints;
	struct	addrinfo	*res;
	pthread_mutex_t		m_sock;
	char			Ip[64];			// IP address
} SENDLIST;

typedef struct _reject_t
{
	struct	_reject_t	*next;			// next linked.
	char			Ip[64];
} REJECTLIST;
typedef struct _rec_taiseki_t
{
	struct	timespec	time;			// 着信時刻
	char			Ip[64];
} RECTAISEKI;
/*****************************************************************************************/
/* 関数定義*/
/*****************************************************************************************/
static	void	OKDialog(char *message);
static void cb_connect (GtkWidget *button, gpointer user_data);
static void destroy_capture( GtkWidget *widget, gpointer   data );
static	cairo_surface_t	*make_cairo_surface(RECVLIST *list,void *data, size_t len,int initflg);
static	void	*write_thread(void *Param);
static	void	disconnector(void);
static	snd_pcm_t *init_sound(snd_pcm_stream_t fmt);
static	char	*get_hot_ip();
static	void	do_session(unsigned char *data,char *ip,int size);
static	void	check_click();
//static	int 	DataDeCompress(unsigned char *in,int insize,unsigned char *out,int *outsize);
static	void	p_message(char *mess,char *name);
gboolean Refresh(gpointer data);
gboolean cb_draw( GtkWidget *widget, cairo_t *cr, gpointer user_data);
void	cb_realize_dummy( GtkWidget *widget, gpointer user_data);
void	cb_size_allocate( GtkWidget *widget, GdkRectangle *allocation, gpointer user_data);
static void	set_area(int	flg);
static void destroy_win( GtkWidget *widget, gpointer   data );
/*****************************************************************************************/
/* 変数定義*/
/*****************************************************************************************/
static	SENDLIST	SendArray[MAX_CLI]={0};
static	RECTAISEKI	RecArray[32]={0};

static	SZOOM	ent_table1[1];
static	SZOOM	ent_table2[2];
static	SZOOM	ent_table3[4];
static	SZOOM	ent_table4[4];
static	SZOOM	ent_table9[9];
static	SZOOM	ent_table16[16];
static	SZOOM	ent_table25[25];
static	SZOOM	ent_table36[36];
static	SZOOM	ent_table49[49];

/*
static	SZOOM	ent_table1[1]={
{0,0,0}
};

static	SZOOM	ent_table4[4]={
{0,WIDTH*0,HEIGHT*0},
{0,WIDTH*1,HEIGHT*0},
{0,WIDTH*0,HEIGHT*1},
{0,WIDTH*1,HEIGHT*1}
};

static	SZOOM	ent_table9[9]={
{0,WIDTH*0,HEIGHT*0},
{0,WIDTH*1,HEIGHT*0},
{0,WIDTH*2,HEIGHT*0},
{0,WIDTH*0,HEIGHT*1},
{0,WIDTH*1,HEIGHT*1},
{0,WIDTH*2,HEIGHT*1},
{0,WIDTH*0,HEIGHT*2},
{0,WIDTH*1,HEIGHT*2},
{0,WIDTH*2,HEIGHT*2}
};

static	SZOOM	ent_table16[16]={
{0,WIDTH*0,HEIGHT*0},
{0,WIDTH*1,HEIGHT*0},
{0,WIDTH*2,HEIGHT*0},
{0,WIDTH*3,HEIGHT*0},
{0,WIDTH*0,HEIGHT*1},
{0,WIDTH*1,HEIGHT*1},
{0,WIDTH*2,HEIGHT*1},
{0,WIDTH*3,HEIGHT*1},
{0,WIDTH*0,HEIGHT*2},
{0,WIDTH*1,HEIGHT*2},
{0,WIDTH*2,HEIGHT*2},
{0,WIDTH*3,HEIGHT*2},
{0,WIDTH*0,HEIGHT*3},
{0,WIDTH*1,HEIGHT*3},
{0,WIDTH*2,HEIGHT*3},
{0,WIDTH*3,HEIGHT*3}
};

static	SZOOM	ent_table25[25]={
{0,WIDTH*0,HEIGHT*0},
{0,WIDTH*1,HEIGHT*0},
{0,WIDTH*2,HEIGHT*0},
{0,WIDTH*3,HEIGHT*0},
{0,WIDTH*4,HEIGHT*0},
{0,WIDTH*0,HEIGHT*1},
{0,WIDTH*1,HEIGHT*1},
{0,WIDTH*2,HEIGHT*1},
{0,WIDTH*3,HEIGHT*1},
{0,WIDTH*4,HEIGHT*1},
{0,WIDTH*0,HEIGHT*2},
{0,WIDTH*1,HEIGHT*2},
{0,WIDTH*2,HEIGHT*2},
{0,WIDTH*3,HEIGHT*2},
{0,WIDTH*4,HEIGHT*2},
{0,WIDTH*0,HEIGHT*3},
{0,WIDTH*1,HEIGHT*3},
{0,WIDTH*2,HEIGHT*3},
{0,WIDTH*3,HEIGHT*3},
{0,WIDTH*4,HEIGHT*3},
{0,WIDTH*0,HEIGHT*4},
{0,WIDTH*1,HEIGHT*4},
{0,WIDTH*2,HEIGHT*4},
{0,WIDTH*3,HEIGHT*4},
{0,WIDTH*4,HEIGHT*4}
};


static	SZOOM	ent_table36[36]={
{0,WIDTH*0,HEIGHT*0},
{0,WIDTH*1,HEIGHT*0},
{0,WIDTH*2,HEIGHT*0},
{0,WIDTH*3,HEIGHT*0},
{0,WIDTH*4,HEIGHT*0},
{0,WIDTH*5,HEIGHT*0},
{0,WIDTH*0,HEIGHT*1},
{0,WIDTH*1,HEIGHT*1},
{0,WIDTH*2,HEIGHT*1},
{0,WIDTH*3,HEIGHT*1},
{0,WIDTH*4,HEIGHT*1},
{0,WIDTH*5,HEIGHT*1},
{0,WIDTH*0,HEIGHT*2},
{0,WIDTH*1,HEIGHT*2},
{0,WIDTH*2,HEIGHT*2},
{0,WIDTH*3,HEIGHT*2},
{0,WIDTH*4,HEIGHT*2},
{0,WIDTH*5,HEIGHT*2},
{0,WIDTH*0,HEIGHT*3},
{0,WIDTH*1,HEIGHT*3},
{0,WIDTH*2,HEIGHT*3},
{0,WIDTH*3,HEIGHT*3},
{0,WIDTH*4,HEIGHT*3},
{0,WIDTH*5,HEIGHT*3},
{0,WIDTH*0,HEIGHT*4},
{0,WIDTH*1,HEIGHT*4},
{0,WIDTH*2,HEIGHT*4},
{0,WIDTH*3,HEIGHT*4},
{0,WIDTH*4,HEIGHT*4},
{0,WIDTH*5,HEIGHT*4},
{0,WIDTH*0,HEIGHT*5},
{0,WIDTH*1,HEIGHT*5},
{0,WIDTH*2,HEIGHT*5},
{0,WIDTH*3,HEIGHT*5},
{0,WIDTH*4,HEIGHT*5},
{0,WIDTH*5,HEIGHT*5}
};

static	SZOOM	ent_table49[49]={
{0,WIDTH*0,HEIGHT*0},
{0,WIDTH*1,HEIGHT*0},
{0,WIDTH*2,HEIGHT*0},
{0,WIDTH*3,HEIGHT*0},
{0,WIDTH*4,HEIGHT*0},
{0,WIDTH*5,HEIGHT*0},
{0,WIDTH*6,HEIGHT*0},
{0,WIDTH*0,HEIGHT*1},
{0,WIDTH*1,HEIGHT*1},
{0,WIDTH*2,HEIGHT*1},
{0,WIDTH*3,HEIGHT*1},
{0,WIDTH*4,HEIGHT*1},
{0,WIDTH*5,HEIGHT*1},
{0,WIDTH*6,HEIGHT*1},
{0,WIDTH*0,HEIGHT*2},
{0,WIDTH*1,HEIGHT*2},
{0,WIDTH*2,HEIGHT*2},
{0,WIDTH*3,HEIGHT*2},
{0,WIDTH*4,HEIGHT*2},
{0,WIDTH*5,HEIGHT*2},
{0,WIDTH*6,HEIGHT*2},
{0,WIDTH*0,HEIGHT*3},
{0,WIDTH*1,HEIGHT*3},
{0,WIDTH*2,HEIGHT*3},
{0,WIDTH*3,HEIGHT*3},
{0,WIDTH*4,HEIGHT*3},
{0,WIDTH*5,HEIGHT*3},
{0,WIDTH*6,HEIGHT*3},
{0,WIDTH*0,HEIGHT*4},
{0,WIDTH*1,HEIGHT*4},
{0,WIDTH*2,HEIGHT*4},
{0,WIDTH*3,HEIGHT*4},
{0,WIDTH*4,HEIGHT*4},
{0,WIDTH*5,HEIGHT*4},
{0,WIDTH*6,HEIGHT*4},
{0,WIDTH*0,HEIGHT*5},
{0,WIDTH*1,HEIGHT*5},
{0,WIDTH*2,HEIGHT*5},
{0,WIDTH*3,HEIGHT*5},
{0,WIDTH*4,HEIGHT*5},
{0,WIDTH*5,HEIGHT*5},
{0,WIDTH*6,HEIGHT*5},
{0,WIDTH*0,HEIGHT*6},
{0,WIDTH*1,HEIGHT*6},
{0,WIDTH*2,HEIGHT*6},
{0,WIDTH*3,HEIGHT*6},
{0,WIDTH*4,HEIGHT*6},
{0,WIDTH*5,HEIGHT*6},
{0,WIDTH*6,HEIGHT*6}
};
*/

static	int			SessDialog_delete=0;
static	int			allow_no_crypto=0;
								// 0 --- 暗号なしデータの着信を許可しない
								// 1 --- 暗号なしデータの着信を許可する
#define	MODE_MULTI		1
#define	MODE_SINGLE		0
static	int			mode = MODE_MULTI;
static	mpz_t			SP,G,A,a,B,b,X,x,Y,y,e,d,W1,W2,W3,W4,seed;
static	GtkWidget   		*Sdialog=NULL;
static	GtkWidget		*wait_dialog = NULL;
static	GtkWidget		*Wdialog=NULL;
static	char 			DialogMessage[256]={0};
static	char			MessAuthCode[32];
static	int			iconified=0;
static	cairo_t 		*gcr=NULL;
static	cairo_surface_t 	*surf=NULL;			// 初期画面
static	cairo_surface_t 	*surf2=NULL;			// イメージなし画像
static	cairo_surface_t 	*surf3=NULL;			// 3画面表示の両サイド画像
static	cairo_surface_t 	*surf4=NULL;			// 2画面表示のイメージなし画像
#if GTK_CHECK_VERSION(3,22,0)
static	cairo_region_t 		*clip = NULL;
static	GdkDrawingContext	*dc=NULL;
#endif


static	pthread_t		tid1=-1,tid2=-1,tid3=-1,tid4=-1,tid5=-1,tid6=-1,tid7=-1,tid8=-1,tid9=-1;
static	GtkCssProvider 		*provider=NULL;
static	GtkCssProvider 		*provider2=NULL;
static	GtkWidget 		*vbox=NULL;
static	GtkWidget 		*hbox=NULL;
static	GtkWidget		*window=NULL;			// メインウィンドウ
static	GtkWidget 		*list_dialog=NULL;		// リスト表示ダイアログ
static	GtkWidget		*window3=NULL;			// キャプチャウィンド
static	GtkWidget 		*drawingarea=NULL;
static	GtkWidget 		*treeview=NULL;
static	GtkWidget		*button_connect=NULL;
static	GtkWidget		*button_disconnect=NULL;
static	GtkWidget		*button_respond=NULL;
static	GtkWidget		*button_set=NULL;
static	GtkWidget		*button_hot=NULL;
static	GtkWidget		*button_key=NULL;
static	GtkWidget		*button_cap=NULL;
static	GtkWidget		*button_lic=NULL;
static	GtkWidget		*button_exit=NULL;
static	GtkWidget 		*button_com=NULL;
static	GtkWidget		*button_rec=NULL;

static	double			zoom_cnt=0.0;		// 拡大の段階
static	double			mag=(double)1.2;	// 拡大率

static	int			width = WIDTH;		// 表示画面のX方向解像度
static	int			height = HEIGHT;	// 表示画面のY方向解像度
static	int			camera_width = 1920;	// USBカメラのX方向解像度
static	int			camera_height = 1080;	// USBカメラのY方向解像度
static	int			cur_sess_no=0;		// 接続中のクライアント数
static	int			voice_no=0;		// 接続中の音声付きクライアント数
static	int			max_sound_threads=MAX_SOUND_THREADS;

//#define	CRYPTO_NONE		1
//#define	CRYPTO_AES		2	
//#define	CRYPTO_IHA		3	
//static	int			cryptoflg = CRYPTO_AES; // 暗号化指示
static	int			cameraflg = 0;		// カメラが動作中か
#define	COMPRESS_YUV422		0
#define	COMPRESS_MJPEG		1
static	int			camera_compress=COMPRESS_YUV422;
static	int			exitflg = 0;		// スレッド終了指示
static	int			recflg=0;		// 録画フラグ
static	gboolean		Ignoreflg=FALSE;	// コンタクトを無視するか
static	int			chgflg=1;		// 0 -- 初期画面の表示不要
							// 1 -- 初期画面の表示要
static	int			Refresh_reg_flg = 0;	// 画面描画関数の登録フラグ
static	int			init_image_flg=0;
static	int			HassinNo = 0;
static	int			outouflg = 0;
//static	int			mousetogle=0;
static	int			PinPmode=0;
#define	DISP_TEXT		0
#define	DISP_IMAGE		1
static	int			PinPDispMode=DISP_TEXT;
static	int			FullFlg=0;
//static	int			my_event_flg=0;

static	long			rfps   = 0;
static	long			b_rfps = 0;
static	long			sfps   = 0;
static	long			b_sfps = 0;
static	long			rvfps = 0;
static	long			b_rvfps = 0;
static	long			svfps = 0;
static	long			b_svfps = 0;
static	long			draws = 0;
static	long			b_draws = 0;
static	long			VoiceWriteTotall=0;
static	long			VoiceWriteCnt=1;
static	long			VoiceReadTotall=0;
static	long			VoiceReadCnt=1;

static	unsigned char		*init_image_buf;
static	unsigned char		*edit_buffer;
static	unsigned char		*edit_buffer2;
static	unsigned char		*edit_buffer3;
static	unsigned char		*dest_buffer;
static	unsigned char		*dest_buffer2;
static	unsigned char 		*jpeg_scanline;
static	unsigned char 		*jpeg_scanline2;
static	pthread_mutex_t 	m_recv_list 	= PTHREAD_MUTEX_INITIALIZER;
static	pthread_mutex_t 	m_send_list 	= PTHREAD_MUTEX_INITIALIZER;
static	pthread_mutex_t 	m_init_sound 	= PTHREAD_MUTEX_INITIALIZER;
static	pthread_mutex_t 	m_rec_button 	= PTHREAD_MUTEX_INITIALIZER;
static	int			MaxSendFps=INIT_MAX_SEND_FPS;
static	int			SendFps=0;
static	int			CaptureFps=10;
static	int			CaptureQuarity=90;
static	int			ImageQuality=20;
//static	int			MaxDraws=1500;
static	KEYLIST			*key_list_top=NULL;
static	RECVLIST		*recv_list_top=NULL;
static	REJECTLIST		*reject_list_top=NULL;
static	char			hot_ip[64]={0};
static	char			*my_ip="127.0.0.1";
static	int			clicked_no=0;
static 	int			max_key_list_no=MAX_KEY_LIST;
static char			*selected_list;
static char			camera_device[32]={0};
static	int			index_palette;
// キー共有
static	int			St = STATUS_IDLE;
static	int			Ev = 0;
static	char			SessionIp[64]={0};
static	int			TimerValue=0;

static	int			key_entry_no=0;
static	char			key_file_dir[257]={0};
static	char			rec_file_dir[257]={0};
static	char			addr_book_file[257]={0};

static	pthread_t		tid_capture;
static	int			stop_capture=0;
static	int			capture_flg=0;

static	FILE			*ffp=NULL;
static	time_t 			g_timer=0;
static	time_t			rec_start_time=0;
static	char			*cmdbuf=NULL;

struct	timespec		befor_disconnector;
/*****************************************************************************************/
/* 画像処理用構造体定義	*/
/*****************************************************************************************/
typedef struct video_image_buff {
    unsigned char 		*ptr;
    int 			content_length;
    size_t 			size;           	// total allocated size 
    size_t 			used;                  	// bytes already used 
    struct timeval 		image_time;      	// time this image was received 
} video_buff;

typedef struct {
    struct v4l2_requestbuffers 	req;
    struct v4l2_buffer 		buf;
    video_buff 			*buffers;
    int 			pframe;
} src_v4l2_st;
/*****************************************************************************************/
/* 音声制御用変数定義*/
/*****************************************************************************************/
struct hwparams_st{
	snd_pcm_format_t 	format;
	unsigned int		channels;
	unsigned int		rate;
};
static struct	hwparams_st	hwparams;
static unsigned			period_time = 0;
static unsigned			buffer_time = 0;
static size_t			bits_per_sample = 0;
static size_t			bits_per_frame = 0;
static size_t			chunk_bytes = 0;
static snd_pcm_uframes_t	chunk_size = 0;
static snd_pcm_uframes_t	period_frames = 0;
static snd_pcm_uframes_t	buffer_frames = 0;
//static snd_output_t		*loger;
static snd_pcm_t		*handle2=NULL;		// キャプチャー用

#define		SOUND_BUFSIZE	chunk_bytes*100
/*****************************************************************************************/
/* CSS	*/
/*****************************************************************************************/
static  char    css_data[]={
#if GTK_CHECK_VERSION(3,22,0)
"button:hover {\n"
#else
".button:hover {\n"
#endif
"    color: red;\n"
"    background-color: blue;\n"
"    opacity: 0.95;\n"
"    margin-top: 0px;\n"
"    margin-bottom: 0px;\n"
"    margin-left: 0px;\n"
"    margin-right: 0px;\n"
"    border-radius: 7px;\n"
"    border: 0px;\n"
"    box-shadow: 0px 0px;\n"
"    border-width: 0px;\n"
"    padding: 0px;\n"
"    background-image:  none;\n"
"}\n"
"\n"
#if GTK_CHECK_VERSION(3,22,0)
"button:active{\n"
#else
".button:active{\n"
#endif
"    color: white;\n"
"    background-color: red;\n"
"    opacity: 0.97;\n"
"    margin-top: 0px;\n"
"    margin-bottom: 0px;\n"
"    margin-left: 0px;\n"
"    margin-right: 0px;\n"
"    border-radius: 7px;\n"
"    border: 0px;\n"
"    box-shadow: 0px 0px;\n"
"    border-width: 0px;\n"
"    padding: 0px;\n"
"    background-image:  none;\n"
"}\n"
"\n"
#if GTK_CHECK_VERSION(3,22,0)
"button {\n"
#else
".button {\n"
#endif
"    color: white;\n"
"    background-color: blue;\n"
"    opacity: 0.797;\n"
"    margin-top: 0px;\n"
"    margin-bottom: 0px;\n"
"    margin-left: 0px;\n"
"    margin-right: 0px;\n"
"    border-radius: 7px;\n"
"    border: 0px;\n"
"    box-shadow: 0px 0px;\n"
"    border-width: 0px;\n"
"    padding: 0px;\n"
"    background-image:  none;\n"
"}\n"
"\n"
};


static  char    css_data2[]={
#if GTK_CHECK_VERSION(3,22,0)
"button:hover {\n"
#else
".button:hover {\n"
#endif
"    color: white;\n"
"    background-color: red;\n"
"    opacity: 0.95;\n"
"    margin-top: 0px;\n"
"    margin-bottom: 0px;\n"
"    margin-left: 0px;\n"
"    margin-right: 0px;\n"
"    border-radius: 7px;\n"
"    border: 0px;\n"
"    box-shadow: 0px 0px;\n"
"    border-width: 0px;\n"
"    padding: 0px;\n"
"    background-image:  none;\n"
"}\n"
"\n"
#if GTK_CHECK_VERSION(3,22,0)
"button:active{\n"
#else
".button:active{\n"
#endif
"    color: white;\n"
"    background-color: red;\n"
"    opacity: 0.97;\n"
"    margin-top: 0px;\n"
"    margin-bottom: 0px;\n"
"    margin-left: 0px;\n"
"    margin-right: 0px;\n"
"    border-radius: 7px;\n"
"    border: 0px;\n"
"    box-shadow: 0px 0px;\n"
"    border-width: 0px;\n"
"    padding: 0px;\n"
"    background-image:  none;\n"
"}\n"
"\n"
#if GTK_CHECK_VERSION(3,22,0)
"button {\n"
#else
".button {\n"
#endif
"    color: white;\n"
"    background-color: red;\n"
"    opacity: 0.797;\n"
"    margin-top: 0px;\n"
"    margin-bottom: 0px;\n"
"    margin-left: 0px;\n"
"    margin-right: 0px;\n"
"    border-radius: 7px;\n"
"    border: 0px;\n"
"    box-shadow: 0px 0px;\n"
"    border-width: 0px;\n"
"    padding: 0px;\n"
"    background-image:  none;\n"
"}\n"
"\n"
};

/*****************************************************************************************/
/* 処理コード*/
/*****************************************************************************************/
/*static        void    dump(unsigned char *p,int len)
{
        int     id;

        printf("***");
        for(id=0;id < len;id++){
                printf("%02x",(unsigned char)*p);
                p++;
        }
        printf("***\n");
}*/
static void	my_strncpy(char *dest, const char *src, size_t n)
{
	strncpy(dest,src,n);

	if(strlen(src) > n){
		*(dest + n -1) = 0x00;
	}
	return;
}
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
		fprintf(stderr,"naibu error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	EVP_DigestUpdate(mdctx, data, len);
	EVP_DigestFinal_ex(mdctx, hash, &digest_len);
	EVP_MD_CTX_destroy(mdctx);
	return;
}
static	int	aes_enc(KEYLIST *list,int size,unsigned char *bdata,int *alen,unsigned char *adata)
{
//	EVP_CIPHER_CTX 		*en;
	unsigned char           aes_key[32],aes_iv[12],aes_aad[16],aes_tag[16];
	int			sz,tmplen;

	memcpy(aes_key,list->aes_key,32);
	memcpy(aes_iv,list->aes_key+32,12);
	memcpy(aes_aad,list->aes_key+32+12,16);

//	en = EVP_CIPHER_CTX_new();
	EVP_EncryptInit_ex(list->en, EVP_aes_256_gcm(), NULL, aes_key, aes_iv);
	EVP_CIPHER_CTX_ctrl(list->en, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL);

	EVP_EncryptUpdate(list->en, NULL, &sz, aes_aad, 16);
	EVP_EncryptUpdate(list->en,adata, &sz, bdata, size);

	EVP_EncryptFinal_ex(list->en,adata , &tmplen);
	EVP_CIPHER_CTX_ctrl(list->en, EVP_CTRL_AEAD_GET_TAG, 16, aes_tag);

//	EVP_CIPHER_CTX_free(en);

	memcpy((char *)(adata + sz),(char *)aes_tag,16);

	sz += 16;
	*alen = sz;

	return 0;
}
static	int	aes_dec(KEYLIST *list,int size,unsigned char *bdata,int *alen,unsigned char *adata)
{
//	EVP_CIPHER_CTX 		*ed;
	unsigned char           aes_key[32],aes_iv[12],aes_aad[16];
	int			rv,tmplen,tmplen2;

	*alen = 0;

	memcpy(aes_key,list->aes_key,32);
	memcpy(aes_iv,list->aes_key+32,12);
	memcpy(aes_aad,list->aes_key+32+12,16);


//	de = EVP_CIPHER_CTX_new();
	EVP_DecryptInit_ex(list->ed, EVP_aes_256_gcm(), NULL, aes_key, aes_iv);
	EVP_CIPHER_CTX_ctrl(list->ed, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL);

	EVP_DecryptUpdate(list->ed, NULL, &tmplen, aes_aad, 16);
	EVP_DecryptUpdate(list->ed,adata, &tmplen, bdata, size - 16);

	rv = EVP_CIPHER_CTX_ctrl(list->ed, EVP_CTRL_AEAD_SET_TAG, 16,(void *)(bdata+size-16));
	if(rv <= 0){
//		EVP_CIPHER_CTX_free(de);
		fprintf(stderr,"EVP_CIPHER_CTX_ctrl error rv=%d.(file=%s line=%d)\n",rv,__FILE__,__LINE__);
		return -1;
	}

	rv = EVP_DecryptFinal_ex(list->ed,adata , &tmplen2);	// why tmplen2????
	if(rv <= 0){
//		EVP_CIPHER_CTX_free(de);
//		fprintf(stderr,"EVP_DecryptFinal_ex error rv=%d\n",rv);
		return -1;
	}

//	EVP_CIPHER_CTX_free(de);

	*alen = tmplen;

	return 0;
}
static	int encrypt_data_r(KEYLIST *list,int len,unsigned char *bdata,int *alen,unsigned char *adata)
{
	int	rv,len2;
	unsigned char 	work[1600];

	if(list == NULL){
		*alen = 0;
		return -101;
	}
	if(list->ctx == NULL){
		*alen = 0;
		return -102;
	}
	if(len > 1500){
		*alen = 0;
		return -103;
	}

	aes_enc(list,len,bdata,alen,work);
	if(*alen > 1500){
		*alen = 0;
		return -104;
	}
	len2 = *alen;
	rv = encrypt_data(list->ctx,len2,work,alen,adata);
	return rv;
}
static	int decrypt_data_r(KEYLIST *list,int len,unsigned char *bdata,int *alen,unsigned char *adata)
{
	int	rv;
	unsigned char 	work[1600];

	if(list == NULL){
		*alen = 0;
		return -110;
	}
	if(list->ctx == NULL){
		*alen = 0;
		return -111;
	}
	if(len > 1500){
		*alen = 0;
		return -112;
	}
	rv = decrypt_data(list->ctx,len,bdata,alen,work);
	if(rv < 0){
		*alen = 0;
		return rv;
	}
	if(*alen > 1500){
		*alen = 0;
		return -113;
	}
	aes_dec(list,*alen,work,alen,adata);

	return 0;
}
static	void	set_ent_tables(void)
{
	int	i,j;

	ent_table1[0].used   = 0;
	ent_table1[0].startx = 0;
	ent_table1[0].starty = 0;

	ent_table2[0].used   = 0;
	ent_table2[0].startx = 0;
	ent_table2[0].starty = 0;
	ent_table2[1].used   = 0;
//	ent_table2[1].startx = width;
	ent_table2[1].startx = width/2;
	ent_table2[1].starty = 0;

	ent_table3[0].used   = 0;
	ent_table3[0].startx = 0;
	ent_table3[0].starty = 0;
	ent_table3[1].used   = 0;
	ent_table3[1].startx = width;
	ent_table3[1].starty = 0;
	ent_table3[2].used   = 0;
	ent_table3[2].startx = width*2/4;
	ent_table3[2].starty = height;
	ent_table3[3].used   = 0;
	ent_table3[3].startx = width*6/4;	// for PinPmode
	ent_table3[3].starty = height;




	for(i=0;i<2;i++){
		for(j=0;j<2;j++){
			ent_table4[i*2+j].used   = 0;
			ent_table4[i*2+j].startx = width*j;
			ent_table4[i*2+j].starty = height*i;
		}
	}

	for(i=0;i<3;i++){
		for(j=0;j<3;j++){
			ent_table9[i*3+j].used   = 0;
			ent_table9[i*3+j].startx = width*j;
			ent_table9[i*3+j].starty = height*i;
		}
	}

	for(i=0;i<4;i++){
		for(j=0;j<4;j++){
			ent_table16[i*4+j].used   = 0;
			ent_table16[i*4+j].startx = width*j;
			ent_table16[i*4+j].starty = height*i;
		}
	}

	for(i=0;i<5;i++){
		for(j=0;j<5;j++){
			ent_table25[i*5+j].used   = 0;
			ent_table25[i*5+j].startx = width*j;
			ent_table25[i*5+j].starty = height*i;
		}
	}

	for(i=0;i<6;i++){
		for(j=0;j<6;j++){
			ent_table36[i*6+j].used   = 0;
			ent_table36[i*6+j].startx = width*j;
			ent_table36[i*6+j].starty = height*i;
		}
	}

	for(i=0;i<7;i++){
		for(j=0;j<7;j++){
			ent_table49[i*7+j].used   = 0;
			ent_table49[i*7+j].startx = width*j;
			ent_table49[i*7+j].starty = height*i;
		}
	}
}
/*****************************************************************************************/
/* インディアン処理*/
/*****************************************************************************************/
static  int     IsLittleEndian()
{
//#if defined(__LITTLE_ENDIAN__)
//        return 1;
//#elif defined(__BIG_ENDIAN__)
//        return 0;
//#else
        int i = 1;
        return (int)(*(char*)&i);
//#endif
}
static uint16_t swap16(uint16_t value)
{
    uint16_t ret;
    ret  = value << 8;
    ret |= value >> 8;
    return ret;
}
/*static uint32_t swap32(uint32_t value)
{
    uint32_t ret;

    ret  = value              << 24;
    ret |= (value&0x0000FF00) <<  8;
    ret |= (value&0x00FF0000) >>  8;
    ret |= value              >> 24;

    return ret;
}*/


#define __R (0)
#define __W (1)

#include <sys/types.h>
#include <sys/wait.h>

static struct pid {
struct pid *next;
	FILE *fp;
	pid_t pid;
} *pidlist;

FILE *popen_err(char *command,char *option)
{
	struct pid *cur;
	int pipe_c2p_e[2];
	int pid;
	FILE *fp;

	if ((*option != 'r') || option[1]){
		fprintf(stderr,"popen_err option error.(file=%s,line=%d)\n",__FILE__,__LINE__);
		return (NULL);
	}

	if ((cur = (struct pid *)malloc(sizeof(struct pid))) == NULL){
		fprintf(stderr,"malloc error.(file=%s,line=%d)\n",__FILE__,__LINE__);
		return (NULL);
	}

/* Create two of pipes. */
	if(pipe(pipe_c2p_e)<0){
		fprintf(stderr,"pipe error errno=%d.(file=%s,line=%d)\n",errno,__FILE__,__LINE__);
		return(NULL);
	}

/* Invoke processs */
	if((pid=fork())<0){
		fprintf(stderr,"fork error.(file=%s,line=%d)\n",__FILE__,__LINE__);
		close(pipe_c2p_e[__R]);
		close(pipe_c2p_e[__W]);
		return(NULL);
	}
	if(pid==0){ /* I'm child */
		close(pipe_c2p_e[__R]);
		dup2(pipe_c2p_e[__W],2);/* stderr */
		close(pipe_c2p_e[__W]);
		execlp("sh","sh","-c",command,NULL);
		_exit(127);
	}

	close(pipe_c2p_e[__W]);

	fp=fdopen(pipe_c2p_e[__R],option);

/* Link into list of file descriptors. */
	cur->fp = fp;
	cur->pid =pid;
	cur->next = pidlist;
	pidlist = cur;

	return(fp);
}

int pclose_err(FILE *fp)
{
	register struct pid *cur, *last;
#if BSD /* for BSD */
//	int omask;
#else /* SYSV */
	sigset_t set,omask;
#endif
	int pstat;
	pid_t pid;
	extern int errno;

	fclose(fp);

/* Find the appropriate file pointer. */
	for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next){
		if (cur->fp == fp){
			break;
		}
	}

	if (cur == NULL){
		return (-1);
	}
#if BSD /* for BSD */
/* Get the status of the process. */
//	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
//	do {
//		pid = waitpid(cur->pid, (int *) &pstat, 0);
//	} while (pid == -1 && errno == EINTR);
//	(void)sigsetmask(omask);
#else /* SYSV */
/* Get the status of the process. */
	sigemptyset(&set);
	sigaddset(&set,SIGINT);
	sigaddset(&set,SIGQUIT);
	sigaddset(&set,SIGHUP);
	sigprocmask(SIG_SETMASK,&set,&omask);
	do {
		pid = waitpid(cur->pid, (int *) &pstat, 0);
	} while (pid == -1 && errno == EINTR);
	sigprocmask(SIG_SETMASK,&omask,NULL);
#endif
/* Remove the entry from the linked list. */
	if (last == NULL){
		pidlist = cur->next;
	}else{
		last->next = cur->next;
	}

	free(cur);

	return (pid == -1 ? -1 : pstat);
}

static	int	my_system(char *cmd)
{
	FILE	*fp;
	char	buf[512];

	fp = popen_err(cmd,"r");
	if(fp == NULL){
		fprintf(stderr,"popen error errno=%d cmd=%s.(file=%s line=%d)\n",errno,cmd,__FILE__,__LINE__);
		return -1;
	}
	memset(buf,0x00,sizeof(buf));
	fread(buf,1,sizeof(buf)-1,fp);
	if((buf[0] != 0x00) && (buf[0] != '\n')){
		printf("%s\n",cmd);
		printf("%s\n",buf);
		pclose_err(fp);
		return -1;
	}
	if(pclose_err(fp) == -1){
		if(errno == ECHILD){
//printf("ECHILD cmd=%s\n",cmd);
			return 0;
		}
		fprintf(stderr,"pclose error errno=%d cmd=%s.(file=%s line=%d)\n",errno,cmd,__FILE__,__LINE__);
		return -1;
	}
	
	return 0;
}
static	int	min(int x,int y)
{
	if(x <= y){
		return x;
	}else{
		return y;
	}
}
static	void	GetResolution(int *x,int *y)
{
	char		buf[256],*str,*stop;
	FILE		*fp;

	if((fp = popen(" xdpyinfo | grep dimensions","r")) == NULL){
		fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		return;
	}
	fgets(buf,sizeof(buf),fp);
	if(pclose(fp) < 0){
		fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		return;
	}

	str = strtok_r(buf," x",&stop);
	str = strtok_r(NULL," x",&stop);
	if(str){
		*x = atoi(str);
	}
	str = strtok_r(NULL," x",&stop);
	if(str){
		*y = atoi(str);
	}
	return;
}
/*int	IsTrisquel(void)
{
	FILE	*fp;
	char	*str,buf[128],*stop;

	fp = fopen("/etc/lsb-release","r");
	if(fp == NULL){
		return 0;
	}
	fgets(buf,sizeof(buf),fp);
	fclose(fp);

	str = strtok_r(buf,"= \n",&stop);
	str = strtok_r(NULL,"= \n",&stop);
	if(str){
		if(strcmp(str,"Trisquel") == 0){
			return 1;
		}
	}
	return 0;
}

static	int	GetCpuThreadNo()
{
	FILE    *fp;
	char    buf[256];

	if((fp = popen("grep processor /proc/cpuinfo | wc -l","r")) == NULL){
		return 0;
	}
	fgets(buf, sizeof(buf), fp);
	pclose(fp);

	return atoi(buf);
}*/
static	void msgsendque(unsigned char *buf,char *ip,int len)
{
        int msqid;
        key_t msgkey;
        struct msgbuf{
                long mtype;
                char mdata[1500+64+sizeof(int)];
        };
        struct msgbuf msgdata,*p;

	if(len > sizeof(p->mdata)){
		len = sizeof(p->mdata);
	}

        p = &msgdata;
	p->mtype = 100;
	strcpy(p->mdata,ip);
	memcpy(&(p->mdata[64]),(char *)&len,sizeof(int));
	memcpy(&(p->mdata[64+sizeof(int)]),buf,len);

        msgkey = ftok(".",'X');
        msqid = msgget(msgkey,0666);
	msgsnd(msqid,p,sizeof(p->mdata),0);
}
/*****************************************************************************************/
/* IPチェック関数*/
/*****************************************************************************************/
static	int	IsIP(char *host)
{
	char	destbuf[32];

	if(inet_pton(AF_INET,host,destbuf) == 0){
		if(inet_pton(AF_INET6,host,destbuf) == 0){
			return 0;		// Not IP address
		}
	}

	return 1;				// IP address
}
static	int	IsReject(char *Ip)
{
	REJECTLIST	*list;
	
	list = reject_list_top;
	while(list){
		if(strcmp(list->Ip,Ip) == 0){
			return 1;
		}
		list = list->next;
	}
	return 0;
}
static	int	reg_reject_list(char *Ip)
{
	REJECTLIST	*list;

	if(IsIP(Ip) == 0){
		return 0;
	}

	if(IsReject(Ip) == 1){
		return 0;
	}

	list = (REJECTLIST *)calloc(1,sizeof(REJECTLIST));
	if(list == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	list->next = reject_list_top;
	my_strncpy(list->Ip,Ip,sizeof(list->Ip));
	reject_list_top = list;
	return 0;
}
/*****************************************************************************************/
/* キーリスト関連関数*/
/*****************************************************************************************/
/*static	void	dump_list(void)
{
	RECVLIST	*rlist;
	SENDLIST	*slist;

	printf("******* dump list start +++++++++\n");
	printf("******* recv list start +++++++++\n");
	rlist = recv_list_top;
	while(rlist){
		if(rlist->Ip){
			printf("         addr=%p next=%p Ip=%s\n",rlist,rlist->next,rlist->Ip);
		}
		rlist = rlist->next;
	}
	printf("******* send list start +++++++++\n");
	slist = send_list_top;
	while(slist){
		if(slist->Ip){
			printf("         Ip=%s\n",slist->Ip);
		}
		slist = slist->next;
	}
	printf("******* dump list end +++++++++\n");

}
*/
static  KEYLIST *list_from_name(char *name)
{
        KEYLIST         *list;

        list = key_list_top;

        while(list){
                if(strcmp(list->Name,name) == 0){
                        return list;
                }
                list = list->next;
        }

        return NULL;
}
static	KEYLIST	*list_from_ip(char *ip)
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(strcmp(list->Ip,ip) == 0){
			return list;
		}
		list = list->next;
	}

	return NULL;
}
static	int	check_list_item(char *Name,char *ip,char *kind,char *fname,KEYLIST *list)
{
	if(Name){
		if(strlen(Name) <  sizeof(list->Name)){
			my_strncpy(list->Name,Name,sizeof(list->Name));
		}else{
			fprintf(stderr,"Warning : Name=%.128s : Is too long.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			return -1;
		}
	}else{
		fprintf(stderr,"Warning : Name=%.128s : Invalid Name.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
		return -1;
	}
	if(IsIP(ip) == 0){
		if((strcmp(Name,"specify ip") == 0) && (strcmp(ip,"*") == 0)){
			strcpy(list->Ip,"*");
		}else{
			fprintf(stderr,"Warning : Name=%.128s : Invalid IP address.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			return -1;
		}
	}else{
		if(list_from_ip(ip) == NULL){
			my_strncpy(list->Ip,ip,sizeof(list->Ip));
		}else{
			fprintf(stderr,"Warning : Name=%.128s : IP duplicated.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			return -1;
		}
	}
	if(strcmp(kind,"ALLOW") == 0){
		list->Kind = KIND_ALLOW;
	}else if(strcmp(kind,"DENY") == 0){
		list->Kind = KIND_DENY;
		reg_reject_list(ip);
	}else if(strcmp(kind,"HOT") == 0){
		if(get_hot_ip() == NULL){
			list->Kind = KIND_HOT;
			my_strncpy(hot_ip,list->Ip,sizeof(hot_ip));
		}else{
			fprintf(stderr,"Warning : Name=%.128s : HOT duplicated.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			return -1;
		}
	}else{
		fprintf(stderr,"Warning : Name=%.128s : Kind setting error.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
		return -1;
	}

	if(fname){
		if(strlen(fname) < sizeof(list->KeyFileName)){
			my_strncpy(list->KeyFileName,fname,sizeof(list->KeyFileName));
		}else{
			fprintf(stderr,"Warning : Name=%.128s : Invalid key file name=%s.(file=%s line=%d)\n",Name,fname,__FILE__,__LINE__);
			return -1;
		}
	}else{
		list->KeyFileName[0] = 0;
	}
	return 0;
}
static	void	free_cipher(KEYLIST *list)
{
	if(list){
		if(list->en){
			EVP_CIPHER_CTX_free(list->en);
			list->en = NULL;
		}
		if(list->ed){
			EVP_CIPHER_CTX_free(list->ed);
			list->ed = NULL;
		}
		if(list->ctx){
			finish_cipher(list->ctx);
			list->ctx = NULL;
		}
	}
	return;
}
static	int	alloc_cipher(KEYLIST *list)
{
	FILE		*fp2;
	char		buf2[512],tmp[3];
	int		rv,i;

	if(list->KeyFileName[0]  == 0){
		list->ctx = NULL;
		list->en = NULL;
		list->ed = NULL;
		return 0;
	}

	rv = -1;
	fp2 = NULL;

//NG	pthread_mutex_init(&(list->m_ctx_enc),NULL);
//NG	pthread_mutex_init(&(list->m_ctx_dec),NULL);
// IHA KEYの設定
	if(list->KeyFileName[0]){
		if((list->ctx = init_cipher(list->KeyFileName,NULL,1200,1500,
				HASH_SHA_256,8,2)) == NULL){
			fprintf(stderr,"Warning : Name=%.128s init_cipher error file=%s.(file=%s line=%d)\n",
			 list->Name,list->KeyFileName,__FILE__,__LINE__);
			goto end;
		}
// AES KEYの設定
		fp2 = fopen(list->KeyFileName,"r");
		if(fp2 == NULL){
			fprintf(stderr,"Warning : Name=%.128s init aes error file=%s.(file=%s line=%d)\n",
			 list->Name,list->KeyFileName,__FILE__,__LINE__);
			goto end;
		}
		while(1){
			if(fgets(buf2,sizeof(buf2),fp2) == NULL){
				fprintf(stderr,"Warning : Name=%.128s init aes error file=%s.(file=%s line=%d)\n",
			 		list->Name,list->KeyFileName,__FILE__,__LINE__);
				goto end;
			}
			if(memcmp(buf2,"[aes_gcm]",9) != 0){
				continue;
			}
			if(fgets(buf2,sizeof(buf2),fp2) == NULL){
				fprintf(stderr,"Warning : Name=%.128s init aes error file=%s.(file=%s line=%d)\n",
			 		list->Name,list->KeyFileName,__FILE__,__LINE__);
				goto end;
			}
			if(strlen(buf2) < 120){
				fprintf(stderr,"Warning : Name=%.128s init aes error file=%s.(file=%s line=%d)\n",
			 		list->Name,list->KeyFileName,__FILE__,__LINE__);
				goto end;
			}
			for(i=0;i<60;i++){
				memcpy(tmp,&(buf2[i*2]),2);
				tmp[2] = 0x00;
				list->aes_key[i] = strtol(tmp,NULL,16);
			}
			break;
		}
	}

	list->en = EVP_CIPHER_CTX_new();
	list->ed = EVP_CIPHER_CTX_new();

	rv = 0;
end:
	if(fp2){
		fclose(fp2);
	}
	return rv;
}
static	int	my_fgets(char *buf,int bufsize,FILE *fp)
{
	int	i;
	char	tmp[2];

	for(i=0;i < bufsize ; i++){
		if(fread(buf+i,1,1,fp) <= 0){
			return i;
		}
		if(*(buf+i) == '\n'){
			return (i+1);
		}
	}
//
// \nまで読み飛ばす
//
	*(buf + bufsize - 1) = 0x00;
	fprintf(stderr,"One line exceeds 1023 bytes in address book or parameter file.(file=%s line=%d)\n",__FILE__,__LINE__);
	while(1){
		if(fread(tmp,1,1,fp) <= 0){
			return -1;
		}
		if(tmp[0] == '\n'){
			return -1;
		}
	}
}
static	int	init_key_list(void)
{
	FILE			*fp;
	char			buf[1024],*str,*stop;
	char			Name[512],Ip[64],Kind[32],KeyFileName[513];
	KEYLIST			list,*list2,*list3;
	int			r,rv;

	rv = 0;

	fp = fopen(addr_book_file,"r");
	if(fp == NULL){
		return -1;
	}

	key_entry_no = 0;

	while(1){
		memset(&list,0x00,sizeof(KEYLIST));
		memset(buf,0x00,sizeof(buf));
		memset(Name,0x00,sizeof(Name));
		Ip[0]          = 0x00;
		Kind[0]        = 0x00;
		KeyFileName[0] = 0x00;

		if((r = my_fgets(buf,sizeof(buf),fp)) == 0){
			break;
		}
		if(r == -1){
			rv = 1;
		}
		if((buf[0] == '#') || buf[0] == '\n'){
			continue;
		}

		str = strtok_r(buf,",\n",&stop);
		if(str == NULL){
			fprintf(stderr,"Warning : Name not found.(file=%s line=%d)\n",__FILE__,__LINE__);
			rv = 1;
			continue;
		}
		if(strlen(str) > MAX_KEY_NAME_LEN){
                        fprintf(stderr,"Warning : Name=%.128s is too long.(file=%s line=%d)\n",str,__FILE__,__LINE__);
			rv = 1;
                        continue;
		}
		my_strncpy(Name,str,sizeof(Name));
//
// 重複チェックを行う
//
		if(list_from_name(Name)){
                        fprintf(stderr,"Warning : Name=%.128s : Name dupricate.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			rv = 1;
                        continue;
                }

		if(key_entry_no >= max_key_list_no){
                        fprintf(stderr,"Warning : Name=%.128s is ignored. Address book is full.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			rv = 1;
                        continue;
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str == NULL){
			fprintf(stderr,"Warning : Name=%.128s : Ip not found.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			rv = 1;
			continue;
		}
		my_strncpy(Ip,str,sizeof(Ip));
//
// 重複チェックを行う
//
		if(list_from_ip(Ip)){
			fprintf(stderr,"Warning : Name=%.128s : Ip dupricate.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			rv = 1;
			continue;
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str == NULL){
			fprintf(stderr,"Warning : Name=%.128s : Kind not found.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
			rv = 1;
			continue;
		}
		my_strncpy(Kind,str,sizeof(Kind));

		str = strtok_r(NULL,",\n",&stop);
		if(str == NULL){
			KeyFileName[0] = 0;
		}else{
			if(strlen(str) > 255){
				fprintf(stderr,"Warning : Name=%.128s : Key file is too long.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
				rv = 1;
				continue;
			}
			if(strcmp("specify ip",Name) == 0){
				KeyFileName[0] = 0;
				fprintf(stderr,"Warning : Name=%.128s : No crypto for specify ip.(file=%s line=%d)\n",Name,__FILE__,__LINE__);
				rv = 1;
			}else{
				if(*str == '/'){		// 絶対パス指定
					my_strncpy(KeyFileName,str,sizeof(KeyFileName));
				}else{				// KEY_DIRからの相対パス指定
					sprintf(KeyFileName,"%s/%s",key_file_dir,str);
				}
			}
		}
		if(check_list_item(Name,Ip,Kind,KeyFileName,&list) == 0){	
// KEYの設定
			pthread_mutex_init(&(list.m_ctx_enc),NULL);
//			pthread_mutex_init(&(list.m_ctx_dec),NULL);
// KEYLISTに追加
			list2 = (KEYLIST *)calloc(1,sizeof(KEYLIST));
			if(list2 == NULL){
				fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
				fclose(fp);
				return -1;
			}
			memcpy(list2,&list,sizeof(KEYLIST));

			if(key_list_top == NULL){
				key_list_top = list2;
			}else{
				list3 = key_list_top;
				while(list3->next){
					list3 = list3->next;
				}
				list3->next = list2;
			}

			key_entry_no++;
		}
	}
	fclose(fp);
	return rv;
}
/*static	IHA_ENC_CTX	*ctx_from_ip(char	*Ip)
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(strcmp(Ip,list->Ip) == 0){
			if(list->Kind != KIND_DENY){		// 着信拒否！！
				return list->ctx;
			}else{
				return NULL;
			}
		}
		list = (KEYLIST *)list->next;
	}
	return NULL;
}
static	pthread_mutex_t	*mutex_enc_from_ip(char	*Ip)
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(strcmp(Ip,list->Ip) == 0){
			return (&(list->m_ctx_enc));
		}
		list = (KEYLIST *)list->next;
	}
	return NULL;
}
static	pthread_mutex_t	*mutex_dec_from_ip(char	*Ip)
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(strcmp(Ip,list->Ip) == 0){
			return (&(list->m_ctx_dec));
		}
		list = (KEYLIST *)list->next;
	}
	return NULL;
}*/

static	char	*get_hot_ip()
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(list->Kind == KIND_HOT){
			return (list->Ip);
		}
		list = (KEYLIST *)list->next;
	}
	return NULL;
}

static	char	*name_from_ip(char *Ip)
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(strcmp(Ip,list->Ip) == 0){
			return (list->Name);
		}
		list = (KEYLIST *)list->next;
	}
	return NULL;
}
static	int	set_kind_ip(char *Ip,int value)
{
	KEYLIST		*list;

	list = key_list_top;

	while(list){
		if(strcmp(Ip,list->Ip) == 0){
			list->SaveKind = list->Kind;
			list->Kind = value;
			list->DenyCnt = DENY_TIME_OUT;
			return 1;
		}
		list = (KEYLIST *)list->next;
	}
	return 0;
}
/*****************************************************************************************/
/* IMAGEエントリ処理*/
/*****************************************************************************************/
static	void		add_send_list(char *Ip,unsigned char crypto)
{
	int		i;
	KEYLIST		*list;

	pthread_mutex_lock(&m_send_list);

	for(i=0;i<MAX_CLI;i++){
		if((SendArray[i].used == 1 ) && (strcmp(SendArray[i].Ip,Ip)) == 0){
			pthread_mutex_unlock(&m_send_list);
			return;
		}
	}

	for(i=0;i<MAX_CLI;i++){
		if(SendArray[i].used == 0){

			list = list_from_ip(Ip);
			if(list){
				if(list->ctx == NULL){
					pthread_mutex_lock(&(list->m_ctx_enc));
					if(alloc_cipher(list) != 0){
						pthread_mutex_unlock(&(list->m_ctx_enc));
						pthread_mutex_unlock(&m_send_list);
						fprintf(stderr,"alloc_cipher error.(file=%s line=%d)\n",__FILE__,__LINE__);
						return;
					}
					pthread_mutex_unlock(&(list->m_ctx_enc));
				}
			}

			SendArray[i].used = 1;
			SendArray[i].sock = -1;
			SendArray[i].Sts = 0;
			SendArray[i].crypto = crypto;
			SendArray[i].image = 0;
			my_strncpy(SendArray[i].Ip,Ip,sizeof(SendArray[i].Ip));
			pthread_mutex_init(&(SendArray[i].m_sock),NULL);

			pthread_mutex_unlock(&m_send_list);
			return;
		}
	}

	pthread_mutex_unlock(&m_send_list);
	return;
}

static	void		del_send_list(char *Ip)
{
	int		i;

	pthread_mutex_lock(&m_send_list);

	for(i=0;i<MAX_CLI;i++){
		if((SendArray[i].used == 1 ) && (strcmp(SendArray[i].Ip,Ip)) == 0){
			SendArray[i].used = 0;
			SendArray[i].Ip[0] = 0;
			SendArray[i].Sts = 0;
			SendArray[i].image = 0;
			if(SendArray[i].sock > 2){
				close(SendArray[i].sock);	// ソケットを閉じる
			}
			SendArray[i].sock = -1;
			break;
		}
	}

	pthread_mutex_unlock(&m_send_list);
	return;
}

static	void		del_send_list2()
{
	int		i;

	pthread_mutex_lock(&m_send_list);

	for(i=0;i<MAX_CLI;i++){
		if(SendArray[i].Sts == 1 ){
//printf("***** del_send_list2 IP=%s\n",SendArray[i].Ip);
			SendArray[i].used = 0;
			SendArray[i].Ip[0] = 0;
			SendArray[i].Sts = 0;
			SendArray[i].image = 0;
			if(SendArray[i].sock > 2){
				close(SendArray[i].sock);		// ソケットを閉じる
			}
			SendArray[i].sock = -1;
			break;
		}
	}

	pthread_mutex_unlock(&m_send_list);
	return;
}
static	SENDLIST	*find_send_list(char	*Ip)
{
	int		i;

	pthread_mutex_lock(&m_send_list);

	for(i=0;i<MAX_CLI;i++){
		if(strcmp(SendArray[i].Ip,Ip) == 0){
			pthread_mutex_unlock(&m_send_list);
			return (&SendArray[i]);
		}
	}

	pthread_mutex_unlock(&m_send_list);

	return NULL;
}
static	RECVLIST	*find_recv_list(char	*Ip)
{
	RECVLIST	*list;

	pthread_mutex_lock(&m_recv_list);

	list = recv_list_top;

	for(;list;){
		if(strcmp(list->Ip,Ip) == 0){
			pthread_mutex_unlock(&m_recv_list);
			return (list);
		}
		list = list->next;
	}

	pthread_mutex_unlock(&m_recv_list);

	return NULL;
}
static	void	reg_rec_taiseki(RECVLIST *list)
{
	int	i;

	for(i=0;i<32;i++){
		if(RecArray[i].Ip[0]  == 0){
			my_strncpy(RecArray[i].Ip,list->Ip,64);
			RecArray[i].time = list->recv_time;
			break;
		}
		if(strcmp(RecArray[i].Ip,list->Ip) == 0){
			RecArray[i].time = list->recv_time;
			break;
		}
	}
}
//
//
// del_recv_listとadd_recv_listは同じスレッド内で使用すること。マルチスレッド非対応！！
//
static	void del_recv_list(char *Ip)
{
	RECVLIST	*list,*list2;
	KEYLIST		*klist;

//	pthread_mutex_lock(&m_recv_list);

	list = recv_list_top;
	if(list == NULL){
//		pthread_mutex_unlock(&m_recv_list);
		return;
	}
//
// 最初のリストの場合
//
	if(strcmp(list->Ip,Ip)==0){
		if(list->tid != -1){
			list->exit_thread_flg = 1;
			pthread_join(list->tid,NULL);
			list->tid = -1;
			if(recflg){
				reg_rec_taiseki(list);
			}
		}
		free(list->image_buf);
		free(list->image_buf2);
		list->image_buf = NULL;
		list->ImageRecv = 0;

		if(list->surf){
			cairo_surface_destroy(list->surf);
			list->surf = NULL;
		}

		recv_list_top = list->next;

		free(list);
	}else{
//
// リストを目的の場所１歩手前まで進める
//
		while(list->next){
			if(strcmp(list->next->Ip,Ip)==0){
				break;
			}
			list = list->next;
		}
		if(list->next == NULL){
			fprintf(stderr,"Internal contradiction!!(file=%s line=%d)\n",__FILE__,__LINE__);
//			pthread_mutex_unlock(&m_recv_list);
			return;
		}

		if(list->next->tid != -1){
			list->next->exit_thread_flg = 1;
			pthread_join(list->next->tid,NULL);
			list->next->tid = -1;
			if(recflg){
				reg_rec_taiseki(list->next);
			}
		}
		list->next->ImageRecv = 0;

		if(list->next->surf){
			cairo_surface_destroy(list->next->surf);
			list->next->surf = NULL;
		}


		free(list->next->image_buf);
		free(list->next->image_buf2);
		list->next->image_buf = NULL;
		list2 = list->next;
		list->next = list->next->next;
		free(list2);
	}

	if(cur_sess_no > 0){
		cur_sess_no--;
		if(cur_sess_no == 0){
			outouflg = 0;
			HassinNo = 0;
		}
	}
	chgflg = 1;
//	pthread_mutex_unlock(&m_recv_list);

	klist = list_from_ip(Ip);
	if(klist){
//
// IPを元に戻す
//
		if(strcmp(klist->Name,"specify ip") == 0) {
			strcpy(klist->Ip,"*");
			klist->DenyCnt = DENY_TIME_OUT;		// タイマを設定
		}
//
// 暗号資源を開放する
//
		pthread_mutex_lock(&(klist->m_ctx_enc));
		free_cipher(klist);
		pthread_mutex_unlock(&(klist->m_ctx_enc));
	}
//
// キャプチャーウィンドウの停止
//
	if((window3) && (cur_sess_no == 0)){
		stop_capture = 1;
		pthread_join(tid_capture,NULL);
		destroy_capture(window3,NULL);
		window3 = NULL;
		capture_flg = 0;
	}

	return;
}

static	RECVLIST	*add_recv_list(char *Ip,unsigned char type)
{
	RECVLIST	*list,*list2;
	int		entno;
	int		i;


	entno=0;
	list = recv_list_top;
	while(list){
		if((type == PRO_SEND_IMG) || (type == PRO_SEND_VOICE) ||	// main ch
		(type == PRO_SEND_IMG_NO_CRYPTO) || (type == PRO_SEND_VOICE_NO_CRYPTO)){
			if(strcmp(list->Ip,Ip)==0){
//printf("add_recv_list ch==0 much!!\n");
				clock_gettime(CLOCK_REALTIME,&(list->last_recv_time));
				return list;		// 既に存在する
			}
		}else if((type == PRO_SEND_IMG_SUB) || (type == PRO_SEND_IMG_SUB_NO_CRYPTO)){	// sub ch
			if(strcmp(&(list->Ip[2]),Ip)==0){
//printf("add_recv_list ch==1 much!!\n");
				clock_gettime(CLOCK_REALTIME,&(list->last_recv_time));
				return list;		// 既に存在する
			}
		}
		entno++;
		list = list->next;
	}
	if(entno >= MAX_CLI){
		printf("No more connection !!\n");
		return NULL;				// 最大数を超える
	}
//
// 登録する
//
	pthread_mutex_lock(&m_recv_list);

	if((list2 = (RECVLIST *)calloc(1,sizeof(RECVLIST))) == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		pthread_mutex_unlock(&m_recv_list);
		return NULL;
	}
	list2->total = 0;

	if((type == PRO_SEND_IMG) || (type == PRO_SEND_IMG_NO_CRYPTO)){
		list2->ch = 0;				// main ch
		my_strncpy(list2->Ip,Ip,sizeof(list2->Ip));
//printf("add_recv_list ch==0\n");
	}else if((type == PRO_SEND_IMG_SUB) || (type == PRO_SEND_IMG_SUB_NO_CRYPTO)){
		list2->ch = 1;				// sub ch
//printf("add_recv_list ch==1\n");
		my_strncpy(list2->Ip,"S.",sizeof(list2->Ip));
		strcat(list2->Ip,Ip);
	}else{
//printf("add_recv_list ch==v\n");
		my_strncpy(list2->Ip,Ip,sizeof(list2->Ip));
	}



	if((list2->image_buf = (char *)calloc(1,(size_t)(MAX_JPEG_SIZE + 32))) == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		free(list2);
		list2 = NULL;
		pthread_mutex_unlock(&m_recv_list);
		return NULL;
	}
	if((list2->image_buf2 = (char *)calloc(1,(size_t)(SECOND_BUF_SIZE))) == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		free(list2->image_buf);
		free(list2);
		list2 = NULL;
		pthread_mutex_unlock(&m_recv_list);
		return NULL;
	}
	list2->bufp = list2->image_buf;
	clock_gettime(CLOCK_REALTIME,&(list2->last_recv_time));
	list2->next = NULL;

// Mutxの初期化
	pthread_mutex_init(&(list2->m_write),NULL);

	cur_sess_no++;

	if((voice_no < max_sound_threads) && (list2->ch != 1)){
// SOUND書き込みスレッドの起動
		if(pthread_create(&(list2->tid), NULL, write_thread, (void *)list2) != 0){
			fprintf(stderr,"pthread_create error6 name=%s.(file=%s line=%d)\n",name_from_ip(list2->Ip),__FILE__,__LINE__);
//			free(list2->image_buf);
//			list2->image_buf = NULL;
			list2->tid = -1;
//			free(list2);
//			pthread_mutex_unlock(&m_recv_list);
//			return NULL;
		}else{
			voice_no++;
		}
	}else{
		list2->tid = -1;
	}
//
// 録画中の着信に対応
//
	if(recflg){
		list2->recflg = 1;
	}
//
// 着信時刻の設定
//
	clock_gettime(CLOCK_REALTIME,&(list2->recv_time));


	if(cur_sess_no == 1){
		ent_table1[0].used = 1;
		list2->startx = ent_table1[0].startx;
		list2->starty = ent_table1[0].starty;
		list2->disp_no = 0;
	}else if(cur_sess_no == 2){
		list = recv_list_top;
		i=0;
		while(list){
			list->startx = ent_table2[i].startx;
			list->starty = ent_table2[i].starty;
			list->disp_no = i;
			ent_table2[i].used = 1;
			list = list->next;
			i++;
		}
		for(i=0;i<2;i++){
			if(ent_table2[i].used == 0){
				ent_table2[i].used = 1;
				list2->startx = ent_table2[i].startx;
				list2->starty = ent_table2[i].starty;
				list2->disp_no = i;
				break;
			}
		}
		ent_table1[0].used = 0;
	}else if(cur_sess_no == 3){
		if(PinPmode == 0){
			cairo_scale(gcr, (double)1.0/(double)2.0, (double)1.0/(double)2.0);
		}
		list = recv_list_top;
		i=0;
		while(list){
			list->startx = ent_table3[i].startx;
			list->starty = ent_table3[i].starty;
			list->disp_no = i;
			ent_table3[i].used = 1;
			list = list->next;
			i++;
		}
		for(i=0;i<3;i++){
			if(ent_table3[i].used == 0){
				ent_table3[i].used = 1;
				list2->startx = ent_table3[i].startx;
				list2->starty = ent_table3[i].starty;
				list2->disp_no = i;
				break;
			}
		}
		ent_table2[0].used = 0;
		ent_table2[1].used = 0;
	}else if(cur_sess_no == 4){
		list = recv_list_top;
		i=0;
		while(list){
			list->startx = ent_table4[i].startx;
			list->starty = ent_table4[i].starty;
			list->disp_no = i;
			ent_table4[i].used = 1;
			list = list->next;
			i++;
		}
		for(i=0;i<4;i++){
			if(ent_table4[i].used == 0){
				ent_table4[i].used = 1;
				list2->startx = ent_table4[i].startx;
				list2->starty = ent_table4[i].starty;
				list2->disp_no = i;
				break;
			}
		}

		ent_table3[0].used = 0;
		ent_table3[1].used = 0;
		ent_table3[2].used = 0;

	}else if((5 <= cur_sess_no) && (cur_sess_no <= 9)){
		if(cur_sess_no == 5){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)2.0/(double)3.0 , (double)2.0/(double)3.0);
			}
			list = recv_list_top;
			i=0;
			while(list){
				list->startx = ent_table9[i].startx;
				list->starty = ent_table9[i].starty;
				list->disp_no = i;
				ent_table9[i].used = 1;
				list = list->next;
				i++;
			}
			for(i=0;i<4;i++){
				ent_table4[i].used = 0;
			}
		}
		for(i=0;i<9;i++){
			if(ent_table9[i].used == 0){
				ent_table9[i].used = 1;
				list2->startx = ent_table9[i].startx;
				list2->starty = ent_table9[i].starty;
				list2->disp_no = i;
				break;
			}
		}
	}else if((10 <= cur_sess_no) && (cur_sess_no <= 16)){
		if(cur_sess_no == 10){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)3.0/(double)4.0, (double)3.0/(double)4.0);
			}
			list = recv_list_top;
			i=0;
			while(list){
				list->startx = ent_table16[i].startx;
				list->starty = ent_table16[i].starty;
				list->disp_no = i;
				ent_table16[i].used = 1;
				list = list->next;
				i++;
			}
			for(i=0;i<9;i++){
				ent_table9[i].used = 0;
			}
		}
		for(i=0;i<16;i++){
			if(ent_table16[i].used == 0){
				ent_table16[i].used = 1;
				list2->startx = ent_table16[i].startx;
				list2->starty = ent_table16[i].starty;
				list2->disp_no = i;
				break;
			}
		}
	}else if((17 <= cur_sess_no) && (cur_sess_no <= 25)){
		if(cur_sess_no == 17){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)4.0/(double)5.0, (double)4.0/(double)5.0);
			}
			list = recv_list_top;
			i=0;
			while(list){
				list->startx = ent_table25[i].startx;
				list->starty = ent_table25[i].starty;
				list->disp_no = i;
				ent_table25[i].used = 1;
				list = list->next;
				i++;
			}
			for(i=0;i<16;i++){
				ent_table16[i].used = 0;
			}
		}
		for(i=0;i<25;i++){
			if(ent_table25[i].used == 0){
				ent_table25[i].used = 1;
				list2->startx = ent_table25[i].startx;
				list2->starty = ent_table25[i].starty;
				list2->disp_no = i;
				break;
			}
		}
	}else if((26 <= cur_sess_no) && (cur_sess_no <= 36)){
		if(cur_sess_no == 26){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)5.0/(double)6.0, (double)5.0/(double)6.0);
			}
			list = recv_list_top;
			i=0;
			while(list){
				list->startx = ent_table36[i].startx;
				list->starty = ent_table36[i].starty;
				list->disp_no = i;
				ent_table36[i].used = 1;
				list = list->next;
				i++;
			}
			for(i=0;i<25;i++){
				ent_table25[i].used = 0;
			}
		}
		for(i=0;i<36;i++){
			if(ent_table36[i].used == 0){
				ent_table36[i].used = 1;
				list2->startx = ent_table36[i].startx;
				list2->starty = ent_table36[i].starty;
				list2->disp_no = i;
				break;
			}
		}
	}else if((37 <= cur_sess_no) && (cur_sess_no <= 49)){
		if(cur_sess_no == 37){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)6.0/(double)7.0, (double)6.0/(double)7.0);
			}
			list = recv_list_top;
			i=0;
			while(list){
				list->startx = ent_table49[i].startx;
				list->starty = ent_table49[i].starty;
				list->disp_no = i;
				ent_table49[i].used = 1;
				list = list->next;
				i++;
			}
			for(i=0;i<36;i++){
				ent_table36[i].used = 0;
			}
		}
		for(i=0;i<49;i++){
			if(ent_table49[i].used == 0){
				ent_table49[i].used = 1;
				list2->startx = ent_table49[i].startx;
				list2->starty = ent_table49[i].starty;
				list2->disp_no = i;
				break;
			}
		}
	}
//
// リストに追加する
//
	if(recv_list_top == NULL){
		recv_list_top = list2;
	}else{
		list = recv_list_top;
		while(list->next){
			list = list->next;	
		}
		list->next = list2;
	}

	chgflg = 1;

	pthread_mutex_unlock(&m_recv_list);

	return list2;
}
/*****************************************************************************************/
/* pcmサスペンド処理*/
/*****************************************************************************************/
static void suspend(snd_pcm_t *handle)
{
	int res;

	printf("Suspended. Trying resume. ");
	while ((res = snd_pcm_resume(handle)) == -EAGAIN){
		sleep(1);	/* wait until suspend flag is released */
	}
	if (res < 0) {
		printf("Failed. Restarting stream. ");
		if ((res = snd_pcm_prepare(handle)) < 0) {
			fprintf(stderr,"suspend: prepare error: %s.(file=%s line=%d)",snd_strerror(res),__FILE__,__LINE__);
		}
	}else{
		printf("Resume Done.\n");
	}
}
/*****************************************************************************************/
/* pcm書き込み処理*/
/*****************************************************************************************/
//struct	timespec	t1,t2;
static int pcm_write(RECVLIST *list,u_char *data, int count)
{
	int			r;
	int			result = 0;
	int			retry = 0;
	struct	timespec	t1,t2;

	clock_gettime(CLOCK_REALTIME,&t1);

//	if(VoiceWriteCnt >= 1){
//		if(t2.tv_sec == t1.tv_sec){
//			VoiceWriteTotall += (t1.tv_nsec - t2.tv_nsec)/1000;
//		}else{
//			VoiceWriteTotall += (t1.tv_nsec + (1000000000 - t2.tv_nsec))/1000;
//		}
//	}


	while (count > 0) {
		r = (int)snd_pcm_writei(list->handle, data, (size_t)count);
		if((0 <= r) && (r <= count)){
//			printf("pcm_write r=%d\n",r);
			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
			if(count != 0){
				snd_pcm_wait(list->handle, 1000);
			}
		}else if(r == -EAGAIN){
			retry++;
			if(retry > 10){
//				printf("pcm_write EAGAIN name=%s\n",name_from_ip(list->Ip));

				list->readp = list->pcm_buf;
				list->writep = list->pcm_buf;
				list->sleepflg = 1;
				snd_pcm_close(list->handle);
				list->handle = init_sound(SND_PCM_STREAM_PLAYBACK);
				return -1;
			}
			snd_pcm_wait(list->handle, 1000);
			continue;

		}else if(r == -EPIPE){
//			p_message("Warning : underrun? from ",name_from_ip(list->Ip));

//			if((r = snd_pcm_recover(list->handle,r,0)) < 0){
//				printf("snd_pcm_writei failed: %s\n",snd_strerror(r));
//			}
//			snd_pcm_prepare(list->handle);
//			if(snd_pcm_reset(list->handle) != 0){
//				printf("snd_pcm_reset failed: %s\n",snd_strerror(r));
//			}

			list->readp = list->pcm_buf;
			list->writep = list->pcm_buf;
			list->sleepflg = 1;
			snd_pcm_close(list->handle);
			list->handle = init_sound(SND_PCM_STREAM_PLAYBACK);

			return r;

		}else if(r == -ESTRPIPE){
			printf("pcm_write suspend!!\n");
			suspend(list->handle);
		}else if(r < 0){
			fprintf(stderr,"write error: %s.(file=%s line=%d)",snd_strerror(r),__FILE__,__LINE__);
			return (-1);
		}
	}
	clock_gettime(CLOCK_REALTIME,&t2);

	VoiceWriteCnt++;
	if(t1.tv_sec == t2.tv_sec){
		VoiceWriteTotall += (t2.tv_nsec - t1.tv_nsec)/1000;
	}else{
		VoiceWriteTotall += (t2.tv_nsec + (1000000000 - t1.tv_nsec))/1000;
	}

	return result;
}

/*****************************************************************************************/
/* pcm読み込み処理*/
/*****************************************************************************************/
static int pcm_read(snd_pcm_t *handle,u_char *data, int rcount)
{
        int			r;
        int			result = 0;
        int			count;
	struct	timespec	t1,t2;

	count = rcount;

	clock_gettime(CLOCK_REALTIME,&t1);

	while(count > 0) {
		r = (int)snd_pcm_readi(handle, data, (size_t)count);
		if((0 <= r && r < count)) {
//			printf("snd_pcm_readi read not complete r=%d count=%d result=%d EAGAIN=%d\n",r,count,result,EAGAIN);
			snd_pcm_wait(handle, 1000);
		}else if(r == -EAGAIN) {
//			printf("snd_pcm_readi -EAGAIN r=%d\n",r);
			snd_pcm_wait(handle, 1000);
			continue;
		}else if(r == -EPIPE) {
			fprintf(stderr,"snd_pcm_readi -EPIPE.(file=%s line=%d)\n",__FILE__,__LINE__);
			return (-1);
		}else if(r == -ESTRPIPE) {
			fprintf(stderr,"snd_pcm_readi -ESTRPIPE.(file=%s line=%d)\n",__FILE__,__LINE__);
			return (-1);
		}else if(r < 0) {
			fprintf(stderr,"snd_pcm_readi < 0.(file=%s line=%d)\n",__FILE__,__LINE__);
			return (-1);
		}
		if (r > 0) {
			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
		}
	}
	clock_gettime(CLOCK_REALTIME,&t2);

	VoiceReadCnt++;
	if(t1.tv_sec == t2.tv_sec){
		VoiceReadTotall += (t2.tv_nsec - t1.tv_nsec)/1000;
	}else{
		VoiceReadTotall += (t2.tv_nsec + (1000000000 - t1.tv_nsec))/1000;
	}
        return result;
}
/*****************************************************************************************/
/* 音声入出力処理用パラメータ設定処理*/
/*****************************************************************************************/
static int set_sound_params(snd_pcm_t *handle)
{
	snd_pcm_hw_params_t	*params;
	snd_pcm_sw_params_t	*swparams;
	snd_pcm_uframes_t	buffer_size;
	snd_pcm_uframes_t	start_threshold, stop_threshold;
	int 			err;
	size_t 			n;
	unsigned int 		rate;
	char 			plugex[64];
	const char 		*pcmname;


	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);
	if(snd_pcm_hw_params_any(handle, params) < 0) {
		fprintf(stderr,"Broken config for this PCM: no config.(file=%s line=%d)\n",__FILE__,__LINE__);
	}
	if(snd_pcm_hw_params_set_access(handle, params,SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		fprintf(stderr,"Access type not available.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(snd_pcm_hw_params_set_format(handle, params, hwparams.format) < 0) {
		fprintf(stderr,"Sample format non available.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(snd_pcm_hw_params_set_channels(handle, params, hwparams.channels) < 0) {
		fprintf(stderr,"Channels count non available.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	rate = hwparams.rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);
	if(err < 0){
		fprintf(stderr,"snd_pcm_hw_params_set_rate_near error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if((float)rate * 1.05 < hwparams.rate || (float)rate * 0.95 > hwparams.rate) {
		pcmname = snd_pcm_name(handle);
		if(! pcmname || strchr(snd_pcm_name(handle), ':')){
			*plugex = 0;
		}else{
			snprintf(plugex, sizeof(plugex), "(-Dplug:%s)",snd_pcm_name(handle));
		}
		printf(gettext("please try the plug plugin %s\n"),plugex);
	}
	err = snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
	if(err < 0){
		fprintf(stderr,"snd_pcm_hw_params_get_buffer_time_max error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(buffer_time > 48000){
		buffer_time = 48000;
	}
	if(buffer_time > 0){
		period_time = buffer_time / 4;
	}else{
		period_frames = buffer_frames / 4;
	}
	if(period_time > 0){
		err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
	}else{
		err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_frames, 0);
	}
	if(err < 0){
		fprintf(stderr,"snd_pcm_hw_params_set_period_time(size)_near error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(buffer_time > 0){
		err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
	}else{
		err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_frames);
	}
	if(err < 0){
		fprintf(stderr,"snd_pcm_hw_params_set_buffer_time(size)_near error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	snd_pcm_hw_params(handle, params);

	snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	if(chunk_size == buffer_size){
		fprintf(stderr,"Can't use period equal to buffer size (%lu == %lu).(file=%s line=%d)\n",chunk_size, buffer_size,__FILE__,__LINE__);
		return -1;
	}


	snd_pcm_sw_params_current(handle, swparams);
	snd_pcm_sw_params_set_avail_min(handle, swparams, chunk_size);

	n = buffer_size;
	start_threshold = n;
	if(start_threshold < 1){
		start_threshold = 1;
	}
	if(start_threshold > n){
		start_threshold = n;
	}
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, start_threshold);
	assert(err >= 0);
	stop_threshold = buffer_size;
	err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);
	if(err < 0){
		fprintf(stderr,"snd_pcm_sw_params_set_stop_threshold error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	if(snd_pcm_sw_params(handle, swparams) < 0) {
		fprintf(stderr,"unable to install sw params.(file=%s line=%d)\n",__FILE__,__LINE__);
//		snd_pcm_sw_params_dump(swparams, loger);
		return -1;
	}


	bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
	bits_per_frame = bits_per_sample * hwparams.channels;
	chunk_bytes = chunk_size * bits_per_frame / 8;

	return 0;
}
/*****************************************************************************************/
/* 音声出力用パラメータ初期化処理*/
/*****************************************************************************************/
static	snd_pcm_t *init_sound(snd_pcm_stream_t fmt)
{
	int		err;
	snd_pcm_info_t	*info;
	snd_pcm_t	*tmphandle;


	pthread_mutex_lock(&m_init_sound);

	snd_pcm_info_alloca(&info);
//	err = snd_output_stdio_attach(&loger, stderr, 0);
//	assert(err >= 0);

	memset(&hwparams,0x00,sizeof(hwparams));
	hwparams.format = SND_PCM_FORMAT_S16_LE;
	hwparams.rate = SOUND_RATE;
	hwparams.channels = SOUND_CH_NO;


/*

char **hints;
err = snd_device_name_hint(-1, "pcm", (void***)&hints);
if (err != 0)
   return NULL;//Error! Just return

char** n = hints;
while (*n != NULL) {

    char *name = snd_device_name_get_hint(*n, "NAME");

    if (name != NULL && 0 != strcmp("null", name)) {
        //Copy name to another buffer and then free it
	
	err = snd_pcm_open(&tmphandle,name, fmt, SND_PCM_NONBLOCK);
	if(err >= 0){
		snd_pcm_close(tmphandle);
		printf("name=%s\n",name);
	}



        free(name);
    }
    n++;
}//End of while

//Free hint buffer too
snd_device_name_free_hint((void**)hints);

return NULL;
*/

// OK	err = snd_pcm_open(&tmphandle,"pulse", fmt, SND_PCM_NONBLOCK);
// OK	err = snd_pcm_open(&tmphandle,"surround21:CARD=PCH,DEV=0", fmt, SND_PCM_NONBLOCK);
// NG	err = snd_pcm_open(&tmphandle,"surround71:CARD=PCH,DEV=0", fmt, SND_PCM_NONBLOCK);
// NG	err = snd_pcm_open(&tmphandle,"surround40:CARD=PCH,DEV=0", fmt, SND_PCM_NONBLOCK);
//鳴らない readi error	err = snd_pcm_open(&tmphandle,"sysdefault:CARD=PCH", fmt, SND_PCM_NONBLOCK);
//NG	err = snd_pcm_open(&tmphandle,"sysdefault:PCH,0", fmt, SND_PCM_NONBLOCK);
// OK	err = snd_pcm_open(&tmphandle,"default", fmt, SND_PCM_NONBLOCK);
////	err = snd_pcm_open(&tmphandle,"default", fmt, SND_PCM_ASYNC);


	if(fmt == SND_PCM_STREAM_PLAYBACK){
		err = snd_pcm_open(&tmphandle,"default", fmt, SND_PCM_NONBLOCK);
	}else{
		err = snd_pcm_open(&tmphandle,"default", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"surround21:CARD=UCAMC0220F,DEV=0", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"front:CARD=UCAMC0220F,DEV=0", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"sysdefault:CARD=UCAMC0220F", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"dsnoop:CARD=UCAMC0220F,DEV=0", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"hw:UCAMC0220F,0", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"plughw:CARD=UCAMC0220F,DEV=0", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"plughw:UCAMC0220F,0", fmt, SND_PCM_NONBLOCK);
//NG		err = snd_pcm_open(&tmphandle,"hw:1,0", fmt, SND_PCM_NONBLOCK);
	}


	if(err < 0){
		fprintf(stderr,"snd_pcm_open error: %s.(file=%s line=%d)", snd_strerror(err),__FILE__,__LINE__);
		pthread_mutex_unlock(&m_init_sound);
		return NULL;
	}

	if((err = snd_pcm_info(tmphandle, info)) < 0){
		fprintf(stderr,"snd_pcm_info error: %s.(file=%s line=%d)", snd_strerror(err),__FILE__,__LINE__);
		snd_pcm_close(tmphandle);
		pthread_mutex_unlock(&m_init_sound);
		return NULL;
	}

	if(set_sound_params(tmphandle) != 0){
		fprintf(stderr,"set_sound_params error: %s.(file=%s line=%d)", snd_strerror(err),__FILE__,__LINE__);
		snd_pcm_close(tmphandle);
		pthread_mutex_unlock(&m_init_sound);
		return NULL;
	}

	if(snd_pcm_start(tmphandle) < 0){
		fprintf(stderr,"snd_pcm_start error: %s.(file=%s line=%d)", snd_strerror(err),__FILE__,__LINE__);
		snd_pcm_close(tmphandle);
		pthread_mutex_unlock(&m_init_sound);
		return NULL;
	}

	pthread_mutex_unlock(&m_init_sound);

	return tmphandle;


/*

	if ((err = snd_pcm_set_params(tmphandle,
		SND_PCM_FORMAT_S16_LE,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		1,
		SOUND_RATE,
		1,
		50000)) < 0) {
		printf("unable to set hw parameters: %s\n", snd_strerror(err));
		return NULL;
	}


	chunk_size = 64;
	chunk_bytes = 128;
	bits_per_frame = 16;
*/

	return tmphandle;

}
/*****************************************************************************************/
/* WAVファイル保存処理*/
/*****************************************************************************************/
static void write_word(FILE* fp, unsigned long value, unsigned size)
{
	for ( ; size; --size, value >>= 8 ) { 
		char v = (char)(value & 0xFF);
		fwrite(&v,1,1,fp);
	}
}


static	void	save_wave(RECVLIST *list,unsigned char *buf,int size)
{
	size_t 		file_length;
	char		FileName[256];


	if(list->recflg == 0){
		if(list->wave_fp){
			file_length = ftell(list->wave_fp);
			fseek(list->wave_fp,list->data_chunk_pos + 4,SEEK_SET);
			write_word(list->wave_fp,file_length - list->data_chunk_pos + 8,4);
			fseek(list->wave_fp,0 + 4,SEEK_SET);
			write_word(list->wave_fp,file_length - 8,4);
			fflush(list->wave_fp);
			fclose(list->wave_fp);
			list->wave_fp = NULL;
		}
	}else{
		if(list->wave_fp == NULL){
			sprintf(FileName,"/tmp/ihaphone_V-%s.wav",list->Ip);
			list->wave_fp = fopen(FileName,"w");
			if ( list->wave_fp == NULL ){
				list->recflg = 0;
				return;
			}

			fprintf(list->wave_fp,"RIFF----WAVEfmt ");
			write_word(list->wave_fp,16,4);
			write_word(list->wave_fp,1,2);
			write_word(list->wave_fp,1,2);
			write_word(list->wave_fp,SOUND_RATE,4);
//			write_word(list->wave_fp,8000,4);サンプル周波数が８０００だとVLCでエラーメッセージがでない。
			write_word(list->wave_fp,(SOUND_RATE * 2 * 1),4);
			write_word(list->wave_fp,2,2);
			write_word(list->wave_fp,16,2);
			list->data_chunk_pos = ftell(list->wave_fp);
			fprintf(list->wave_fp,"data----");
		}else{
			fwrite(buf,1,size,list->wave_fp);
			fflush(list->wave_fp);
		}
	}
}
/*****************************************************************************************/
/* 音声データ書き込みスレッド*/
/*****************************************************************************************/
static	void	*write_thread(void *Param)
{
	RECVLIST	*list;
	int		DummyWrite;

	list = (RECVLIST *)Param;

/*	cpu_set_t       cpu_set;
	int             result;



	if(GetCpuThreadNo() >= 2){
		CPU_ZERO(&cpu_set);
		CPU_SET(0x00000001,&cpu_set);
		result = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
		if(result != 0){
			printf("sched_setaffinity error rv = %d\n",result);
		}
	}
*/
/*
	pthread_t		th;
	pthread_attr_t		attr;
	struct	sched_param	sparam;
	int			policy=0,priority=0;


	th = pthread_self();
	pthread_attr_init(&attr);
//	pthread_attr_getschedpolicy(&attr,&policy);
//printf("policy=%d\n",policy);
	policy = SCHED_FIFO;
	if(pthread_attr_setschedpolicy(&attr,policy) !=0){
		printf("pthread_attr_setschedpolicy error\n");
	}
//printf("policy=%d\n",policy);
//	priority = sched_get_priority_max(policy);
	priority = 50;
//printf("priority=%d\n",priority);
	sparam.sched_priority = priority;
	if(pthread_setschedparam(th,policy,&sparam) != 0){
		printf("pthread_setschedparam error\n");
	}

	if(pthread_setschedprio(th,priority) != 0){
		fprintf(stderr,"pthread_setschedprio error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}else{
		printf("Thread priority set to FIFO %d for %s\n",priority,name_from_ip(list->Ip));
	}
*/
// バッファの確保
	list->tmp_pcm_buf = (unsigned char *)calloc(1,chunk_bytes);
	if(list->tmp_pcm_buf == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return NULL;
	}
        list->pcm_buf = (unsigned char *)calloc(1,SOUND_BUFSIZE);
        if(list->pcm_buf == NULL){
                fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
                free(list->tmp_pcm_buf);
                list->tmp_pcm_buf = NULL;
                return NULL;
        }

	list->readp = list->pcm_buf;
	list->writep = list->pcm_buf;
	list->sleepflg = 0;
	list->init_complete = 1;

	usleep(1000*200);
// SOUND初期化
	list->handle = init_sound(SND_PCM_STREAM_PLAYBACK);
	if(list->handle == NULL){
		fprintf(stderr,"init_sound error ip=%s.(file=%s line=%d)\n",list->Ip,__FILE__,__LINE__);
		return NULL;
	}

	DummyWrite = 0;

	while(1){
		if(list->exit_thread_flg){

			if(list->recflg){
				list->recflg = 0;
				save_wave(list,list->tmp_pcm_buf,chunk_bytes);
			}

			list->init_complete = 0;
			free(list->pcm_buf);
			list->pcm_buf = NULL;
			free(list->tmp_pcm_buf);
			list->tmp_pcm_buf = NULL;

			if(list->handle){
				snd_pcm_close(list->handle);
			}
			list->handle = NULL;
			voice_no--;
			return NULL;
		}
		if(list->sleepflg == 1){
			usleep(1000*200);	// バッファリング
			list->sleepflg = 0;
		}

		if((unsigned long)(list->readp) > (unsigned long)(list->writep)){
			if(((unsigned long)(list->readp) - (unsigned long)(list->pcm_buf)) >= SOUND_BUFSIZE){
				list->readp = list->pcm_buf;
				if(((unsigned long)(list->writep) - (unsigned long)(list->readp)) < chunk_bytes){
					if(DummyWrite){
//						p_message("dummy write2 for ",name_from_ip(list->Ip));
						memset(list->tmp_pcm_buf,0x00,chunk_bytes);
						pcm_write(list,list->tmp_pcm_buf,(int)chunk_size);
						save_wave(list,list->tmp_pcm_buf,chunk_bytes);
						DummyWrite = 0;
					}else{
						usleep(1000*16);
						DummyWrite = 1;
					}
					continue;
				}
			}
		}else{
			if(((unsigned long)(list->writep) - (unsigned long)(list->readp)) < chunk_bytes){
				if(DummyWrite){
//					p_message("dummy write for ",name_from_ip(list->Ip));
					memset(list->tmp_pcm_buf,0x00,chunk_bytes);
					pcm_write(list,list->tmp_pcm_buf,(int)chunk_size);
					save_wave(list,list->tmp_pcm_buf,chunk_bytes);
					DummyWrite = 0;
				}else{
					usleep(1000*16);
					DummyWrite = 1;
				}
				continue;
			}
		}

		memcpy(list->tmp_pcm_buf,list->readp,chunk_bytes);
		memset(list->readp,0x00,chunk_bytes);
		list->readp += chunk_bytes;
		if(((unsigned long)(list->readp) - (unsigned long)(list->pcm_buf)) >= SOUND_BUFSIZE){
			list->readp = list->pcm_buf;
		}

		DummyWrite = 0;

		pcm_write(list,list->tmp_pcm_buf,(int)chunk_size);
		save_wave(list,list->tmp_pcm_buf,chunk_bytes);
	}
}
/*****************************************************************************************/
/* 送信処理*/
/*****************************************************************************************/
static	int	send_sess(int utime,unsigned char *data,int length,char	*ip,int port)
{
	int			sock=-1,on=1;
	int			err;
	char			portstr[32];
	struct timeval 		tv;
	struct addrinfo		hints,*res;
	char			Editbuffer[1600];
//	SHA256_CTX              c1;
	unsigned char           hash[32];

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	sprintf(portstr,"%d",port);
	err = getaddrinfo(ip,portstr,&hints,&res);
	if(err != 0){
		fprintf(stderr,"getaddrinfo err 1: ip=%s  %s.(file=%s line=%d)\n",ip,gai_strerror(err),__FILE__,__LINE__);
		return -1;
	}

	sock = socket(res->ai_family, res->ai_socktype, 0);
	if(sock < 0){
		freeaddrinfo(res);
		fprintf(stderr,"socket error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1; 
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	tv.tv_sec = 0; tv.tv_usec = utime;
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));


	if(length <= sizeof(Editbuffer) - 32){
		memcpy(Editbuffer,data,length);
		memcpy(Editbuffer + length,MessAuthCode,32);
//		SHA256_Init(&c1);
//		SHA256_Update(&c1,Editbuffer,length + 32);
//		SHA256_Final(hash,&c1);

		sha_cal(256,(unsigned char *)Editbuffer,length + 32,hash);

		memcpy(Editbuffer + length,hash,32);
	}

	if(sendto(sock,Editbuffer,length + 32 ,MSG_EOR,res->ai_addr,res->ai_addrlen) == -1){
		freeaddrinfo(res);
		close(sock);
		fprintf(stderr,"Warning : sendto error errno=%d ip=%s.(file=%s line=%d)\n",errno,ip,__FILE__,__LINE__);
		return -1;
	}
	freeaddrinfo(res);
	close(sock);
	return 0;
}
/*****************************************************************************************/
/* 同報データ送信処理*/
/*****************************************************************************************/
static	int	send_data(int utime,int size,unsigned char * buf,unsigned char type)
{
	int			err,rv;
	char			portstr[32];
	struct timeval 		tv;
	unsigned char		*bufp;
	int			remain,port,datalen=0,endflg2=0;
	unsigned char		data[1600],buf3[1600];
	unsigned short		sendlen=0,sendlen2=0;
	int			on2=1,prio2=6;
	KEYLIST			*list;
	int			firstflg;
	int			errflg;
	int			i;

	if((outouflg == 0) && (HassinNo == 0) ){
		return 0;
	}

	for(i=0;i<MAX_CLI;i++){
		if((SendArray[i].used == 1) && (SendArray[i].sock == -1)){
			port = PORT_NUM;
			memset(&(SendArray[i].hints),0,sizeof(SendArray[i].hints));
			SendArray[i].hints.ai_family = AF_UNSPEC;
			SendArray[i].hints.ai_socktype = SOCK_DGRAM;
			sprintf(portstr,"%d",port);
			err = getaddrinfo(SendArray[i].Ip,portstr,&(SendArray[i].hints),&(SendArray[i].res));
			if(err != 0){
				fprintf(stderr,"getaddrinfo error 2.(file=%s line=%d)\n",__FILE__,__LINE__);
				return -1;
			}

			SendArray[i].sock = socket(SendArray[i].res->ai_family, SendArray[i].res->ai_socktype, 0);
			if(SendArray[i].sock < 0){
				freeaddrinfo(SendArray[i].res);
				fprintf(stderr,"socket error.(file=%s line=%d)\n",__FILE__,__LINE__);
				return -1; 
			}
			setsockopt(SendArray[i].sock, SOL_SOCKET, SO_REUSEPORT, &on2, sizeof(on2));
			setsockopt(SendArray[i].sock, SOL_SOCKET, SO_REUSEADDR, &on2, sizeof(on2));
			tv.tv_sec = 0;
			tv.tv_usec = utime;
			setsockopt(SendArray[i].sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,sizeof(tv));
			setsockopt(SendArray[i].sock, SOL_SOCKET, SO_PRIORITY, &prio2,sizeof(prio2));
		}
	}

	remain = size;
	bufp = buf;
	endflg2 = 0;
	firstflg = 1;
	errflg = 0;

	while(remain > 0){

		if(remain > (1200 - 3 - 16)){
			sendlen = 1200 - 3 - 16;
			if(firstflg == 1){
				endflg2 = 0;
				firstflg = 0;
			}else{
				endflg2 = 1;
			}
		}else{
			sendlen = remain;
			endflg2 = 2;
		}
		buf3[0] = endflg2;

		if(IsLittleEndian()){
			sendlen2 = swap16(sendlen);
		}else{
			sendlen2 = sendlen;
		}

		memcpy(&(buf3[1]),&sendlen2,2);
		memcpy(&(buf3[3]),bufp,sendlen);

		for(i=0;i<MAX_CLI;i++){
			pthread_mutex_lock(&m_send_list);
			if(SendArray[i].used == 0){
				pthread_mutex_unlock(&m_send_list);
				continue;
			}
			if(SendArray[i].Sts == 1){
				pthread_mutex_unlock(&m_send_list);
				continue;
			}
			if(SendArray[i].sock < 3){
				pthread_mutex_unlock(&m_send_list);
				continue;
			}
//
// プロトコルの設定
//
			if((SendArray[i].crypto == PRO_SEND_IMG_NO_CRYPTO) ||
			   (SendArray[i].crypto == PRO_SEND_IMG_SUB_NO_CRYPTO) ||
			   (SendArray[i].crypto == PRO_SEND_VOICE_NO_CRYPTO)){
				if(type == PRO_SEND_VOICE){
					data[0] = PRO_SEND_VOICE_NO_CRYPTO;
				}else if(type == PRO_SEND_IMG){
					data[0] = PRO_SEND_IMG_NO_CRYPTO;
				}else if(type == PRO_SEND_IMG_SUB){
					data[0] = PRO_SEND_IMG_SUB_NO_CRYPTO;
				}
				datalen = sendlen + 3;
				memcpy(&data[1],buf3,datalen);
			}else{
//
// 最初の１バイトは暗号化しない！！
//
//			memset(data,0x00,sizeof(data));	// valgrindのエラーを無くすために必要
				data[0] = type;

				list = list_from_ip(SendArray[i].Ip);
				if(list == NULL){
					continue;
				}

				pthread_mutex_lock(&(list->m_ctx_enc));
				rv = encrypt_data_r(list,sendlen + 3,buf3,&datalen,&(data[1]));
				pthread_mutex_unlock(&(list->m_ctx_enc));

				if(rv != 0){
					fprintf(stderr,"encrypt_data err rv=%d len=%d.(file=%s line=%d)\n",rv,datalen,__FILE__,__LINE__);
				}
			}
//			pthread_mutex_lock(&(SendArray[i].m_sock));
			if(sendto(SendArray[i].sock,data,datalen + 1,MSG_EOR,
				SendArray[i].res->ai_addr,SendArray[i].res->ai_addrlen) == -1){

				SendArray[i].Sts = 1;
				errflg = 1;

				fprintf(stderr,"Warning : sendto error errno=%d Name=%s.(file=%s line=%d)\n",errno,name_from_ip(SendArray[i].Ip),__FILE__,__LINE__);
			}else{
				if(endflg2 == 2){
					if((type == PRO_SEND_IMG)  || (type == PRO_SEND_IMG_SUB)){
//						SendArray[i].image++;
						sfps++;
					}else if((type == PRO_SEND_VOICE)){
						svfps++;
					}
				}
			}
//			pthread_mutex_unlock(&(SendArray[i].m_sock));

			pthread_mutex_unlock(&m_send_list);
		}

		if(endflg2 == 2){
			break;
		}
		remain -= sendlen;
		bufp += sendlen;
	}
//
// 通信エラーになったエントリを取り除く
//
	if(errflg){
		del_send_list2();
	}

	return 0;
}
/*****************************************************************************************/
/* 以下は音声及び画像ソケット受信スレッド*/
/*****************************************************************************************/
static	int do_recv(unsigned char * buf,char	*Ip)
{
	unsigned short		length;
	unsigned char		pro,flg;
	int			remain;
	RECVLIST		*list;
	KEYLIST			*klist;
//	int			csize;


	pro = *buf;
	flg  = *(buf + 1);
	memcpy(&length,buf+2,2);

	if(IsLittleEndian()){
		length = swap16(length);
	}
	if((length > 1500 ) || 
		((pro != PRO_SEND_VOICE) && (pro != PRO_SEND_IMG) && (pro != PRO_SEND_IMG_SUB) &&
		(pro != PRO_SEND_VOICE_NO_CRYPTO) && (pro != PRO_SEND_IMG_NO_CRYPTO) && 
		(pro != PRO_SEND_IMG_SUB_NO_CRYPTO)) || 
		((flg != 0) && (flg != 1) && (flg != 2))){
		fprintf(stderr,"******* wrong packet length=%d pro=%d flg=%d.(file=%s line=%d)\n",length,pro,flg,__FILE__,__LINE__);
		return -1;
	}

	list = add_recv_list(Ip,pro);
	if(list == NULL){
		return 0;
	}

	if((list->added == 0) && (list->ch == 0)){
//printf("do_recv add_send_list ch=%d Ip=%s\n",list->ch,list->Ip);

		add_send_list(list->Ip,pro);
		list->added = 1;

		klist = key_list_top;
		while(klist){
			if((strcmp(Ip,klist->Ip) == 0) && (klist->Kind == KIND_CALLING)){
				klist->Kind = klist->SaveKind;
				klist->CallCnt = 0;
				break;
			}
			klist = klist->next;
		}
	}

	clock_gettime(CLOCK_REALTIME,&(list->last_recv_time));

	if((pro == PRO_SEND_VOICE) || (pro == PRO_SEND_VOICE_NO_CRYPTO)){

		if(flg == 2){
			rvfps++;
		}else{
			fprintf(stderr,"************* Voice packet flagmented!! **************(file=%s line=%d)\n",__FILE__,__LINE__);
		}

		if(list->init_complete == 0){
			return 0;
		}

		if(list->tid == -1){				// sound write thread なし
			return 0;
		}

//		csize = sizeof(sound_buffer3);
//		if(DataDeCompress(buf + 4,(int)length,sound_buffer3,&csize)){
//			fprintf(stderr,"DataDeCompress error.(file=%s line=%d)\n",__FILE__,__LINE__);
//			return 0;
//		}

//		length = csize;

		if((unsigned long)(list->writep) < (unsigned long)(list->readp)){
			if(((unsigned long)(list->readp) - (unsigned long)(list->writep)) <= chunk_size){
				fprintf(stderr,"buf overflow data from %s.(file=%s line=%d)\n",name_from_ip(list->Ip),__FILE__,__LINE__);
			}
		}

		remain = SOUND_BUFSIZE  - ((unsigned long)(list->writep) - (unsigned long)(list->pcm_buf));
		if(remain == length){
//			memcpy(list->writep,sound_buffer3,length);
			memcpy(list->writep,buf + 4,length);
			list->writep = list->pcm_buf;
		}else if(remain > length){
// 			memcpy(list->writep,sound_buffer3,length);
			memcpy(list->writep,buf + 4,length);
			list->writep += length;
		}else{
//			memcpy(list->writep,sound_buffer3,remain);
			memcpy(list->writep,buf + 4,remain);
//			memcpy(list->pcm_buf,sound_buffer3 + remain,length - remain);
			memcpy(list->pcm_buf,buf + 4 + remain,length - remain);
			list->writep = list->pcm_buf + (length - remain);
		}

		return 0;
	}

	clock_gettime(CLOCK_REALTIME,&(list->last_recv_time2));

	if(list->ImageRecv != 0){
		if((list->ImageRecv == 2) || (list->ImageRecv == 100)){
			list->ImageRecv = 0;
		}else if((list->ImageRecv == 1) && (flg == 2)){
			rfps++;
			list->total2 = 0;
//			printf("*** data from %s is losted *** ImageRecv=%d\n",name_from_ip(list->Ip),list->ImageRecv);
			return 0;
		}else{
			if((list->total2 + length) < SECOND_BUF_SIZE){
				memcpy(list->image_buf2+list->total2,buf+4,length);
				list->total2 += length;
			}else{
//				printf("*** data from %s will lost *** ImageRecv=%d\n",name_from_ip(list->Ip),list->ImageRecv);
				list->total2 = 0;
			}
			return 0;
		}
	}

	if(list->total2){
		memcpy(list->bufp,list->image_buf2,list->total2);
		list->bufp += list->total2;
		list->total = list->total2;
		list->total2 = 0;
	}

	if(flg == 0){			// first
		list->bufp = list->image_buf;
		list->total = length;
	}else if(flg == 1){		// next
		list->total += length;
	}else{				// end
		list->total += length;
	}

	if(list->total >= MAX_JPEG_SIZE){
		fprintf(stderr,"jpeg buffer over flow size=%d.(file=%s line=%d)\n",list->total,__FILE__,__LINE__);
		list->total = 0;
		list->bufp = list->image_buf;
		return -1;
	}

	memcpy(list->bufp,buf+4,length);
	list->bufp += length;


	if(flg != 2){
		return 0;
	}

	rfps++;

	if(list->total < MIN_JPEG_SIZE){
		list->total = 0;
		list->bufp = list->image_buf;
		return 0;
	}

	list->SurfWritedCounter = 0;

	list->ImageRecv = 1;

////	gtk_widget_queue_draw(drawingarea);

	return 0;
}
static void p_message(char *mess,char *name)
{
	time_t 			timer;
	struct tm 		*local;

	timer = time(NULL);
	local = localtime(&timer);

	fprintf(stderr,"%02d:%02d:%02d %s %s.\n",local->tm_hour,local->tm_min,local->tm_sec,mess,name);
}
/*****************************************************************************************/
/* 初期画面表示処理*/
/*****************************************************************************************/
void	display_surf(cairo_surface_t *surf,int startx,int starty,char *Ip)
{
//	char			rec_buf[256];
//	time_t 			timer;
//	struct tm 		*local;

	if(init_image_flg == 0){
		return;
	}
	if(surf == NULL){
		return;
	}


	cairo_set_source_surface (gcr, surf, startx, starty);
	cairo_paint(gcr);
	draws++;

/*	if(recflg && (startx == 0) && (starty ==0)){
		cairo_set_source_rgb(gcr, 1.0, 0.0, 0.0);
		cairo_set_font_size(gcr, 25);
		cairo_move_to(gcr, 30, 30);

		timer = time(NULL);
		local = localtime(&timer);

		sprintf(rec_buf,"Rec %d/%02d/%02d %02d:%02d:%02d",
				local->tm_year + 1900,
				local->tm_mon + 1,local->tm_mday,
				local->tm_hour,local->tm_min,local->tm_sec);

		cairo_show_text(gcr, rec_buf);
	}
*/
	if(Ip){
		cairo_set_source_rgb(gcr, 0.0, 0.5, 0.0);
		cairo_set_font_size(gcr, 100);
		cairo_move_to(gcr, startx + 20, starty + (height - 20));
		cairo_show_text(gcr, name_from_ip(Ip));
	}

	cairo_set_source_rgb(gcr, 0.0, 1.0, 0.0);
	cairo_rectangle(gcr,startx, starty,width, 5);			// 上
	cairo_rectangle(gcr,startx, starty + height -5,width,5);	// 下

	cairo_rectangle(gcr,startx, starty, 5,height);			// 左
	cairo_rectangle(gcr,startx + width -5, starty-5 ,5,height);	// 右
	cairo_fill(gcr);


	gdk_display_flush(gdk_display_get_default());

}
static	void set_init_image(int startx,int starty)
{
	FILE			*fp;
	int			rv;
	struct stat 		st;
	int			i;
	char			fname[256];


	if(init_image_flg == 0){
		for(i=1;i<=4;i++){
			sprintf(fname,"./init%d.jpg",i);
			stat(fname,&st);
			if(st.st_size >= MAX_JPEG_SIZE){
				fprintf(stderr,"file size is too large fname=%s.(file=%s line=%d)\n",fname,__FILE__,__LINE__);
				return;
			}
			fp = fopen(fname,"r");
			if(fp == NULL){
				fprintf(stderr,"file open error fname=%s.(file=%s line=%d)\n",fname,__FILE__,__LINE__);
				return;
			}
			rv = fread(init_image_buf,1,st.st_size,fp);
			if(rv != st.st_size){
				fprintf(stderr,"file read error fname=%s.(file=%s line=%d)\n",fname,__FILE__,__LINE__);
				fclose(fp);
				return;
			}
			fclose(fp);
			if(i==1){
				surf = make_cairo_surface(NULL,init_image_buf, st.st_size,1);
				if(surf == NULL){
					fprintf(stderr,"make_cairo_surface error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
				if(cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS){
					fprintf(stderr,"cairo_surface_status error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
			}else if(i == 2){
				surf2 = make_cairo_surface(NULL,init_image_buf, st.st_size,1);
				if(surf2 == NULL){
					fprintf(stderr,"make_cairo_surface 2 error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
				if(cairo_surface_status(surf2) != CAIRO_STATUS_SUCCESS){
					fprintf(stderr,"cairo_surface_status 2 error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
			}else if(i == 3){
				surf3 = make_cairo_surface(NULL,init_image_buf, st.st_size,1);
				if(surf3 == NULL){
					fprintf(stderr,"make_cairo_surface 3 error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
				if(cairo_surface_status(surf3) != CAIRO_STATUS_SUCCESS){
					fprintf(stderr,"cairo_surface_status 3 error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
			}else{
				surf4 = make_cairo_surface(NULL,init_image_buf, st.st_size,1);
				if(surf4 == NULL){
					fprintf(stderr,"make_cairo_surface 4 error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}
				if(cairo_surface_status(surf4) != CAIRO_STATUS_SUCCESS){
					fprintf(stderr,"cairo_surface_status 4 error.(file=%s line=%d)\n",__FILE__,__LINE__);
					return;
				}

			}
			init_image_flg = 1;
		}
	}

	display_surf(surf,startx,starty,NULL);

	return;
}
static	void set_init_image_all()
{
	int		i;

	if(PinPmode == 1){
		return;
	}

	if(cur_sess_no == 0){
		set_init_image(ent_table1[0].startx,ent_table1[0].starty);
	}else if(cur_sess_no == 2){
		;
	}else if(cur_sess_no == 3){
		cairo_scale(gcr, (double)1.0/(double)2.0, (double)1.0);
		display_surf(surf3,0,height,NULL);
		display_surf(surf3,width*3/4*4,height,NULL);
		cairo_scale(gcr, (double)2.0, (double)1.0);
	}else if(cur_sess_no == 4){
		for(i=0;i<4;i++){
			if(ent_table4[i].used == 0){
				set_init_image(ent_table4[i].startx,ent_table4[i].starty);
			}
		}
	}else if((5 <= cur_sess_no) && (cur_sess_no <= 9)){
		for(i=0;i<9;i++){
			if(ent_table9[i].used == 0){
				set_init_image(ent_table9[i].startx,ent_table9[i].starty);
			}
		}
	}else if((10 <= cur_sess_no) && (cur_sess_no <= 16)){
		for(i=0;i<16;i++){
			if(ent_table16[i].used == 0){
				set_init_image(ent_table16[i].startx,ent_table16[i].starty);
			}
		}
	}else if((17 <= cur_sess_no) && (cur_sess_no <= 25)){
		for(i=0;i<25;i++){
			if(ent_table25[i].used == 0){
				set_init_image(ent_table25[i].startx,ent_table25[i].starty);
			}
		}
	}else if((26 <= cur_sess_no) && (cur_sess_no <= 36)){
		for(i=0;i<36;i++){
			if(ent_table36[i].used == 0){
				set_init_image(ent_table36[i].startx,ent_table36[i].starty);
			}
		}
	}else if((37 <= cur_sess_no) && (cur_sess_no <= 49)){
		for(i=0;i<49;i++){
			if(ent_table49[i].used == 0){
				set_init_image(ent_table49[i].startx,ent_table49[i].starty);
			}
		}
	}

	return;
}
/*****************************************************************************************/
/* 以下は通信セッション処理*/
/*****************************************************************************************/
void printf_st(const char *format, ...)
{
#if	1	
	va_list va;

	va_start(va, format);
	vprintf(format, va);
	va_end(va);
#endif
}


static	void	disp_event(unsigned char ev)
{
	switch(ev){
		case CMD_START_SESSION:
//			printf_st("*** START_SESSION ***\n");
			printf_st("event START_SESSION\n");
			break;
		case CMD_ACCEPT:
			printf_st("recv ACCEPT\n");
			break;
		case CMD_SEND_REQ:
			printf_st("recv SEND_REQ\n");
			break;
		case CMD_RESP_REQ:
			printf_st("recv RESP_REQ\n");
			break;
		case CMD_SEND_A:
			printf_st("recv SEND_A\n");
			break;
		case CMD_RESP_A:
			printf_st("recv RESP_A\n");
			break;
		case CMD_SEND_B:
			printf_st("recv SEND_B\n");
			break;
		case CMD_RESP_B:
			printf_st("recv RESP_B\n");
			break;
		case CMD_SEND_X:
			printf_st("recv SEND_X\n");
			break;
		case CMD_RESP_X:
			printf_st("recv RESP_X\n");
			break;
		case CMD_SEND_Y:
			printf_st("recv SEND_Y\n");
			break;
		case CMD_RESP_Y:
			printf_st("recv RESP_Y\n");
			break;
		case CMD_SEND_KEY:
			printf_st("recv SEND_KEY\n");
			break;
		case CMD_RESP_KEY:
			printf_st("recv RESP_KEY\n");
			break;
		case CMD_TIMEOUT:
			printf_st("event TIMEOUT\n");
			break;
		default:
//			printf_st("recv invalid cmd\n");
			break;
	}
}

static	int	gmp_init_flg=0;
static	gmp_randstate_t		state;

static int	clear_gmp()
{
	if(gmp_init_flg){
		mpz_clear(SP);
		mpz_clear(G);
		mpz_clear(a);
		mpz_clear(b);
		mpz_clear(A);
		mpz_clear(B);
		mpz_clear(x);
		mpz_clear(y);
		mpz_clear(X);
		mpz_clear(Y);
		mpz_clear(W1);
		mpz_clear(W2);
		mpz_clear(W3);
		mpz_clear(W4);
		mpz_clear(e);
		mpz_clear(d);
		mpz_clear(seed);

		gmp_randclear(state);
	}
	gmp_init_flg=0;
	return 0;
}
static	void	init_auth_code(char *auth)
{
	unsigned char		random_data[128];
	FILE			*fp;

	fp = fopen("/dev/urandom","r");
	if(fp == NULL){
		fprintf(stderr,"urandom open error1.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	if(fread(random_data,1,sizeof(random_data),fp) != sizeof(random_data)){
		fprintf(stderr,"urandom read error1.(file=%s line=%d)\n",__FILE__,__LINE__);
		fclose(fp);
		return;
	}
	fclose(fp);

	sha_cal(256,random_data,sizeof(random_data),(unsigned char *)auth);

	return;
}
static int	init_gmp()
{
//	SHA256_CTX		c1;
	unsigned char		hash1[32],hashdata1[65],random_data[128];
	unsigned char		*p;
	FILE			*fp;
	int			i;

	mpz_init(SP);
	mpz_init(G);
	mpz_init(a);
	mpz_init(b);
	mpz_init(A);
	mpz_init(B);
	mpz_init(x);
	mpz_init(y);
	mpz_init(X);
	mpz_init(Y);
	mpz_init(W1);
	mpz_init(W2);
	mpz_init(W3);
	mpz_init(W4);
	mpz_init(e);
	mpz_init(d);
	mpz_init(seed);


	mpz_set_str(SP,SPSTR, BASE);
	mpz_set_str(G,GENSTR,BASE);

	fp = fopen("/dev/urandom","r");
	if(fp == NULL){
		fprintf(stderr,"urandom open error2.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(fread(random_data,1,sizeof(random_data),fp) != sizeof(random_data)){
		fprintf(stderr,"urandom read error2.(file=%s line=%d)\n",__FILE__,__LINE__);
		fclose(fp);
		return -1;
	}
	fclose(fp);

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,random_data,sizeof(random_data));
//	SHA256_Final(hash1,&c1);

	sha_cal(256,random_data,sizeof(random_data),hash1);
	p = hash1;
	hashdata1[0] = 0x00;
	for(i=0;i<32;i++){
		sprintf((char *)&(hashdata1[i*2]),"%02x",*p);
		p++;
	}


	mpz_init_set_str(seed,(char *)hashdata1,16);
	gmp_randinit_default (state);
	gmp_randseed(state,seed);

	mpz_urandomm(a,state,SP);
	mpz_urandomm(x,state,SP);
	mpz_urandomm(b,state,SP);
	mpz_urandomm(y,state,SP);

	mpz_powm(A,G,a,SP);
	mpz_powm(B,G,b,SP);
	mpz_powm(X,G,x,SP);
	mpz_powm(Y,G,y,SP);

	gmp_init_flg = 1;

	return 0;
}

//static	char    *sA,sB[4096];
static	char    sA[4096],sB[4096];
static	char    sYA[4096],sXB[4096];

void	cal_key()
{
	unsigned char		hash1[32],hash2[32],hashdata1[65],hashdata2[65];
	unsigned char		*p;
	int			i;
//	SHA256_CTX		c1;


	mpz_get_str(sA,BASE,A);
	mpz_get_str(sYA,BASE,Y);
	strcat(sYA,sA);

	mpz_get_str(sB,BASE,B);
	mpz_get_str(sXB,BASE,X);
	strcat(sXB,sB);

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,sYA,strlen(sYA));
//	SHA256_Final(hash1,&c1);
	sha_cal(256,(unsigned char *)sYA,strlen(sYA),hash1);

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,sXB,strlen(sXB));
//	SHA256_Final(hash2,&c1);
	sha_cal(256,(unsigned char *)sXB,strlen(sXB),hash2);

	p = hash1;
	hashdata1[0] = 0x00;
	for(i=0;i<32;i++){
		sprintf((char *)&(hashdata1[i*2]),"%02x",*p);
		p++;
	}
	
	p = hash2;
	hashdata2[0] = 0x00;
	for(i=0;i<32;i++){
		sprintf((char *)&(hashdata2[i*2]),"%02x",*p);
		p++;
	}

	mpz_init_set_str(e,(char *)hashdata1,16);
	mpz_init_set_str(d,(char *)hashdata2,16);

	mpz_mod(e,e,SP);
	mpz_mod(d,d,SP);

// 共有データの計算（送信元）
	mpz_powm(W1,B,e,SP);
	mpz_mul(W1,W1,Y);
	mpz_mul(W2,a,d);
	mpz_add(W2,W2,x);
	mpz_powm(W3,W1,W2,SP);

// 共有データの計算（受信先）
	mpz_powm(W1,A,d,SP);
	mpz_mul(W1,W1,X);
	mpz_mul(W2,b,e);
	mpz_add(W2,W2,y);
	mpz_powm(W4,W1,W2,SP);


/*
// 共有データの計算（送信元）
	mpz_mul(W1,B,Y);
//	mpz_mod(W1,W1,SP);
	mpz_add(W2,a,x);
//	mpz_mod(W2,W2,SP);
	mpz_powm(W3,W1,W2,SP);

// 共有データの計算（受信先）
	mpz_mul(W1,A,X);
//	mpz_mod(W1,W1,SP);
	mpz_add(W2,b,y);
//	mpz_mod(W2,W2,SP);
	mpz_powm(W4,W1,W2,SP);
*/

	return;
}
int my_random(unsigned char *rword,mpz_t N,mpz_t e)
{
	struct  timespec        ts;
//	SHA256_CTX              c1;
	unsigned char           *p,hash[32],hashdata[65];
	__int64_t               *nsecp;
	__uint64_t              *tscp;
	int                     rv,i;

	mpz_init(e);


	clock_gettime(CLOCK_REALTIME, &ts);
	tscp = (__uint64_t *)(rword+100);
	nsecp =(__int64_t *)(rword+108);
	*tscp = random();
	*nsecp = ts.tv_nsec;

//	SHA256_Init(&c1);
//	SHA256_Update(&c1,rword,116);
//	SHA256_Final(hash,&c1);

	sha_cal(256,rword,116,hash);

	p = hash;
	hashdata[0] = 0x00;
	for(i=0;i<32;i++){
		sprintf((char *)&(hashdata[i*2]),"%02x",*p);
		p++;
	}

	mpz_init_set_str(e,(char *)hashdata,16);
	mpz_mod(e,e,N);

	rv = (int)mpz_get_ui(e);

	mpz_clear(e);

	return rv;
}

int put_number(FILE *fp,unsigned char *rword,int no)
{
	int             i,ari,add,k,j,len,rand;
	unsigned int    *com;
	char            tmpbuf[128],buf[4096];
	mpz_t           N,e;

	mpz_init(N);
//	mpz_init(e);

	add = 0;
	sprintf(tmpbuf,"%d",no);
	mpz_set_str(N,tmpbuf, 10);

	com = (unsigned int *)calloc(1,no*4+4);
	if(com == NULL ){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	for(i=0;i<no+1;i++){
		com[i] = -1;
	}

	while(1){
		rand = my_random(rword,N,e)%no;
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
	com = NULL;
 
	mpz_clear(N);

	return 0;
}

int	gen_key_file(char *fname)
{
	int		threshold,hashsize,bitsno,iv_len;
	FILE		*fp;
	unsigned char	randomword[100+17];	
	char		cmd[300];
	int		i;
	char		buf[121];


	threshold = 1200;
	hashsize = 32;
	bitsno = 8;
	iv_len = 32;

	fp = fopen("/dev/urandom","r");
	if(fp == NULL){
		fprintf(stderr,"urandom open error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(fread(randomword,1,sizeof(randomword),fp) != sizeof(randomword)){
		fprintf(stderr,"urandom read error.(file=%s line=%d)\n",__FILE__,__LINE__);
		fclose(fp);
		return -1;
	}
	fclose(fp);

        fp = fopen(fname,"w");
        if(fp == NULL){
                fprintf(stderr,"fopen error filename=%s.(file=%s line=%d)\n",fname,__FILE__,__LINE__);
                return -1;
        }

	fwrite("[com_last1]\n",12,1,fp);
	put_number(fp,randomword,(threshold + 2 + hashsize + iv_len)*8/bitsno);

	fwrite("[com_last2]\n",12,1,fp);
	put_number(fp,randomword,(threshold + 2 + hashsize + iv_len*2)*8/bitsno);

//	fwrite("[com_else]\n",11,1,fp);
//	put_number(fp,randomword,threshold*8/bitsno);

        fwrite("[aes_gcm]\n",10,1,fp);

        for(i=0;i<60;i++){
                sprintf(&buf[i*2],"%02x",randomword[i]);
        }
        buf[120] = '\n';
        fwrite(buf,121,1,fp);

	fclose(fp);

	sprintf(cmd,"chmod 0600 %s",fname);
	my_system(cmd);

	return 0;
}

static unsigned char	*FileBuffer1;					// 変換前のファイルデータ
static unsigned char	*FileBuffer2;					// 変換後のファイルデータ TAGを含む
static	int		KeyFileSize=0;					// 暗号化後のファイルサイズ+taglen
static	int		SendDataLen=0;					// 送信済みデータ長を格納する
static	int		RecvDataLen=0;					// 受信済みデータ長を格納する
static	int	combert_key_file1(char *fname,char *share_data)
{
//	SHA512_CTX              c0;
	KEYLIST			list;
	unsigned char           hash[64];
	struct stat 		st;
	FILE			*fp;
	int			rv,size;
//
// AESのキーと初期化ベクトルを作成する
//
//	SHA512_Init(&c0);
//	SHA512_Update(&c0,share_data,strlen(share_data));
//	SHA512_Final(hash,&c0);

	sha_cal(512,(unsigned char *)share_data,strlen(share_data),hash);
//
// キーファイルの中身を暗号化する
//
	stat(fname,&st);
	size = st.st_size;
	if(size >= FILE_BUF_SIZE){
		fprintf(stderr,"FileBuffer error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	fp = fopen(fname,"r");
	if(fp == NULL){
		return -1;
	}
	memset(FileBuffer1,0x00,FILE_BUF_SIZE);
	memset(FileBuffer2,0x00,FILE_BUF_SIZE);
	rv = fread(FileBuffer1,1,size,fp);
	if(rv != size){
		fclose(fp);
		return -1;
	}
	fclose(fp);

	memset(&list,0x00,sizeof(list));
	memcpy(list.aes_key,hash,60);
	list.en = EVP_CIPHER_CTX_new();

	aes_enc(&list,size,FileBuffer1,&KeyFileSize,FileBuffer2);

	EVP_CIPHER_CTX_free(list.en);

//	dump(FileBuffer1,size);

	return 0;
}


static	int	combert_key_file2(char *share_data)
{
//	SHA512_CTX              c0;
	KEYLIST			list;
	unsigned char           hash[64];
	int			rv,len;
	FILE			*fp;
	char			FileName[256],cmd[300];
//
// AESのキーと初期化ベクトルを作成する
//
//	SHA512_Init(&c0);
//	SHA512_Update(&c0,share_data,strlen(share_data));
//	SHA512_Final(hash,&c0);

	sha_cal(512,(unsigned char *)share_data,strlen(share_data),hash);
//
// 受信データを復号化する
//
	memset(&list,0x00,sizeof(list));
	memcpy(list.aes_key,hash,60);
	list.ed = EVP_CIPHER_CTX_new();

	rv = aes_dec(&list,RecvDataLen,FileBuffer1,&len,FileBuffer2);

	EVP_CIPHER_CTX_free(list.ed);
//
// TAGをチェックする
//
	if(rv != 0){
		fprintf(stderr,"tag error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
//
// キーファイルを保存する。モードの変更も必要！！
//
	sprintf(FileName,"/tmp/ihaphone_%s.key",SessionIp);
	fp = fopen(FileName,"w");
	if(fp == NULL){
		fprintf(stderr,"File create error for Session.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	fwrite(FileBuffer2,len,1,fp);
//	fwrite(FileBuffer2,RecvDataLen - 16,1,fp);
	fclose(fp);

	sprintf(cmd,"chmod 0600 %s",FileName);
	my_system(cmd);

//	dump(FileBuffer2,strlen((char *)FileBuffer2));

	return 0;
}


static	char	PartnerName[128];

static	void	MakeTempFileName(char *KeyFileName)
{
	int	cnt;
	int	tmp;
	FILE	*fp;
	char	Name[256];
	char	KeyFileName2[1024];
	
	srandom((unsigned int)time(NULL));

retry:
	Name[0] = '1';
	Name[1] = '2';
	Name[2] = '3';
	for(cnt=3;cnt<19;){
		tmp = random()%128;

		if(((0x30 <= tmp) && (tmp <= 0x39)) || ((0x41 <= tmp) && (tmp <= 0x5a)) || ((0x61 <= tmp) && ( tmp <= 0x7a))){
			Name[cnt] = tmp;
			cnt++;
		}
	}
	Name[cnt] = 0x00;
	sprintf(KeyFileName,"%s.key",Name);
	sprintf(KeyFileName2,"%s/%s.key",key_file_dir,Name);
//
// 既に存在する場合はやり直し
//
	fp = fopen(KeyFileName2,"r");
	if(fp != NULL){
		fclose(fp);
		goto retry;
	}


	return;
}
static	int	copy_file(char *fname1,char *fname2)
{
	FILE	*fp1,*fp2;
	char	buf[2];

	fp1 = fopen(fname1,"r");
	if(fp1 == NULL){
		return -1;
	}
	fp2 = fopen(fname2,"w");
	if(fp2 == NULL){
		fclose(fp1);
		return -1;
	}

	while(1){
		if(fread(buf,1,1,fp1) <= 0){
			if(feof(fp1)){
				break;
			}else{
				fclose(fp1);
				fclose(fp2);
				return -1;
			}
		}
		if(fwrite(buf,1,1,fp2) < 0){
			fclose(fp1);
			fclose(fp2);
			return -1;
		}
	}

	fclose(fp1);
	fclose(fp2);
	return 0;
}
static	int	update_address_book(void)
{
	FILE			*fp1,*fp2;
	char			buf[1024],buf2[1024],buf3[1024+100],*str,*stop;
	char			Name[128],Ip[64],Kind[32],KeyFileName[256],NewName[256];
	KEYLIST			*list,*list2;
	int			found;


	fp1 = fopen(addr_book_file,"r");
	if(fp1 == NULL){
		fprintf(stderr,"update_address_book error(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	sprintf(buf,"%s.tmp",addr_book_file);
	fp2 = fopen(buf,"w");
	if(fp2 == NULL){
		fclose(fp1);
		fprintf(stderr,"update_address_book error(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	MakeTempFileName(NewName);
	found = 0;

	while(1){
		memset(buf,0x00,sizeof(buf));
		memset(buf2,0x00,sizeof(buf2));
		Name[0]        = 0x00;
		Ip[0]          = 0x00;
		Kind[0]        = 0x00;
		KeyFileName[0] = 0x00;

		if(fgets(buf,sizeof(buf),fp1) == NULL){
			break;
		}
		my_strncpy(buf2,buf,sizeof(buf2));
		if((buf[0] == '#') || buf[0] == '\n'){
			fwrite(buf,strlen(buf),1,fp2);
			continue;
		}

		str = strtok_r(buf,",\n",&stop);
		if(str){
			my_strncpy(Name,str,sizeof(Name));
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str){
			my_strncpy(Ip,str,sizeof(Ip));
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str){
			my_strncpy(Kind,str,sizeof(Kind));
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str){
			my_strncpy(KeyFileName,str,sizeof(KeyFileName));
		}

		if(strcmp(Ip,SessionIp) == 0){

//NG			sprintf(buf3,"rm %s",KeyFileName);
//NG			my_system(buf3);

			sprintf(buf3,"%s,%s,%s,%s\n",PartnerName,SessionIp,Kind,NewName);
			fwrite(buf3,strlen(buf3),1,fp2);
			found = 1;
		}else{
			fwrite(buf2,strlen(buf2),1,fp2);
		}
	}

	if(found == 0){
		if(key_entry_no >= max_key_list_no){
			printf("Address book is full.\n");
			return -1;
		}
		my_strncpy(Kind,"ALLOW",sizeof(Kind));
		sprintf(buf3,"%s,%s,%s,%s\n",PartnerName,SessionIp,Kind,NewName);
		fwrite(buf3,strlen(buf3),1,fp2);
	}

	fclose(fp1);
	fclose(fp2);

//スペースに対応出来ない	sprintf(buf3,"cp -a ./%s.key %s/%s",SessionIp,key_file_dir,NewName);
//	my_system(buf3);

	sprintf(buf2,"/tmp/ihaphone_%s.key",SessionIp);
	sprintf(buf3,"%s/%s",key_file_dir,NewName);
	if(copy_file(buf2,buf3) < 0){
		return -1;
	}

	sprintf(buf3,"rm /tmp/ihaphone_%s.key",SessionIp);
	my_system(buf3);

	sprintf(buf3,"mv %s %s.bak",addr_book_file,addr_book_file);
	my_system(buf3);
	sprintf(buf3,"mv %s.tmp %s",addr_book_file,addr_book_file);
	my_system(buf3);
	sprintf(buf3,"chmod 0600 %s",addr_book_file);
	my_system(buf3);
	my_system("sync");
//
// メモリ上のkey_listを更新する。
//
	if(found == 1){
//
// key_listにある場合はcipherを置き換える。
//
		list = key_list_top;

		while(list){
			if(strcmp(list->Ip,SessionIp) == 0){
				my_strncpy(list->Name,PartnerName,sizeof(list->Name));
				sprintf(buf3,"%s/%s",key_file_dir,NewName);
				my_strncpy(list->KeyFileName,buf3,sizeof(list->KeyFileName));

				pthread_mutex_lock(&(list->m_ctx_enc));

				free_cipher(list);
// KEYの設定
				if(alloc_cipher(list) != 0){
					pthread_mutex_unlock(&(list->m_ctx_enc));
					fprintf(stderr,"update_address_book error(file=%s line=%d)\n",__FILE__,__LINE__);
					return -1;
				}
				pthread_mutex_unlock(&(list->m_ctx_enc));
				return 0;
			}
			list = list->next;
		}
	}
//
// key_listにない場合は追加する。
//
	if(key_entry_no >= max_key_list_no){
		fprintf(stderr,"Address book is full.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}


	list = (KEYLIST *)calloc(1,sizeof(KEYLIST));
	if(list == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	my_strncpy(list->Ip,SessionIp,sizeof(list->Ip));
	my_strncpy(list->Name,PartnerName,sizeof(list->Name));
	my_strncpy(list->KeyFileName,NewName,sizeof(list->KeyFileName));
	list->next = NULL;
	list->DenyCnt = 0;
	list->CallCnt = 0;

	if(strcmp(Kind,"ALLOW") == 0){
		list->Kind = KIND_ALLOW;
	}else if(strcmp(Kind,"DENY") == 0){
		list->Kind = KIND_DENY;
	}else if(strcmp(Kind,"HOT") == 0){
		list->Kind = KIND_HOT;
	}
// KEYの設定
	pthread_mutex_init(&(list->m_ctx_enc),NULL);
//	pthread_mutex_init(&(list->m_ctx_dec),NULL);

	sprintf(buf3,"%s/%s",key_file_dir,NewName);
	my_strncpy(list->KeyFileName,buf3,sizeof(list->KeyFileName));

	pthread_mutex_lock(&(list->m_ctx_enc));
	if(alloc_cipher(list) != 0){
		pthread_mutex_unlock(&(list->m_ctx_enc));
		fprintf(stderr,"update_address_book error(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	if(key_list_top == NULL){
		key_list_top = list;
	}else{
		list2 = key_list_top;
		while(list2->next){
			list2 = list2->next;
		}
		list2->next = list;
	}
	key_entry_no++;
	pthread_mutex_unlock(&(list->m_ctx_enc));
	return 0;
}

static	int	update_params(char *item,char *value)
{
	FILE			*fp1,*fp2;
	char			param_file[64],buf[1024],buf2[1024],buf3[1024+100],*str,*stop;


	my_strncpy(param_file,"./ihaphone-params.txt",sizeof(param_file));
	fp1 = fopen(param_file,"r");
	if(fp1 == NULL){
		return -1;
	}
	sprintf(buf,"%s.tmp",param_file);
	fp2 = fopen(buf,"w");
	if(fp2 == NULL){
		fclose(fp1);
		return -1;
	}

	while(1){
		if(fgets(buf,sizeof(buf),fp1) == NULL){
			break;
		}
		my_strncpy(buf2,buf,sizeof(buf2));
		if((buf[0] == '#') || buf[0] == '\n'){
			fwrite(buf,strlen(buf),1,fp2);
			continue;
		}

		str = strtok_r(buf,"=\n",&stop);
		if(str){
			if(strcmp(item,str) == 0){
				sprintf(buf3,"%s=%s\n",item,value);
				fwrite(buf3,strlen(buf3),1,fp2);
			}else{
				fwrite(buf2,strlen(buf2),1,fp2);
			}
		}else{
			fwrite(buf2,strlen(buf2),1,fp2);
		}
	}

	fclose(fp1);
	fclose(fp2);

	sprintf(buf3,"mv %s.tmp %s",param_file,param_file);
	my_system(buf3);
	sprintf(buf3,"chmod 0600 %s",param_file);
	my_system(buf3);

	return 0;
}






static	int	update_address_book2(void)
{
	FILE			*fp1,*fp2;
	char			buf[1024],buf2[1024],buf3[1024+100],*str,*stop;
	char			Name[128],Ip[64],Kind[32],KeyFileName[256];
	KEYLIST			*list;


	fp1 = fopen(addr_book_file,"r");
	if(fp1 == NULL){
		return -1;
	}
	sprintf(buf,"%s.tmp",addr_book_file);
	fp2 = fopen(buf,"w");
	if(fp2 == NULL){
		fclose(fp1);
		return -1;
	}

	while(1){
		memset(buf,0x00,sizeof(buf));
		memset(buf2,0x00,sizeof(buf2));
		Name[0]        = 0x00;
		Ip[0]          = 0x00;
		Kind[0]        = 0x00;
		KeyFileName[0] = 0x00;

		if(fgets(buf,sizeof(buf),fp1) == NULL){
			break;
		}
		my_strncpy(buf2,buf,sizeof(buf2));
		if((buf[0] == '#') || buf[0] == '\n'){
			fwrite(buf,strlen(buf),1,fp2);
			continue;
		}

		str = strtok_r(buf,",\n",&stop);
		if(str){
			my_strncpy(Name,str,sizeof(Name));
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str){
			if(IsIP(str)){
				my_strncpy(Ip,str,sizeof(Ip));
			}
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str){
			my_strncpy(Kind,str,sizeof(Kind));
		}

		str = strtok_r(NULL,",\n",&stop);
		if(str){
			my_strncpy(KeyFileName,str,sizeof(KeyFileName));
		}

		if((strcmp(Ip,hot_ip) == 0) && (Ip[0] != 0)){
			sprintf(buf3,"%s,%s,%s,%s\n",Name,hot_ip,"HOT",KeyFileName);
			fwrite(buf3,strlen(buf3),1,fp2);
		}else{
			if((strcmp(Kind,"HOT") == 0) && Ip[0] != 0){
				sprintf(buf3,"%s,%s,%s,%s\n",Name,Ip,"ALLOW",KeyFileName);
				fwrite(buf3,strlen(buf3),1,fp2);
			}else{
				fwrite(buf2,strlen(buf2),1,fp2);
			}
		}
	}

	fclose(fp1);
	fclose(fp2);

	sprintf(buf3,"mv %s %s.bak",addr_book_file,addr_book_file);
	my_system(buf3);
	sprintf(buf3,"mv %s.tmp %s",addr_book_file,addr_book_file);
	my_system(buf3);
	sprintf(buf3,"chmod 0600 %s",addr_book_file);
	my_system(buf3);
	my_system("sync");

	list = key_list_top;
	while(list){
		if(list->Kind == KIND_HOT){
			list->Kind = KIND_ALLOW;
			break;
		}
		list = list->next;
	}

	list = key_list_top;
	while(list){
		if((strcmp(list->Ip,hot_ip) == 0) && (strcmp(list->Name,"specify ip") != 0)){
			list->Kind = KIND_HOT;
			break;
		}
		list = list->next;
	}

	return 0;
}

static	void	InvalidSt(int S,int Ev)
{

	fprintf(stderr,"Communication error : Invalid Event(%d,%02x)(file=%s line=%d)\n",S,Ev,__FILE__,__LINE__);

	sprintf(DialogMessage,"%s(%d,%02x)",gettext("Communication error"),S,Ev);
	g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);

	St = STATUS_IDLE;
	SessionIp[0] = 0;
	TimerValue = 0;
	init_auth_code(MessAuthCode);
	clear_gmp();
	printf_st("*** END_SESSION ***\n");
}
static	void	do_session(unsigned char *data,char *ip,int recvsize)
{
	unsigned char	buf[1600];
	char		fname[256];
	int		size;
	int		response;


	if(Ignoreflg){
		return;
	}
	if(IsReject(ip)){
		return;
	}

	if(SessionIp[0]){
		if((strcmp(SessionIp,ip) != 0) && (strcmp(my_ip,ip) != 0)){
			return;
		}
	}

	Ev = data[1];

	if((Ev == CMD_START_SESSION) || (Ev == CMD_TIMEOUT) || (Ev == CMD_ACCEPT)){
		if(strcmp(ip,my_ip)){
			return;
		}
	}

	disp_event(Ev);

	switch(St){
		case STATUS_IDLE:
			switch(Ev){
//
// 依頼元はここからスタート
//
				case  CMD_START_SESSION:
					printf_st("*** START_SESSION ***\n");
					printf_st("send SEND_REQ\n");
					
////					my_strncpy(SessionIp,ip,sizeof(SessionIp));

					if(SessionIp[0]){
						buf[0] = PRO_SEND_SESSION;
						buf[1] = CMD_SEND_REQ;
						send_sess(1000,buf,2,SessionIp,PORT_NUM);
						St = STATUS_WAIT_RESP_REQ;
					}
					TimerValue = SESS_TIME_OUT;
					break;
//
// 依頼先はここからスタート
//
				case  CMD_SEND_REQ:

					printf_st("*** START_SESSION ***\n");

					my_strncpy(SessionIp,ip,sizeof(SessionIp));

					g_signal_emit_by_name(G_OBJECT(window ), "AcceptDialog",NULL);	// AcceptDialog
					
					St = STATUS_WAIT_ACCEPT;

					TimerValue = SESS_TIME_OUT;
					break;
				default:
// ここは無視する			InvalidSt(St,Ev);
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);
					break;
			}
			break;
//
// 依頼元
//
		case STATUS_WAIT_RESP_REQ:
			switch(Ev){
				case  CMD_RESP_REQ:
					if(data[2] == 0){
						printf_st("send SEND_A\n");
					 
						if(init_gmp() != 0){
							St = STATUS_IDLE;
							SessionIp[0] = 0;
							TimerValue = 0;
							init_auth_code(MessAuthCode);
							return;
						}

						memset(buf,0x00,sizeof(buf));
						buf[0] = PRO_SEND_SESSION;
						buf[1] = CMD_SEND_A;
						mpz_get_str((char *)(buf + 2),10,A);

						printf_st("%s\n",(char *)(buf + 2));

						send_sess(1000,buf,strlen((char *)buf)+1,ip,PORT_NUM);
					
						St = STATUS_WAIT_RESP_A;
						TimerValue = SESS_TIME_OUT;
					}else{
						St = STATUS_IDLE;
						SessionIp[0] = 0;
						TimerValue = 0;
						init_auth_code(MessAuthCode);

						my_strncpy(DialogMessage,gettext("Connection rejected."),sizeof(DialogMessage));
						g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					}

					break;
				case  CMD_TIMEOUT:		// 相手から返事なし
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					my_strncpy(DialogMessage,gettext("No response"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					break;
				case  CMD_AUTH_NG:

					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_SEND_A;
					send_sess(1000,buf,2,ip,PORT_NUM);

					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					my_strncpy(DialogMessage,gettext("Authentication failed.\n"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					printf_st("*** END_SESSION ***\n");
					break;
				case  CMD_SEND_REQ:		// 自分で自分に送った場合はここ
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;

		case STATUS_WAIT_RESP_A:
			switch(Ev){
				case  CMD_RESP_A:
					St = STATUS_WAIT_B;
					TimerValue = SESS_TIME_OUT;
					break;
				case  CMD_SEND_B:
						St = STATUS_IDLE;
						SessionIp[0] = 0;
						TimerValue = 0;
						init_auth_code(MessAuthCode);

						my_strncpy(DialogMessage,gettext("Invaid protocol"),sizeof(DialogMessage));
						g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					break;
				case  CMD_TIMEOUT:		// 相手から返事なし
					InvalidSt(St,Ev);
					break;
				case  CMD_AUTH_NG:
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					my_strncpy(DialogMessage,gettext("Authentication failed.\n"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					printf_st("END_SESSION\n");
					break;
				default:
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);
					break;
			}
			break;

		case STATUS_WAIT_B:
			switch(Ev){
				case  CMD_SEND_B:
					printf_st("%s\n",(char *)(data + 2));

					mpz_init_set_str(B,(char *)(data + 2),10);



					printf_st("send RESP B\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_B;
					send_sess(1000,buf,2,ip,PORT_NUM);
					

					printf_st("send SEND_X\n");
					memset(buf,0x00,sizeof(buf));
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_SEND_X;
					mpz_get_str((char *)(buf + 2),10,X);
					send_sess(1000,buf,strlen((char *)buf)+1,ip,PORT_NUM);

					printf_st("%s\n",(char *)(buf + 2));

					St = STATUS_WAIT_RESP_X;
					TimerValue = SESS_TIME_OUT;
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;

		case STATUS_WAIT_RESP_X:
			switch(Ev){
				case  CMD_RESP_X:
					St = STATUS_WAIT_Y;
					TimerValue = SESS_TIME_OUT;
					break;
				case  CMD_SEND_Y:
						St = STATUS_IDLE;
						SessionIp[0] = 0;
						TimerValue = 0;

						my_strncpy(DialogMessage,gettext("Invaid protocol"),sizeof(DialogMessage));
						g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;

		case STATUS_WAIT_Y:
			switch(Ev){
				case  CMD_SEND_Y:
					printf_st("%s\n",(char *)(data + 2));

					
					mpz_init_set_str(Y,(char *)(data + 2),10);

					printf_st("send RESP_Y\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_Y;
					send_sess(1000,buf,2,ip,PORT_NUM);

					cal_key();

					sprintf(fname,"/tmp/ihaphone_%s.key",ip);
					gen_key_file(fname);


//					printf_st("KEYDATA=");
//					mpz_out_str (stdout, BASE, W3);
//					printf_st("\n");


					mpz_get_str(sA,BASE,W3);
					combert_key_file1(fname,sA);


					printf_st("send KEY\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_SEND_KEY;
					buf[2] = 0;

					if(KeyFileSize > 1200){
						size = 1200;
						SendDataLen += 1200;
						KeyFileSize -= 1200;

					}else{
						size = KeyFileSize;
						SendDataLen += KeyFileSize;
						KeyFileSize = 0;
					}

					memcpy(buf+3,FileBuffer2,size);

					send_sess(1000,buf,size+3,ip,PORT_NUM);
					
					St = STATUS_WAIT_RESP_KEY;
					TimerValue = SESS_TIME_OUT;
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;

		case STATUS_WAIT_RESP_KEY:
			switch(Ev){
				case  CMD_RESP_KEY:

					TimerValue = SESS_TIME_OUT;

					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_SEND_KEY;

					if(data[2] != 0){
						my_strncpy(DialogMessage,gettext("Key share failed. Please retry."),sizeof(DialogMessage));
						g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);

						printf_st("*** END_SESSION ***\n");
						SessionIp[0] = 0;
						St = STATUS_IDLE;
						TimerValue = 0;
						SendDataLen = 0;
						init_auth_code(MessAuthCode);
						clear_gmp();
					}else if(KeyFileSize > 1200){
						printf_st("send KEY\n");
						buf[2] = 0x00;
//if(SendDataLen == 1200){
//	SendDataLen += 1200;
//	KeyFileSize -= 1200;
//}
						size = 1200;
						KeyFileSize -= 1200;

						memcpy(buf+3,FileBuffer2 + SendDataLen ,size);
						send_sess(1000,buf,size + 3,ip,PORT_NUM);

						SendDataLen += 1200;
					}else if(KeyFileSize > 0){
						printf_st("send KEY(last)\n");
						buf[2] = 0x01;

						size = KeyFileSize;
						KeyFileSize = 0;

						memcpy(buf+3,FileBuffer2 + SendDataLen ,size);
						send_sess(1000,buf,size + 3,ip,PORT_NUM);

						SendDataLen += KeyFileSize;
					}else{
						if(data[2] == 0){
							if(update_address_book() == 0){
								my_strncpy(DialogMessage,gettext("Key share succsessful."),sizeof(DialogMessage));
							}else{
								my_strncpy(DialogMessage,gettext("Key share failed. Please retry."),sizeof(DialogMessage));
							}
						}else{
							my_strncpy(DialogMessage,gettext("Key share failed. Please retry."),sizeof(DialogMessage));
						}
						g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);

						printf_st("*** END_SESSION ***\n");
						SessionIp[0] = 0;
						St = STATUS_IDLE;
						TimerValue = 0;
						SendDataLen = 0;
						init_auth_code(MessAuthCode);
						clear_gmp();
					}
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;
//
// 依頼先
//
		case STATUS_WAIT_ACCEPT:
			switch(Ev){
				case  CMD_ACCEPT:

					printf_st("send RESP_REQ\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_REQ;
					buf[2] = 0x00;
					send_sess(1000,buf,3,SessionIp,PORT_NUM);
					TimerValue = SESS_TIME_OUT;
					St = STATUS_WAIT_A;
					break;
				case  CMD_TIMEOUT:		// ユーザの入力なし

					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					SessDialog_delete = 1;
					g_signal_emit_by_name(G_OBJECT(window ), "AcceptDialog",NULL);	// AcceptDialog

//NG					if(Sdialog){
//NGprintf("destroy Sdialog=%p\n",Sdialog);
//NG						gtk_widget_destroy(Sdialog);
//NG					}
//NG					Sdialog = NULL;
					break;
/*REQ攻撃に対応できない！！	case  CMD_AUTH_NG:
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					my_strncpy(DialogMessage,gettext("Authentication failed.\n"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					printf_st("END_SESSION\n");
					break;
*/
				case  CMD_RESP_REQ:
				case  CMD_SEND_REQ:		// REQが多数来た場合２番目以降は無視する
				default:
					break;
			}
			break;
		case STATUS_WAIT_A:
			switch(Ev){
				case  CMD_SEND_A:
					printf_st("%s\n",(char *)(data + 2));

					if(init_gmp() != 0){
						St = STATUS_IDLE;
						SessionIp[0] = 0;
						TimerValue = 0;
						return;
					}

					mpz_init_set_str(A,(char *)(data + 2),10);



					printf_st("send RESP A\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_A;
					send_sess(1000,buf,2,ip,PORT_NUM);

					printf_st("send SEND B\n");
					memset(buf,0x00,sizeof(buf));
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_SEND_B;
					mpz_get_str((char *)(buf + 2),10,B);
					send_sess(1000,buf,strlen((char *)buf)+1,ip,PORT_NUM);

					printf_st("%s\n",(char *)(buf + 2));

					St = STATUS_WAIT_RESP_B;
					break;
				case  CMD_AUTH_NG:

					my_strncpy(DialogMessage,gettext("Authentication failed.\n"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
// 相手にもAUTH NGを通知してやる。
					printf_st("send RESP A\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_A;
					send_sess(1000,buf,2,ip,PORT_NUM);

					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);
					printf_st("*** END_SESSION ***\n");
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;
		case STATUS_WAIT_RESP_B:
			switch(Ev){
				case  CMD_RESP_B:
					St = STATUS_WAIT_X;
					break;
				case  CMD_SEND_X:
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					my_strncpy(DialogMessage,gettext("Invaid protocol"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;
		case STATUS_WAIT_X:
			switch(Ev){
				case  CMD_SEND_X:
					printf_st("%s\n",(char *)(data + 2));

					mpz_init_set_str(X,(char *)(data + 2),10);



					printf_st("send RESP X\n");
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_X;
					send_sess(1000,buf,2,ip,PORT_NUM);

					printf_st("send SEND Y\n");
					memset(buf,0x00,sizeof(buf));
					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_SEND_Y;
					mpz_get_str((char *)(buf+2),10,Y);
					send_sess(1000,buf,strlen((char *)buf)+1,ip,PORT_NUM);
					
					printf_st("%s\n",(char *)&(buf[1]));

					St = STATUS_WAIT_RESP_Y;
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;
		case STATUS_WAIT_RESP_Y:
			switch(Ev){
				case  CMD_RESP_Y:
					St = STATUS_WAIT_KEY;
	
					cal_key();

					RecvDataLen = 0;

//					printf_st("KEYDATA=");
//					mpz_out_str (stdout, BASE, W4);
//					printf_st("\n");

					break;
				case  CMD_SEND_KEY:
					St = STATUS_IDLE;
					SessionIp[0] = 0;
					TimerValue = 0;
					init_auth_code(MessAuthCode);

					my_strncpy(DialogMessage,gettext("Invaid protocol"),sizeof(DialogMessage));
					g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;
		case STATUS_WAIT_KEY:
			switch(Ev){
				case  CMD_SEND_KEY:

					printf_st("send RESP KEY\n");

					if((RecvDataLen >= FILE_BUF_SIZE) || (recvsize <= 34)){
						my_strncpy(DialogMessage,gettext("Key share failed. Please retry."),sizeof(DialogMessage));
						g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);

						printf_st("*** END_SESSION ***\n");
						St = STATUS_IDLE;
						SessionIp[0] = 0;
						TimerValue = 0;
						RecvDataLen = 0;
						init_auth_code(MessAuthCode);
						response = -1;
					}else{
						memcpy(FileBuffer1 + RecvDataLen,data+3,recvsize - 35);
						RecvDataLen += (recvsize - 35);

						if(data[2] == 0x01){		// 最終データ
							mpz_get_str(sA,BASE,W4);
							response = combert_key_file2(sA);

							if(response == 0){
								if((response = update_address_book()) == 0){
									my_strncpy(DialogMessage,gettext("Key share succsessful."),sizeof(DialogMessage));
								}else{
									my_strncpy(DialogMessage,gettext("Key share failed. Please retry."),sizeof(DialogMessage));
								}
							}else{
								my_strncpy(DialogMessage,gettext("Key share failed. Please retry."),sizeof(DialogMessage));
							}
							g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);

						}else{
							response = 0;
						}
					}

					buf[0] = PRO_SEND_SESSION;
					buf[1] = CMD_RESP_KEY;
					buf[2] = response;
					send_sess(1000,buf,3,ip,PORT_NUM);

					if(data[2] == 0x01){		// 最終データ
						printf_st("*** END_SESSION ***\n");
						St = STATUS_IDLE;
						SessionIp[0] = 0;
						TimerValue = 0;
						RecvDataLen = 0;
						init_auth_code(MessAuthCode);	// send_sessの後でやること！！
						clear_gmp();
					}


					break;
				default:
					InvalidSt(St,Ev);
					break;
			}
			break;
		default:
			break;
	}
	return;
}
/*****************************************************************************************/
/* 受信スレッド処理*/
/*****************************************************************************************/
static	unsigned char		*readbuf,*readbuf2;
static	void	*recv_thread(void *Param)
{
	struct sockaddr_in6 	client;
	struct addrinfo         hints,*res2;
	int                     err;
	char                    portstr[16],addrbuffer[256];
	socklen_t 		addrlen;
	ssize_t 		readsize;
	int 			sock=0,port1=0,rv,datalen,on=1;
	int			recvbufsize;
	char			*ip;
	KEYLIST			*list;
	unsigned char           hash1[32],hash2[32];
	int			option_len=sizeof(int);

/*
	cpu_set_t       cpu_set;
	int             result;

	if(GetCpuThreadNo() >= 2){
		CPU_ZERO(&cpu_set);
		CPU_SET(0x00000000,&cpu_set);
		result = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
		if(result != 0){
			fprintf(stderr,"sched_setaffinity error rv = %d.(file=%s line=%d)\n",result,__FILE__,__LINE__);
		}
	}



	pthread_t		th;
	pthread_attr_t		attr;
	struct	sched_param	sparam;
	int			policy=0,priority=0;

	if(GetCpuThreadNo() >= 2){

	th = pthread_self();

	pthread_attr_init(&attr);
//	pthread_attr_getschedpolicy(&attr,&policy);
//printf("policy=%d\n",policy);

	policy = SCHED_FIFO;
	if(pthread_attr_setschedpolicy(&attr,policy) !=0){
		fprintf(stderr,"pthread_attr_setschedpolicy error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}
//printf("policy=%d\n",policy);
//	priority = sched_get_priority_max(policy);

	priority = 50;
//printf("priority=%d\n",priority);
	sparam.sched_priority = priority;
	if(pthread_setschedparam(th,policy,&sparam) != 0){
		fprintf(stderr,"pthread_setschedparam error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}

	if(pthread_setschedprio(th,priority) != 0){
		fprintf(stderr,"pthread_setschedprio error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}
	}
*/


	port1 = PORT_NUM;

	memset(&hints,0x00,sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	sprintf(portstr,"%d",port1);
	err = getaddrinfo(NULL,portstr,&hints,&res2);
	if(err != 0){
		fprintf(stderr,"getaddrinfo err 3: %s.(file=%s line=%d)\n",gai_strerror(err),__FILE__,__LINE__);
		return 0;
	}
	sock = socket(res2->ai_family, res2->ai_socktype, 0);
	if(sock < 0){
		fprintf(stderr,"sock error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return 0;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

	recvbufsize = 212992;	//425984;	//212992;
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&recvbufsize, sizeof(recvbufsize));

	recvbufsize = 0;
	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&recvbufsize,(unsigned int *)&option_len);
//	printf("socket buffer size=%d\n",recvbufsize);

	if(bind(sock, res2->ai_addr,res2->ai_addrlen) != 0){
		fprintf(stderr,"bind error no=%d port=%d.(file=%s line=%d)\n",errno,port1,__FILE__,__LINE__);
		return 0;
	}

	freeaddrinfo(res2);

	clock_gettime(CLOCK_REALTIME,&befor_disconnector);

	while(1){
// REQ攻撃に耐えられない！！		if(cur_sess_no == 0){
//			usleep(1000*100);
//		}

		if(exitflg){
			close(sock);
			return 0;
		}

		addrlen = sizeof(client);
		readsize = recvfrom(sock, readbuf,2048,MSG_DONTWAIT,
				(struct sockaddr *)&client,&addrlen);
		if(readsize == -1){
			if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
				disconnector();
				usleep(1);
				continue;
			}else{
				fprintf(stderr,"recvfrom errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
				continue;
			}
		}else if((readsize == 0) || (readsize > 1500)){
			usleep(1);
			continue;
		}

		inet_ntop(AF_INET6,&client.sin6_addr,addrbuffer,sizeof(addrbuffer));

		if(memcmp(addrbuffer,"::ffff:",7) == 0){
			ip = addrbuffer + 7;
		}else{
			ip = addrbuffer;
		}
	
		if(IsReject(ip)){
			continue;
		}

		if(readbuf[0] == PRO_SEND_SESSION){
			if(Ignoreflg){
				continue;
			}
//
// メッセージ認証コードの検証
//
//			if((readbuf[1] != CMD_SEND_REQ) && (readbuf[1] != CMD_RESP_REQ)){
			if(readbuf[1] != CMD_SEND_REQ){
				if(readsize <= 32){
					continue;
				}
				memcpy(hash1,readbuf + readsize - 32,32);
				memcpy(readbuf2 , readbuf , readsize -32);
				memcpy(readbuf2 + readsize -32,MessAuthCode,32);

				sha_cal(256,readbuf2,readsize,hash2);

				if(memcmp(hash1,hash2,32) != 0){
//					printf(gettext("Authentication failed.\n"));
					readbuf[0] = PRO_SEND_SESSION;
					readbuf[1] = CMD_AUTH_NG;
					msgsendque(readbuf,ip,2);
					continue;
				}
			}
			msgsendque(readbuf,ip,readsize);
			continue;
		}
//
// シングルモードでは最初のIPのみ通す
//
		if((cur_sess_no >= 1) && (mode == MODE_SINGLE)){
			if(strcmp(ip,recv_list_top->Ip) != 0){
				continue;
			}
		}

		list = list_from_ip(ip);

		if((readbuf[0] == PRO_SEND_IMG_NO_CRYPTO) ||
		   (readbuf[0] == PRO_SEND_VOICE_NO_CRYPTO) ||
		   (readbuf[0] == PRO_SEND_IMG_SUB_NO_CRYPTO)){
//
// 暗号化なし
//
			if(allow_no_crypto == 0){
				continue;
			}
//			if(mode != MODE_SINGLE){
//				continue;
//			}
//
// key listにない暗号なしは１セッションしか通信できない！！
// 最初につながったものだけ！！specify ipのエントリを使う。
//
			if(list == NULL){
				list = list_from_name("specify ip");
				if(list == NULL){
					continue;
				}
				if(strcmp(list->Ip,"*") != 0){
					if(strcmp(list->Ip,ip) != 0){
						continue;
					}
				}else{
					strcpy(list->Ip,ip);
				}
			}
			memcpy(readbuf2,readbuf,readsize);;
		}else{
			if(list == NULL){
				continue;
			}
//
// 前回の暗号なし通信のデータが来た
//
			if((strcmp(list->Name,"specify ip") == 0) || 
			   (list->KeyFileName[0] == 0) || 
			   ((readbuf[0] == PRO_SEND_IMG_NO_CRYPTO) ||
		    	    (readbuf[0] == PRO_SEND_VOICE_NO_CRYPTO) ||
		    	    (readbuf[0] == PRO_SEND_IMG_SUB_NO_CRYPTO))){
				continue;
			}
//
// 最初の１バイトは暗号化されていない！！
//
			pthread_mutex_lock(&(list->m_ctx_enc));
			if(list->ctx == NULL){
				if(alloc_cipher(list) != 0){
					pthread_mutex_unlock(&(list->m_ctx_enc));
					fprintf(stderr,"alloc_cipher error.(file=%s line=%d)\n",__FILE__,__LINE__);
					continue;
				}
			}
			rv = decrypt_data_r(list,readsize - 1,&readbuf[1],&datalen,&readbuf2[1]);
			readbuf2[0] = readbuf[0];
			pthread_mutex_unlock(&(list->m_ctx_enc));

			if(rv != 0){
//				fprintf(stderr,"decrypt err rv=%d size=%ld len=%d Name=%s.(file=%s line=%d)\n",
//					rv,readsize,datalen,name_from_ip(ip),__FILE__,__LINE__);
				continue;
			}
		}

		if((list->DenyCnt == 0) && (list->Kind != KIND_DENY)){
			do_recv(readbuf2,ip);
		}
	}
	close(sock);
	return 0;
}
/*****************************************************************************************/
/* 音声ファイル再生処理*/
/*****************************************************************************************/
static	void	play_wave(char *wavefile)
{
	char			cmd[256];

	sprintf(cmd,"aplay %s > /dev/null 2>&1 ",wavefile);
	my_system(cmd);
	return ;
}
/*****************************************************************************************/
/* タイマースレッド*/
/*****************************************************************************************/
//int	cnt=0;
static	void	disconnector(void)
{
	RECVLIST		*list,*list2;
	int			i;
	char			Ip[64];
	int			bg,disp_no;
	struct	timespec	ts;
	long			delta;

	clock_gettime(CLOCK_REALTIME,&ts);
	
	if(befor_disconnector.tv_sec == ts.tv_sec){
		delta = ts.tv_nsec - befor_disconnector.tv_nsec;
	}else{
		delta = ((1000000000 - befor_disconnector.tv_nsec) + ts.tv_nsec);
	}
	if(delta >= 100000000){
		;
	}else{
		return;
	}

	befor_disconnector = ts;

//printf("disconnector cnt=%d\n",cnt++);


	pthread_mutex_lock(&m_recv_list);

	list2 = recv_list_top;
	while(list2){

		if((ts.tv_sec - list2->last_recv_time.tv_sec) <= 3){
			list2 = list2->next;
			continue;
		}

		my_strncpy(Ip,list2->Ip,sizeof(Ip));
		bg = list2->background;
		disp_no = list2->disp_no;

		del_send_list(Ip);		// list2->IpはNG
		del_recv_list(Ip);

		if(cur_sess_no == 0){
			ent_table1[0].used = 0;
			PinPmode = 0;
		}else if(cur_sess_no == 1){
			if(PinPmode == 0){
				;
			}else if(PinPmode == 1){
				cairo_scale(gcr, (double)1.1, (double)1.0);
			}
			list = recv_list_top;
			if(list){
				list->startx = ent_table1[0].startx;
				list->starty = ent_table1[0].starty;
				list->disp_no = 0;
				list->background = 0;
				ent_table1[0].used = 1;
			}
			PinPmode = 0;
			for(i=0;i<2;i++){
				ent_table2[i].used = 0;
			}
		}else if(cur_sess_no == 2){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)2.0, (double)2.0);
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
					}
					PinPmode = 0;
				}
			}
			list = recv_list_top;
			for(i=0 ;  (i<2) && (list != NULL) ; i++){
				list->startx = ent_table2[i].startx;
				list->starty = ent_table2[i].starty;
				list->disp_no = i;
				ent_table2[i].used = 1;
				list = list->next;
			}

			for(i=0;i<3;i++){
				ent_table3[i].used = 0;
			}
		}else if(cur_sess_no == 3){
			if(PinPmode == 0){
				;
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)2.0, (double)1.0/(double)2.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)2.0, (double)1.0/(double)2.0);
					}
					PinPmode = 0;
				}
			}
			list = recv_list_top;
			for(i=0 ;  (i<3) && (list != NULL) ; i++){
				list->startx = ent_table3[i].startx;
				list->starty = ent_table3[i].starty;
				list->disp_no = i;
				ent_table3[i].used = 1;
				list = list->next;
			}

			for(i=0;i<4;i++){
				ent_table4[i].used = 0;
			}
		}else if(cur_sess_no == 4){
			if(PinPmode == 0){
				cairo_scale(gcr, (double)1.5, (double)1.5);
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)2.0, (double)1.0/(double)2.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)2.0, (double)1.0/(double)2.0);
					}
					PinPmode = 0;
				}
			}
			list = recv_list_top;
			for(i=0 ;  (i<4) && (list != NULL) ; i++){
				list->startx = ent_table4[i].startx;
				list->starty = ent_table4[i].starty;
				list->disp_no = i;
				ent_table4[i].used = 1;
				list = list->next;
			}

			for(i=0 ; i<9 ; i++){
				ent_table9[i].used = 0;
			}
		}else if((5 <= cur_sess_no) && (cur_sess_no <= 9)){
			if(PinPmode == 0){
				if(cur_sess_no == 9){
					cairo_scale(gcr, (double)4.0/(double)3.0, (double)4.0/(double)3.0);
					list = recv_list_top;
					for(i=0 ;  (i<9) && (list != NULL) ; i++){
						list->startx = ent_table9[i].startx;
						list->starty = ent_table9[i].starty;
						list->disp_no = i;
						ent_table9[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no <9)){
						ent_table9[disp_no].used = 0;
					}
				}
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)3.0, (double)1.0/(double)3.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)3.0, (double)1.0/(double)3.0);
					}
					PinPmode = 0;
				}
				if(cur_sess_no == 9){
					list = recv_list_top;
					for(i=0 ;  (i<9) && (list != NULL) ; i++){
						list->startx = ent_table9[i].startx;
						list->starty = ent_table9[i].starty;
						list->disp_no = i;
						ent_table9[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no <9)){
						ent_table9[disp_no].used = 0;
					}
				}
			}
			for(i=0;i<16;i++){
				ent_table16[i].used = 0;
			}
		}else if((10 <= cur_sess_no) && (cur_sess_no <= 16)){
			if(PinPmode == 0){
				if(cur_sess_no == 16){
					cairo_scale(gcr, (double)1.25, (double)1.25);
					list = recv_list_top;
					for(i=0 ;  (i<16) && (list != NULL) ; i++){
						list->startx = ent_table16[i].startx;
						list->starty = ent_table16[i].starty;
						list->disp_no = i;
						ent_table16[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no <16)){
						ent_table16[disp_no].used = 0;
					}
				}
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)4.0, (double)1.0/(double)4.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)4.0, (double)1.0/(double)4.0);
					}
					PinPmode = 0;
				}
				if(cur_sess_no == 16){
					list = recv_list_top;
					for(i=0 ;  (i<16) && (list != NULL) ; i++){
						list->startx = ent_table16[i].startx;
						list->starty = ent_table16[i].starty;
						list->disp_no = i;
						ent_table16[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no <16)){
						ent_table16[disp_no].used = 0;
					}
				}
			}
			for(i=0;i<25;i++){
				ent_table25[i].used = 0;
			}
		}else if((17 <= cur_sess_no) && (cur_sess_no <= 25)){
			if(PinPmode == 0){
				if(cur_sess_no == 25){
					cairo_scale(gcr, (double)1.2, (double)1.2);
					list = recv_list_top;
					for(i=0 ;  (i<25) && (list != NULL) ; i++){
						list->startx = ent_table25[i].startx;
						list->starty = ent_table25[i].starty;
						list->disp_no = i;
						ent_table25[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no <25)){
						ent_table25[disp_no].used = 0;
					}
				}
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)5.0, (double)1.0/(double)5.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)5.0, (double)1.0/(double)5.0);
					}
					PinPmode = 0;
				}
				if(cur_sess_no == 25){
					list = recv_list_top;
					for(i=0 ;  (i<25) && (list != NULL) ; i++){
						list->startx = ent_table25[i].startx;
						list->starty = ent_table25[i].starty;
						list->disp_no = i;
						ent_table25[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no <25)){
						ent_table25[disp_no].used = 0;
					}
				}
			}
			for(i=0;i<36;i++){
				ent_table36[i].used = 0;
			}
		}else if((26 <= cur_sess_no) && (cur_sess_no <= 36)){
			if(PinPmode == 0){
				if(cur_sess_no == 36){
					cairo_scale(gcr, (double)7.0/(double)6.0, (double)7.0/(double)6.0);
					list = recv_list_top;
					for(i=0 ;  (i<36) && (list != NULL) ; i++){
						list->startx = ent_table36[i].startx;
						list->starty = ent_table36[i].starty;
						list->disp_no = i;
						ent_table36[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no < 36)){
						ent_table36[disp_no].used = 0;
					}
				}
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)6.0, (double)1.0/(double)6.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)6.0, (double)1.0/(double)6.0);
					}
					PinPmode = 0;
				}
				if(cur_sess_no == 36){
					list = recv_list_top;
					for(i=0 ;  (i<36) && (list != NULL) ; i++){
						list->startx = ent_table36[i].startx;
						list->starty = ent_table36[i].starty;
						list->disp_no = i;
						ent_table36[i].used = 1;
						list = list->next;
					}
				}else{
					if((0 <= disp_no) && (disp_no < 36)){
						ent_table36[disp_no].used = 0;
					}
				}
			}
			for(i=0;i<49;i++){
				ent_table49[i].used = 0;
			}
		}else if((37 <= cur_sess_no) && (cur_sess_no <= 49)){
			if(PinPmode == 0){
				if((0 <= disp_no) && (disp_no < 49)){
					ent_table49[disp_no].used = 0;
				}
			}else{
				if(bg == 1){
					if(PinPmode == 1){
						cairo_scale(gcr, (double)1.1, (double)1.0);
						cairo_scale(gcr, (double)1.0/(double)7.0, (double)1.0/(double)7.0);
					}else{
						cairo_scale(gcr, (double)1.0/(double)7.0, (double)1.0/(double)7.0);
					}
					PinPmode = 0;
				}
				if((0 <= disp_no) && (disp_no < 49)){
					ent_table49[disp_no].used = 0;
				}
			}
		}

		chgflg = 1;
		break;			// ここで抜けないとlistがくるってしまう！！
	}

	pthread_mutex_unlock(&m_recv_list);
//printf("disconnector exit\n");
	return;
}
/*****************************************************************************************/
/* FPS制御スレッド*/
/*****************************************************************************************/
static	void	*fps_thread(void *Param)
{
	char			buf[256];
	struct	timespec	ts;
	RECVLIST		*list;

	while(1){
		if(exitflg){
			return 0;
		}
		usleep(1000*1000);

		sprintf(buf,"IHAPHONE(RIP:%03ld SIP:%03ld RVP:%03ld SVP:%03ld DRAWS:%04ld VWRITE:%ld VREAD:%ld)",
				(rfps - b_rfps),
				(sfps - b_sfps),
				(rvfps - b_rvfps),
				(svfps - b_svfps),
				(draws - b_draws),
				(VoiceWriteTotall/VoiceWriteCnt),
				(VoiceReadTotall/VoiceReadCnt)
				);

		SendFps = sfps - b_sfps;
		gtk_window_set_title(GTK_WINDOW(window), buf);

		b_rfps = rfps;
		b_sfps = sfps;
		b_rvfps = rvfps;
		b_svfps = svfps;
		b_draws = draws;

		if(VoiceReadCnt >= 7*9*60){
			VoiceWriteTotall = 0;
			VoiceWriteCnt = 1;
			VoiceReadTotall = 0;
			VoiceReadCnt = 1;
		}

		clock_gettime(CLOCK_REALTIME,&ts);

		list = recv_list_top;
		while(list){
			if((ts.tv_sec - list->last_recv_time2.tv_sec) >= 1){
				list->SurfWritedCounter = 0;
			}
			if(list->TotalSurfWritedCounter == 0){
				if(list->ImageRecv != 100){
					chgflg = 1;
				}
				list->ImageRecv = 100;
				list->SurfWritedCounter = 0;
			}
			list = list->next;
		}
	}
}
/*****************************************************************************************/
/* JPEGからカイロのサーフェスへの変換処理*/
/*****************************************************************************************/
static unsigned int  translate_rgb(const unsigned char* src)
{
        int r = *(src++);
        int g = *(src++);
        int b = *(src++);

        return 0xFF000000 | (r << 16) | (g << 8) | b;
}
static void copy0_scanline(unsigned char* dst, const unsigned char* src, int w)
{
        for (; w > 0; w--, dst +=3 ,src += 3) {
		memcpy(dst,src,3);
        }
	return;
}
static void copy1_scanline(unsigned char* dst, const unsigned char* src, int w)
{
        unsigned int * current = (unsigned int *) dst;

        for (; w > 0; w--, src += 3) {
                *(current++) = translate_rgb(src);
        }
	return;
}
//
// 横方向の両端を1/4ずつ削り、横方向に２倍に伸長する。
//
static void copy10_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (; w > 0; w--, src += 3) {
		if((w2/4 <= w) && (w <= w2*3/4)){
			*(current++) = translate_rgb(src);
			*(current++) = translate_rgb(src);
		}
        }
	return;
}
static void copy2_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i;
        unsigned int * current = (unsigned int *) dst;

        for (i=0; w > 0; w--, src += 3 ,i++) {
                *(current++) = translate_rgb(src);
		if(i%2 == 0){
                	*(current++) = translate_rgb(src);
		}
        }
	return;
}
static void copy20_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i,w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (i=0; w > 0; w--, src += 3 ,i++) {
		if((w2/4 <= i) && (i <= w2*3/4)){
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
		}
		if(i%2 == 0){
			if((w2/4 <= i) && (i <= w2*3/4)){
                		*(current++) = translate_rgb(src);
                		*(current++) = translate_rgb(src);
			}
		}
        }
	return;
}
static void copy4_scanline(unsigned char* dst, const unsigned char* src, int w)
{
        unsigned int * current = (unsigned int *) dst;

        for (; w > 0; w--, src += 3 ) {
                *(current++) = translate_rgb(src);
               	*(current++) = translate_rgb(src);
               	*(current++) = translate_rgb(src);
        }
	return;
}
static void copy40_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i,w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (i=0; w > 0; w--, src += 3 ,i++) {
		if((w2/4 <= i) && (i <= w2*3/4)){
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
		}
        }
	return;
}
static void copy5_scanline(unsigned char* dst, const unsigned char* src, int w)
{
        unsigned int * current = (unsigned int *) dst;

        for (; w > 0; w--, src += 3 ) {
                *(current++) = translate_rgb(src);
               	*(current++) = translate_rgb(src);
        }
	return;
}
static void copy50_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i,w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (i=0; w > 0; w--, src += 3 ,i++) {
		if((w2/4 <= i) && (i <= w2*3/4)){
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
                	*(current++) = translate_rgb(src);
		}
        }
	return;
}
static void copy6_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i;
        unsigned int * current = (unsigned int *) dst;

        for (i=0; w > 0; w--, src += 3 ,i++) {
		if(i%2 == 0){
                	*(current++) = translate_rgb(src);
		}
        }
	return;
}
static void copy60_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i,w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (i=0; w > 0; w--, src += 3 ,i++) {
		if(i%2 == 0){
			if((w2/4 <= i) && (i <= w2*3/4)){
                		*(current++) = translate_rgb(src);
                		*(current++) = translate_rgb(src);
			}
		}
        }
	return;
}
static void copy7_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i;
        unsigned int * current = (unsigned int *) dst;

        for (i=0; w > 0; w--, src += 3 ,i++) {
		if(i%3 == 0){
                	*(current++) = translate_rgb(src);
		}
        }
	return;
}
static void copy70_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i,w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (i=0; w > 0; w--, src += 3 ,i++) {
		if(i%3 == 0){
			if((w2/4 <= i) && (i <= w2*3/4)){
                		*(current++) = translate_rgb(src);
                		*(current++) = translate_rgb(src);
			}
		}
        }
	return;
}
static void copy8_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i;
        unsigned int * current = (unsigned int *) dst;

        for (i=0; w > 0; w--, src += 3 ,i++) {
		if(i%3 == 2){
			continue;
		}
                *(current++) = translate_rgb(src);
        }
	return;
}
static void copy80_scanline(unsigned char* dst, const unsigned char* src, int w)
{
	int		i,w2;
        unsigned int * current = (unsigned int *) dst;

	w2 = w;
        for (i=0; w > 0; w--, src += 3 ,i++) {
		if(i%3 == 2){
			continue;
		}
		if((w2/4 <= i) && (i <= w2*3/4)){
               		*(current++) = translate_rgb(src);
               		*(current++) = translate_rgb(src);
		}
        }
	return;
}
static	jmp_buf		jmpbuf,jmpbuf2;
static void error_exit(j_common_ptr cinfo)
{
	longjmp(jmpbuf, 1);
}
static void error_exit2(j_common_ptr cinfo)
{
	longjmp(jmpbuf2, 1);
}
static void nullfunc(j_common_ptr cinfo)
{
	return;
}
void	dummy_free(void *ptr)
{
	return;
}
static	void	RGBtoYUV420(unsigned char *src,int w,int h,unsigned char *dst)
{
	int	frameSize,chromasize,yIndex,uIndex,vIndex;
	int	R,G,B,Y,U,V,index,i,j;

	frameSize = w*h;
	chromasize = frameSize/4;

	yIndex = 0;
	uIndex = frameSize;
	vIndex = frameSize + chromasize;

	index = 0;

	for(j=0;j<h;j++){
		for(i=0;i<w;i++){

			R = *src;
			G = *(src+1);
			B = *(src+2);

			src += 3;

			Y = ((66 * R + 129 * G +  25 * B + 128) >> 8) +  16;
			U = (( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
			V = (( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128;

			dst[yIndex++] = (unsigned char)((Y < 0) ? 0 : ((Y > 255) ? 255 : Y));

			if((j%2 == 0) && (index%2 == 0)){
				dst[uIndex++] = (unsigned char)((U < 0) ? 0 : ((U > 255) ? 255 : U));
				dst[vIndex++] = (unsigned char)((V < 0) ? 0 : ((V > 255) ? 255 : V));
			}

			index++;
		}
	}

	return;
}
static	int	JPEGtoRGB(void *src,int src_len,unsigned char *dst,int *w,int *h)
{
	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr		jerr;
	unsigned char			*row;
	unsigned char 			*buffers[1];

	memset(&cinfo,0x00,sizeof(cinfo));
	jpeg_create_decompress(&cinfo);

	cinfo.err = jpeg_std_error(&jerr);
	cinfo.err->output_message = nullfunc;
	jerr.error_exit = error_exit2;

	if(setjmp(jmpbuf2)){
		jpeg_destroy_decompress(&cinfo);
		fprintf(stderr,"decompress error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;		// エラー時はここに帰ってくる。
	}

	jpeg_mem_src(&cinfo, src, src_len);

	if(jpeg_read_header(&cinfo, TRUE) == 0){
		fprintf(stderr,"jpeg_read_header error.(file=%s line=%d)\n",__FILE__,__LINE__);
		jpeg_destroy_decompress(&cinfo);
		return -1;
	}


	cinfo.out_color_space = JCS_RGB;

	jpeg_start_decompress(&cinfo);

	if(cinfo.output_width > 3840){
		fprintf(stderr,"large jpeg not supported.(file=%s line=%d)\n",__FILE__,__LINE__);
		jpeg_destroy_decompress(&cinfo);
		return -1;
	}
	*w = cinfo.output_width;
	*h = cinfo.output_height;


	buffers[0] = jpeg_scanline2;
	row = dst;

	while (cinfo.output_scanline < cinfo.output_height) {

		jpeg_read_scanlines(&cinfo, buffers, 1);
			
		copy0_scanline(row, jpeg_scanline2, cinfo.output_width);

		row += cinfo.output_width*3;
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return 0;
}
/*****************************************************************************************/
/* JPEGからカイロのサーフェスへの変換処理*/
/* マルチスレッドでの使用は不可！！*/
/*****************************************************************************************/
/*static	cairo_surface_t *cairo_rgb_mem(void *data)
{
	int			stride;
	cairo_surface_t 	*sfc=NULL;

	stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24,width);

	sfc = cairo_image_surface_create_for_data (data,CAIRO_FORMAT_RGB24,width,height,stride);

	return sfc;
}*/

static	cairo_surface_t *make_cairo_surface(RECVLIST *list,void *data, size_t len,int initflg)
{
	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr		jerr;
	cairo_surface_t 		*sfc=NULL;
	int				stride;
	unsigned char			*row;
	unsigned char 			*buffers[1];
	unsigned char			*ep;
	int				i,scan_line_no;
 

	memset(&cinfo,0x00,sizeof(cinfo));
	jpeg_create_decompress(&cinfo);

	cinfo.err = jpeg_std_error(&jerr);
	cinfo.err->output_message = nullfunc;
	jerr.error_exit = error_exit;

	if(setjmp(jmpbuf)){
		jpeg_destroy_decompress(&cinfo);
		fprintf(stderr,"decompress error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return NULL;		// エラー時はここに帰ってくる。
	}

	jpeg_mem_src(&cinfo, data, len);

	if(jpeg_read_header(&cinfo, TRUE) == 0){
		fprintf(stderr,"jpeg_read_header error.(file=%s line=%d)\n",__FILE__,__LINE__);
		jpeg_destroy_decompress(&cinfo);
		return NULL;
	}

	cinfo.out_color_space = JCS_RGB;

	jpeg_start_decompress(&cinfo);

	if(cinfo.output_width > 3840){
		fprintf(stderr,"large jpeg not supported.(file=%s line=%d)\n",__FILE__,__LINE__);
		jpeg_destroy_decompress(&cinfo);
		return NULL;
	}

// create Cairo image surface

	if(list){
		if(list->surf == NULL){
			sfc = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width,height);
		}else{
			sfc = list->surf;
		}
	}else{
		sfc = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width,height);
	}


	if (cairo_surface_status(sfc) != CAIRO_STATUS_SUCCESS){
		fprintf(stderr,"cairo_image_surface_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return NULL;
	}

//printf("width=%d height=%d jpg_width=%d jpg_height=%d\n",width,height,cinfo.output_width,cinfo.output_height);

	if((cinfo.output_width * cinfo.num_components) > SCAN_LINE_SIZE){
		fprintf(stderr,"scanline overflow.(file=%s line=%d)\n",__FILE__,__LINE__);
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return NULL;
	}

	stride = cairo_image_surface_get_stride(sfc);
	row = cairo_image_surface_get_data(sfc);
	buffers[0] = jpeg_scanline;
	ep = edit_buffer2;

	if((width == 3840) && (cinfo.output_width == 1920)){		// 補間する
		if((cur_sess_no == 2) && (PinPmode == 0)){
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);
				copy50_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;

				copy50_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;
			}
			scan_line_no = cinfo.output_scanline;
			ep = edit_buffer2;

			for(i=0;i<scan_line_no;i++){
				memcpy(row, ep, stride);
				row += stride;
				ep += stride;

				memcpy(row, ep, stride);
				row += stride;
				ep += stride;
			}
		}else{
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);
				copy5_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;

				copy5_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;
			}
		}
	}else if((width == 3840) && (cinfo.output_width == 1280)){	// 補間する
		if((cur_sess_no == 2) && (PinPmode == 0)){
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);
				copy40_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;

				copy40_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;

				copy40_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;
			}
			scan_line_no = cinfo.output_scanline;
			ep = edit_buffer2;

			for(i=0;i<scan_line_no;i++){
				memcpy(row, ep, stride);
				row += stride;
				ep += stride;

				memcpy(row, ep, stride);
				row += stride;
				ep += stride;

				memcpy(row, ep, stride);
				row += stride;
				ep += stride;
			}
		}else{
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);
				copy4_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;

				copy4_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;

				copy4_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;
			}
		}
	}else if((width == 1920) && (cinfo.output_width == 1280)){	// 補間する
		if((cur_sess_no == 2) && (PinPmode == 0) && (initflg == 0)){
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				copy20_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;

				if(cinfo.output_scanline %2 == 0){
					copy20_scanline(ep, jpeg_scanline, cinfo.output_width);
					ep += stride;
				}
			}
			scan_line_no = cinfo.output_scanline;
			ep = edit_buffer2;

			for(i=0;i<scan_line_no;i++){
				memcpy(row, ep, stride);
				row += stride;
				ep += stride;

				if(i%2 == 1){
					memcpy(row, ep, stride);
					row += stride;
					ep += stride;
				}
			}
		}else{
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				copy2_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;

				if(cinfo.output_scanline %2 == 0){
					copy2_scanline(row, jpeg_scanline, cinfo.output_width);
					row += stride;
				}
			}
		}
	}else if((width == 1920) && (cinfo.output_width == 3840)){	// 間引く
		if((cur_sess_no == 2) && (PinPmode == 0)){
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				if(cinfo.output_scanline % 2 == 0){
					copy60_scanline(ep, jpeg_scanline, cinfo.output_width);
					ep += stride;
				}
			}
			scan_line_no = cinfo.output_scanline;
			ep = edit_buffer2;

			for(i=0;i<scan_line_no;i++){
				if(i%2 == 1){
					memcpy(row, ep, stride);
					row += stride;
					ep += stride;
				}
			}
		}else{
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				if(cinfo.output_scanline % 2 == 0){
					copy6_scanline(row, jpeg_scanline, cinfo.output_width);
					row += stride;
				}
			}
		}
	}else if((width == 1280) && (cinfo.output_width == 3840)){	// 間引く
		if((cur_sess_no == 2) && (PinPmode == 0)){
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				if(cinfo.output_scanline % 3 == 0){
					copy70_scanline(ep, jpeg_scanline, cinfo.output_width);
					ep += stride;
				}
			}
			scan_line_no = cinfo.output_scanline;
			ep = edit_buffer2;

			for(i=0;i<scan_line_no;i++){
				if(i%3 == 1){
					memcpy(row, ep, stride);
					row += stride;
					ep += stride;
				}
			}
		}else{
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				if(cinfo.output_scanline % 3 == 0){
					copy7_scanline(row, jpeg_scanline, cinfo.output_width);
					row += stride;
				}
			}
		}
	}else if((width == 1280) && (cinfo.output_width == 1920)){	// 間引く
		if((cur_sess_no == 2) && (PinPmode == 0)){
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				if(cinfo.output_scanline % 3 == 2){
					continue;
				}
				copy80_scanline(ep, jpeg_scanline, cinfo.output_width);
				ep += stride;
			}
			scan_line_no = cinfo.output_scanline;
			ep = edit_buffer2;

			for(i=0;i<scan_line_no;i++){
				if(i%3 == 2){
					continue;
				}

				memcpy(row, ep, stride);
				row += stride;
				ep += stride;
			}
		}else{
			while (cinfo.output_scanline < cinfo.output_height) {

				jpeg_read_scanlines(&cinfo, buffers, 1);

				if(cinfo.output_scanline % 3 == 2){
					continue;
				}

				copy8_scanline(row, jpeg_scanline, cinfo.output_width);
				row += stride;
			}
		}
	}else{								// 変更なし
		while (cinfo.output_scanline < cinfo.output_height) {

			jpeg_read_scanlines(&cinfo, buffers, 1);
			
			if((cur_sess_no == 2) && (PinPmode == 0)){
				copy10_scanline(row, jpeg_scanline, cinfo.output_width);
			}else{
				copy1_scanline(row, jpeg_scanline, cinfo.output_width);
			}
			row += stride;
		}
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

////	cairo_surface_mark_dirty(sfc);
/*	if(cairo_surface_set_mime_data(sfc, CAIRO_MIME_TYPE_JPEG, data, len, dummy_free, data) !=
			CAIRO_STATUS_SUCCESS){
		fprintf(stderr,"cairo_surface_set_mime_data error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return NULL;
	}
*/
	return sfc;
}
/*****************************************************************************************/
/* 以下はデバイスからの音声受信スレッド*/
/*****************************************************************************************/
/*static void DataCompress(unsigned char *in,int insize,unsigned char *out,int *outsize)
{
	int		ret;
	z_stream	strm;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = deflateInit(&strm, Z_BEST_COMPRESSION);
	if (ret != Z_OK){
		*outsize = insize;
		memcpy(out,in,insize);
		fprintf(stderr,"deflateInit error!! ret=%d.(file=%s line=%d)\n",ret,__FILE__,__LINE__);
		return;
	}
        strm.next_in = in;
	strm.next_out = out;
	strm.avail_in = insize;
	strm.avail_out = *outsize;

	ret = deflate(&strm, Z_FINISH);
//printf("deflate ret=%d strm.avail_out = %d\n",ret,strm.avail_out);
	if(ret != Z_STREAM_END){
		fprintf(stderr,"deflate error ret=%d strm.avail_out = %d.(file=%s line=%d)\n",ret,strm.avail_out,__FILE__,__LINE__);
	}

	*outsize -= strm.avail_out;

	deflateEnd(&strm);

	return;
}

static int DataDeCompress(unsigned char *in,int insize,unsigned char *out,int *outsize)
{
	int		ret;
	z_stream	strm;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	ret = inflateInit(&strm);
	if (ret != Z_OK){
		*outsize = insize;
		memcpy(out,in,insize);
		fprintf(stderr,"inflateInit error!! ret=%d.(file=%s line=%d)\n",ret,__FILE__,__LINE__);
		return -1;
	}
	
//printf("insize = %d\n",insize);

	strm.next_in = in;
	strm.next_out = out;
	strm.avail_in = insize;
	strm.avail_out = *outsize;

//printf("befor1 strm.avail_out = %d\n",strm.avail_out);
	ret = inflate(&strm, Z_FINISH);
//printf("after1 strm.avail_out = %d  ret = %d\n",strm.avail_out,ret);

//	assert(ret != Z_STREAM_ERROR);

	*outsize -= strm.avail_out;

	inflateEnd(&strm);

	return 0;
}
*/

static	void *sound_thread(void *pParam)
{
	int			f,cb;
	int			i;
	unsigned char		*sound_buffer;		// sound data buffer
	RECVLIST		dummy_list;
	unsigned char		*sound_buffer2;

	cb = chunk_bytes;

	sound_buffer = (unsigned char *)calloc(1,cb*CHUNK_NO);
	if(sound_buffer == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return 0;
	}
	sound_buffer2 = (unsigned char *)calloc(1,cb*CHUNK_NO+100);
	if(sound_buffer2 == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return 0;
	}
	memset(&dummy_list,0x00,sizeof(dummy_list));
	my_strncpy(dummy_list.Ip,my_ip,sizeof(dummy_list.Ip));
	f = cb * 8 / bits_per_frame;

	while (1) {
		if(exitflg){
			snd_pcm_close(handle2);
			free(sound_buffer);
			free(sound_buffer2);
			return 0;
		}
		if(cur_sess_no <= 0){
			usleep(1000*100);
		}

		for(i=0;i<CHUNK_NO;i++){
			if(pcm_read(handle2,&sound_buffer[cb*i], f) != f){	// USBを切っても正常値がかえる！！
								// 切断状態を認識できない！！
				fprintf(stderr,"pcm_read error.(file=%s line=%d)\n",__FILE__,__LINE__);
				snd_pcm_close(handle2);
				handle2 = init_sound(SND_PCM_STREAM_CAPTURE);
				usleep(1000*1000);
				continue;
			}
		}

		dummy_list.recflg = recflg;
		save_wave(&dummy_list,sound_buffer,cb*CHUNK_NO);

//		csize1 = cb*CHUNK_NO+100;
//		DataCompress(sound_buffer,cb*CHUNK_NO,sound_buffer2,&csize1);
//		send_data(1000*20,csize1,sound_buffer2,PRO_SEND_VOICE);

		send_data(1000*20,cb*CHUNK_NO,sound_buffer,PRO_SEND_VOICE);
	}
	return 0;
}
/*****************************************************************************************/
/* 以下はデバイスからの画像受信用スレッド*/
/*****************************************************************************************/
static	void yuv422to420(unsigned char *map, unsigned char *cmap, int width, int height)
{
	unsigned char *src, *dest, *src2, *dest2;
	int i, j;

	src = cmap;
	dest = map;
	for (i = width * height; i > 0; i--) {
		*dest++ = *src;
		src += 2;
	}
	src = cmap + 1;
	src2 = cmap + width * 2 + 1;
	dest = map + width * height;
	dest2 = dest + (width * height) / 4;
	for (i = height / 2; i > 0; i--) {
		for (j = width / 2; j > 0; j--) {
			*dest = ((int) *src + (int) *src2) / 2;
			src += 2;
			src2 += 2;
			dest++;
			*dest2 = ((int) *src + (int) *src2) / 2;
			src += 2;
			src2 += 2;
			dest2++;
		}
		src += width * 2;
		src2 += width * 2;
	}
}

typedef struct {
	struct jpeg_destination_mgr pub;
	JOCTET *buf;
	size_t bufsize;
	size_t jpegsize;
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;

void init_destination(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = dest->bufsize;
	dest->jpegsize = 0;
}

boolean empty_output_buffer(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = dest->bufsize;

	return FALSE;
}

void term_destination(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
	dest->jpegsize = dest->bufsize - dest->pub.free_in_buffer;
}

static void  _jpeg_mem_dest(j_compress_ptr cinfo, JOCTET* buf, size_t bufsize)
{
	mem_dest_ptr dest;

	if (cinfo->dest == NULL) {
		cinfo->dest = (struct jpeg_destination_mgr *)
				(*cinfo->mem->alloc_small)((j_common_ptr)cinfo,
				 JPOOL_PERMANENT,
				sizeof(mem_destination_mgr));
	}

	dest = (mem_dest_ptr) cinfo->dest;

	dest->pub.init_destination    = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination    = term_destination;

	dest->buf      = buf;
	dest->bufsize  = bufsize;
	dest->jpegsize = 0;
}

static int _jpeg_mem_size(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;
	return dest->jpegsize;
}

static int YUV420toJPEG(unsigned char *dest_image, int image_size,
                   unsigned char *input_image, int width, int height, int quality)

{
	int i, j, jpeg_image_size;
	JSAMPROW y[16],cb[16],cr[16];
	JSAMPARRAY data[3];
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	data[0] = y;
	data[1] = cb;
	data[2] = cr;

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	jpeg_set_defaults(&cinfo);
	jpeg_set_colorspace(&cinfo, JCS_YCbCr);
	cinfo.raw_data_in = TRUE;

#if JPEG_LIB_VERSION >= 70
	cinfo.do_fancy_downsampling = FALSE;
#endif
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;

	jpeg_set_quality(&cinfo, quality, TRUE);
	cinfo.dct_method = JDCT_FASTEST;

	_jpeg_mem_dest(&cinfo, dest_image, image_size);

	jpeg_start_compress(&cinfo, TRUE);

	for(j = 0; j < height; j += 16) {
		for(i = 0; i < 16; i++) {
			if((width * (i + j)) < (width * height)) {
				y[i] = input_image + width * (i + j);
				if(i % 2 == 0) {
					cb[i/2] = input_image + width * height + 
						width/2 * ((i + j) /2);
					cr[i/2] = input_image + width * height + 
						width * height/4 + width/2 * ((i + j)/2);
				}
			}else{
				y[i] = 0x00;
				cb[i] = 0x00;
				cr[i] = 0x00;
			}
		}
		jpeg_write_raw_data(&cinfo, data, 16);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_image_size = _jpeg_mem_size(&cinfo);
	jpeg_destroy_compress(&cinfo);

	return jpeg_image_size;
}
/*****************************************************************************************/
/* VIDEO CAPTURE*/
/*****************************************************************************************/
/*static	int	get_jpeg_size(unsigned char *data,int	bufsize)
{
	int		len;
	unsigned char	*p;
	unsigned short	seg_len1,seg_len2;

	len = 0;
	p = data;

	for(;1;){
		if((*p == 0xff) && (*(p+1) == 0xd8)){
			len = 2;
			p += 2;
		}
		if((*p == 0xff) && (*(p+1) == 0xd9)){
			len += 2;
			break;
		}
		p += 1;
		len += 1;

//		memcpy(&seg_len1,p+2,2);
//		if(IsLittleEndian()){
//			seg_len2 = swap16((unsigned short)seg_len1);
//		}else{
//			seg_len2 = seg_len1;
//		}
//		printf("seg_len=%d\n",seg_len2);
//		p += (2 + seg_len2);
//		len += (2 + seg_len2);

		if(bufsize < len){
			break;
		}

	}

	return len;
}*/
static	void *video_thread(void *Param)
{
	int			fd,cnt;
	int			send_size,n;
	int			buffer_index;
	int			buffer_count;
	int    			pal,index,save_index;
	int			rv,w,h;
	unsigned int 		p_array[25];
	char			devbuf[32];
	fd_set 			readfds;
	enum v4l2_buf_type 	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	src_v4l2_st 		vid;
	struct timeval 		tv;
	struct v4l2_capability 	cap;
	struct v4l2_fmtdesc 	fmtd;
	struct v4l2_format 	dst_fmt;
	struct v4l2_buffer 	buf2;
	struct	timespec	ts1,ts2,ts3,ts4;
	long			delta,sleep_time;

top:
	while(1){
		if(camera_device[0]){
			fd = open(camera_device,O_RDWR);
			if(fd == -1){
				fprintf(stderr,"camera device(%s) open error.(file=%s line=%d)\n",camera_device,__FILE__,__LINE__);
			}else{
				printf("%s is opened \n",camera_device);
				cameraflg = 1;
				break;
			}
		}

		for(cnt=0;cnt<100;cnt++){
			sprintf(devbuf,"/dev/video%d",cnt);
			fd = open(devbuf,O_RDWR);
			if(fd == -1){
				cameraflg = 0;
				usleep(1000*10);
				continue;
			}else{
				printf("%s is opened \n",devbuf);
				cameraflg = 1;
				break;
			}
		}
		if(cnt >= 100){
			if(exitflg){
				return 0;
			}
			usleep(1000*10);
		}else{
			break;
		}
	}

	memset(&cap,0x00,sizeof(cap));

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		fprintf(stderr,"VIDIOC_QUERYCAP error device=%s.(file=%s line=%d)\n",devbuf,__FILE__,__LINE__);
		close(fd);
		goto top;
	}
	if(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE){
		printf("capability : V4L2_CAP_VIDEO_CAPTURE \n");
	}
	if(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT){
		printf("capability : V4L2_CAP_VIDEO_OUTPUT \n");
	}
	if(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY){
		printf("capability : V4L2_CAP_VIDEO_OVERLAY \n");
	}
	if(cap.capabilities & V4L2_CAP_VBI_CAPTURE){
		printf("capability : V4L2_CAP_VBI_CAPTURE \n");
	}
	if(cap.capabilities & V4L2_CAP_VBI_OUTPUT){
		printf("capability : V4L2_CAP_VBI_OUTPUT \n");
	}
	if(cap.capabilities & V4L2_CAP_RDS_CAPTURE){
		printf("capability : V4L2_CAP_RDS_CAPTURE \n");
	}
	if(cap.capabilities & V4L2_CAP_TUNER){
		printf("capability : V4L2_CAP_TUNER \n");
	}
	if(cap.capabilities & V4L2_CAP_AUDIO){
		printf("capability : V4L2_CAP_AUDIO \n");
	}
	if(cap.capabilities & V4L2_CAP_READWRITE){
		printf("capability : V4L2_CAP_READWRITE \n");
	}
	if(cap.capabilities & V4L2_CAP_ASYNCIO){
		printf("capability : V4L2_CAP_ASYNCIO \n");
	}
	if(cap.capabilities & V4L2_CAP_STREAMING){
		printf("capability : V4L2_CAP_STREAMING \n");
	}
	if(cap.capabilities & V4L2_CAP_TIMEPERFRAME){
		printf("capability : V4L2_CAP_TIMEPERFRAME\n");
	}

       memset(&fmtd, 0, sizeof(struct v4l2_fmtdesc));
       fmtd.index = pal = 0;
       fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

       p_array[0] = V4L2_PIX_FMT_SN9C10X;
       p_array[1] = V4L2_PIX_FMT_SBGGR16;
       p_array[2] = V4L2_PIX_FMT_SBGGR8;
       p_array[3] = V4L2_PIX_FMT_SPCA561;
       p_array[4] = V4L2_PIX_FMT_SGBRG8;
       p_array[5] = V4L2_PIX_FMT_SGRBG8;
       p_array[6] = V4L2_PIX_FMT_PAC207;
       p_array[7] = V4L2_PIX_FMT_PJPG;
       p_array[8] = V4L2_PIX_FMT_MJPEG;
       p_array[9] = V4L2_PIX_FMT_JPEG;
       p_array[10] = V4L2_PIX_FMT_RGB24;
       p_array[11] = V4L2_PIX_FMT_SPCA501;
       p_array[12] = V4L2_PIX_FMT_SPCA505;
       p_array[13] = V4L2_PIX_FMT_SPCA508;
       p_array[14] = V4L2_PIX_FMT_UYVY;
       p_array[15] = V4L2_PIX_FMT_YUYV;			/* first one */
       p_array[16] = V4L2_PIX_FMT_YUV422P;
       p_array[17] = V4L2_PIX_FMT_YUV420;
       p_array[18] = V4L2_PIX_FMT_Y10;
       p_array[19] = V4L2_PIX_FMT_Y12;
       p_array[20] = V4L2_PIX_FMT_GREY;
       p_array[21] = V4L2_PIX_FMT_H264;

       save_index = -1;

	while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtd) != -1) {
		printf("format %s (compressed:%d)\n", fmtd.description,fmtd.flags);
		for (index = 0; index <= V4L2_PALETTE_COUNT_MAX; index++){
			if (p_array[index] == fmtd.pixelformat){
				index_palette = index;

				if(camera_compress == COMPRESS_YUV422){
					if(index == 15){
						save_index = index;
					}
				}else if(camera_compress == COMPRESS_MJPEG){
					if(index == 8 || index == 9){
						save_index = index;
					}
				}
			}
		}

		memset(&fmtd, 0, sizeof(struct v4l2_fmtdesc));
		fmtd.index = ++pal;
		fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}
	if(save_index != -1){
		index_palette = save_index;
	}

	if(index_palette == 15){
		printf("selected format is YUV422\n");
	}else if(index_palette == 8){
		printf("selected format is Motion-JPEG\n");
	}else if(index_palette == 9){
		printf("selected format is JPEG\n");
	}else if(index_palette == 17){
		printf("selected format is YUV420\n");
	}else{
		printf("selected format is  UNKNOWN\n");
	}
	memset(&dst_fmt,0x00,sizeof(dst_fmt));
	dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_fmt.fmt.pix.width = camera_width;
	dst_fmt.fmt.pix.height = camera_height;
	dst_fmt.fmt.pix.pixelformat = p_array[index_palette];
	dst_fmt.fmt.pix.field = V4L2_FIELD_ANY;
	if (ioctl(fd, VIDIOC_TRY_FMT, &dst_fmt) != -1 ){
		if (ioctl(fd, VIDIOC_S_FMT, &dst_fmt) == -1) {
			fprintf(stderr,"VIDIOC_S_FMT error.(file=%s line=%d)\n",__FILE__,__LINE__);
			close(fd);
			goto top;
		}
	}else{
		fprintf(stderr,"VIDIOC_TRY_FMT error.(file=%s line=%d)\n",__FILE__,__LINE__);
		close(fd);
		goto top;
	}
	if((dst_fmt.fmt.pix.width != camera_width) || (dst_fmt.fmt.pix.height != camera_height)){
		printf("This camera is not support %dx%d\n",camera_width,camera_height);
	}

	vid.pframe = -1;
	memset(&vid.req, 0, sizeof(struct v4l2_requestbuffers));
	vid.req.count = MMAP_BUFFERS;
	vid.req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vid.req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &vid.req) == -1) {
		fprintf(stderr,"Err request buffers %d for memory map. VIDIOC_REQBUFS.(file=%s line=%d)\n",vid.req.count,__FILE__,__LINE__);
		close(fd);
		goto top;
	}

	buffer_count = vid.req.count;

	if (buffer_count < MIN_MMAP_BUFFERS) {
		fprintf(stderr,"Insufficient buffer memory %d < MIN_MMAP_BUFFERS.(file=%s line=%d)\n",
				buffer_count,__FILE__,__LINE__);
		close(fd);
		goto top;
	}

	vid.buffers = calloc(buffer_count, sizeof(video_buff));
	if (!vid.buffers) {
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		close(fd);
		goto top;
	}

	for (buffer_index = 0; buffer_index < buffer_count; buffer_index++) {
		memset(&buf2, 0, sizeof(struct v4l2_buffer));
		buf2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf2.memory = V4L2_MEMORY_MMAP;
		buf2.index = buffer_index;

		if (ioctl(fd, VIDIOC_QUERYBUF, &buf2) == -1) {
			fprintf(stderr,"Error querying buffer %i\nVIDIOC_QUERYBUF.(file=%s line=%d)\n",buffer_index,__FILE__,__LINE__);
			free(vid.buffers);
			vid.buffers = NULL;
			close(fd);
			goto top;
		}

		vid.buffers[buffer_index].size = buf2.length;
		vid.buffers[buffer_index].ptr = mmap(NULL, buf2.length,
							 PROT_READ | PROT_WRITE,
                                                     	 MAP_SHARED, fd, buf2.m.offset);

		if (vid.buffers[buffer_index].ptr == MAP_FAILED) {
			fprintf(stderr,"Error mapping buffer %i mmap.(file=%s line=%d)\n", buffer_index,__FILE__,__LINE__);
			free(vid.buffers);
			vid.buffers = NULL;
			close(fd);
			goto top;
		}
	}

	for (buffer_index = 0; buffer_index < buffer_count; buffer_index++) {

		memset(&vid.buf, 0, sizeof(struct v4l2_buffer));
		vid.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vid.buf.memory = V4L2_MEMORY_MMAP;
		vid.buf.index = buffer_index;

		if (ioctl(fd, VIDIOC_QBUF, &vid.buf) == -1) {
			fprintf(stderr,"VIDIOC_QBUF error.(file=%s line=%d)\n",__FILE__,__LINE__);
			close(fd);
			goto top;
		}
	}

	if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		fprintf(stderr,"Error starting stream. VIDIOC_STREAMON.(file=%s line=%d)\n",__FILE__,__LINE__);
		close(fd);
		goto top;
	}

	memset(&buf2, 0, sizeof(struct v4l2_buffer));
	buf2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf2.memory = V4L2_MEMORY_MMAP;
	buf2.bytesused = 0;

	clock_gettime(CLOCK_REALTIME,&ts1);

	while(1){
		
		clock_gettime(CLOCK_REALTIME,&ts3);
		if(exitflg){
			close(fd);
			return 0;
		}

		while(1){
			FD_ZERO(&readfds);
			FD_SET(fd, &readfds);
			tv.tv_sec = 0; tv.tv_usec = 1000;

			n = select(fd+1, &readfds, NULL, NULL, &tv);
			if(n == 0){
				if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {	// 接続確認用
					close(fd);
					goto top;
				}
				continue;
			}
			break;
		}

		if(ioctl(fd, VIDIOC_DQBUF, &buf2) == -1){
			close(fd);
			goto top;
		}

		vid.pframe = vid.buf.index;
		vid.buffers[vid.buf.index].used = vid.buf.bytesused;
		vid.buffers[vid.buf.index].content_length = vid.buf.bytesused;
		video_buff *the_buffer = &vid.buffers[vid.buf.index];

//printf("buf.index=%d\n",buf.index);
		the_buffer = &vid.buffers[buf2.index];

		if(((cur_sess_no <= 0) && (HassinNo == 0))){
			usleep(1000);
			goto next;
		}

		if((index_palette == 8) || (index_palette == 9)){
			if(EDIT_BUF_SIZE - 32 < buf2.bytesused){
				fprintf(stderr,"jpeg buffer is too small.(file=%s line=%d)\n",__FILE__,__LINE__);
				usleep(1000);
				goto next;
			}
//			memcpy(dest_buffer,the_buffer->ptr,buf2.bytesused);
//			send_size = get_jpeg_size((unsigned char *)dest_buffer,buf2.bytesused);
//printf("size1=%d\n",buf2.bytesused);
//printf("size2=%d\n",send_size);
//
// motion jpegでは１画像サイズより大きい値がbytesusedに入る！！１０倍くらいの値。ImageQualityが大きめだから？
// １画像のサイズを取得するために変換が必要である！！
// ImageQualityの設定も効かなくなる！！通信にかかる負荷が大きくなる！！
//
			rv = JPEGtoRGB((void *)the_buffer->ptr,buf2.bytesused,edit_buffer,&w,&h);
			if(rv != 0){
				goto next;
			}
			RGBtoYUV420(edit_buffer,w,h,edit_buffer3);

			send_size = YUV420toJPEG(dest_buffer,
					EDIT_BUF_SIZE ,edit_buffer3,w, h, ImageQuality);

		}else if(index_palette == 15){
			if(EDIT_BUF_SIZE -32 <  buf2.bytesused){
				fprintf(stderr,"jpeg buffer is too small.(file=%s line=%d)\n",__FILE__,__LINE__);
				usleep(1000);
				goto next;
			}
			yuv422to420(edit_buffer, the_buffer->ptr, camera_width, camera_height);
			send_size = YUV420toJPEG(dest_buffer,
					EDIT_BUF_SIZE ,edit_buffer,camera_width, camera_height, ImageQuality);
		}else if(index_palette == 17){
			if(EDIT_BUF_SIZE -32 <  buf2.bytesused){
				fprintf(stderr,"jpeg buffer is too small.(file=%s line=%d)\n",__FILE__,__LINE__);
				usleep(1000);
				goto next;
			}
			send_size = YUV420toJPEG(dest_buffer,
					EDIT_BUF_SIZE ,the_buffer->ptr,camera_width, camera_height, ImageQuality);
		}else{
			goto next;
		}
//
// 画像の送信
//
		if(MaxSendFps == 0){
			clock_gettime(CLOCK_REALTIME,&ts2);
			if((ts1.tv_sec + 30) <= ts2.tv_sec){
				sha_cal(256,dest_buffer,send_size,dest_buffer + send_size);
				send_data(1000*20,send_size + 32 , dest_buffer,PRO_SEND_IMG);
				usleep(1000*1000);
				clock_gettime(CLOCK_REALTIME,&ts1);
			}else{
				usleep(1000*1000);
			}
		}else if(0 < MaxSendFps){
			sha_cal(256,dest_buffer,send_size,dest_buffer + send_size);
			send_data(1000*20,send_size + 32 , dest_buffer,PRO_SEND_IMG);

			clock_gettime(CLOCK_REALTIME,&ts4);
			if(ts3.tv_sec == ts4.tv_sec){
				delta = ts4.tv_nsec - ts3.tv_nsec;
			}else{
				delta = ((1000000000 - ts3.tv_nsec) + ts4.tv_nsec);
			}
			if(delta/1000 > 1000000/MaxSendFps){
				sleep_time = 0;
			}else{
				sleep_time = 1000000/MaxSendFps - delta/1000;
			}
//			printf("delta=%ldmS sleeptime=%ldusec\n",delta/1000000,sleep_time);
			usleep(sleep_time);
		}
next:
		if (ioctl(fd, VIDIOC_QBUF, &buf2) == -1) {
			close(fd);
			goto top;
		}
	}
	return NULL;
}
/*****************************************************************************************/
/* Gtkコールバック関数*/
/*****************************************************************************************/
static void destroy_capture( GtkWidget *widget, gpointer   data )
{
	if(window3){
		stop_capture = 1;
		pthread_join(tid_capture,NULL);
		gtk_widget_destroy(GTK_WIDGET(window3));
		window3 = NULL;
		stop_capture = 0;
		capture_flg = 0;
	}
}

void *capture_thread(void *pParam)
{
	unsigned char	*buffer;
	FILE		*fp=NULL;
	int		rv;
	int		size;
	int		x,y,savex=-1,savey=-1,rx,ry;
	char		cmdb[256];


	GetResolution(&rx,&ry);

	buffer = calloc(1,camera_width*camera_height*3);
	if(buffer == NULL){
		fprintf(stderr,"calloc error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return NULL;
	}

	while(1){
		if(stop_capture || (window3 == NULL)){
			free(buffer);
			if(fp){
				if(pclose(fp) < 0){
					if(errno != ECHILD){
						fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
					}
				}
			}
			stop_capture=0;
			break;
		}
		if(MaxSendFps == 0){
			usleep(1000*10);
			continue;
		}

		gtk_window_get_position(GTK_WINDOW(window3),&x,&y);
		if((x != savex ) ||(y != savey)){

			savex = x;
			savey = y;


//			if(IsTrisquel()){
//				y += 25;
//			}else{
				y += 40;
//			}
			if(x < 0){
				x = 0;
			}
			if(y < 0){
				y = 0;
			}
			if(rx-x < width){
				x = (rx-width);
			}
			if(ry-y < height){
				y = ry - height;
			}
			sprintf(cmdb,"ffmpeg -loglevel quiet -f x11grab -video_size %dx%d -i :0.0+%d,%d -r %d  -pix_fmt yuv420p -vcodec rawvideo -f image2pipe  pipe:1 &",
				camera_width,camera_height,x,y,min(MaxSendFps,CaptureFps));

			if(fp){
				if(pclose(fp) < 0){
					if(errno != ECHILD){
						fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
					}
				}
			}
			fp = popen(cmdb,"r");
			if(fp == NULL){
				fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
				return NULL;
			}
		}

		if(fp == NULL){
			usleep(1000*1000);
			continue;
		}

		rv = fread(buffer,1,camera_width*camera_height*3,fp);
		if(rv != camera_width*camera_height*3){
			fprintf(stderr,"pipe read error.(file=%s line=%d)\n",__FILE__,__LINE__);
			usleep(1000*1000);
			if(pclose(fp) < 0){
				fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
			}
			fp = NULL;
			continue;
		}
		size = YUV420toJPEG(dest_buffer2,EDIT_BUF_SIZE,buffer,camera_width, camera_height, CaptureQuarity);
		sha_cal(256,dest_buffer2,size,dest_buffer2 + size);
		send_data(1000*20,size + 32 , dest_buffer2,PRO_SEND_IMG_SUB);
	}
	return NULL;
}
static void cb_capture( GtkWidget *checkbutton,GtkWidget *entry )
{
	GdkScreen	 *screen;
	GdkVisual	 *visual;
	FILE		*fp;
	char		buf[256];

	if(MaxSendFps == 0){
		return ;
	}
	if(cur_sess_no == 0){
		return;
	}
	if(capture_flg){
		return;
	}

	if((fp = popen("ffmpeg -version","r")) == NULL){
		fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		OKDialog(gettext("Ffmpeg is not installed\n"));
		return;
	}
	fgets(buf,sizeof(buf),fp);
	if(pclose(fp) < 0){
		fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		return;
	}
	if(memcmp(buf,"ffmpeg",6) != 0){
		fprintf(stderr,"ffmpeg not found.(file=%s line=%d)\n",__FILE__,__LINE__);
		OKDialog(gettext("Ffmpeg is not installed\n"));
		return;
	}



	capture_flg = 1;

	window3 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
//	gtk_window_set_keep_above(GTK_WINDOW(window3),TRUE);

	gtk_window_set_title(GTK_WINDOW(window3), gettext("Capture Window"));
	g_signal_connect(G_OBJECT(window3), "destroy", G_CALLBACK(destroy_capture), NULL);
	gtk_window_set_default_size (GTK_WINDOW (window3), camera_width, camera_height);
	gtk_window_set_resizable (GTK_WINDOW(window3), FALSE);

	gtk_widget_set_app_paintable(window3, TRUE);
	screen = gdk_screen_get_default();
	visual = gdk_screen_get_rgba_visual(screen);
	if (visual != NULL && gdk_screen_is_composited(screen)) {
		gtk_widget_set_visual(window3, visual);
	}

	gtk_widget_show_all(window3);


	if(pthread_create(&tid_capture, NULL, capture_thread, (void *)NULL) != 0){
		fprintf(stderr,"capture thread create error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}
	return;
}
static void cb_respond( GtkWidget *checkbutton,GtkWidget *entry )
{
	check_click();

	outouflg = 1;

	return;
}
static	void	*wait_thread(void *Param)
{
	int	status;
	pid_t	pid;

	while(1){
		usleep(1000*1000);

		if(exitflg){
			return NULL;
		}
//		if(wait(&status) < 0){
		if((pid = waitpid(0,&status,WNOHANG)) < 0){
			if(errno == ECHILD){
				continue;
			}
			fprintf(stderr,"wait error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		}
//printf("wait return code=%d pid=%d\n",status,pid);
	}
	return NULL;
}
static void cb_control_center( GtkWidget *button,gpointer user_data)
{
	FILE		*fp;
	char		buf[256];
	int		pid;

	fp = popen("ps aux | grep gnome-control-center | grep sound","r");
	if(fp == NULL){
		fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		return;
	}
	fgets(buf,sizeof(buf),fp);
	fgets(buf,sizeof(buf),fp);
	memset(buf,0x00,sizeof(buf));
	fgets(buf,sizeof(buf),fp);
	if(pclose(fp) < 0){
		fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		return;
	}

//	printf("buf=%s\n",buf);
//	my_system("gnome-control-center -s sound");

	if(buf[0] == 0){
		if((pid=fork())<0){
			fprintf(stderr,"fork error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
			return;
		}
		if(pid==0){ /* I'm child */
			execlp("sh","sh","-c","gnome-control-center -s sound",NULL);
			_exit(127);
		}
	}
}
static int	IsDigit(char *data)
{
	while(*data){
		if(*data < 0x30 || 0x39 < *data){
			return 0;
		}
		data++;
	}
	return 1;
}
static void cb_set( GtkWidget *button,gpointer user_data)
{
	GtkWidget 	*dialog;
	GtkWidget 	*label,*button4;
	GtkWidget 	*entry1,*entry2,*entry3;	//,*entry4;
	GtkWidget 	*check1,*check2,*check3;
	gint		response;
	char		buf[128];
	const 		gchar *entry_text1,*entry_text2,*entry_text3;	//,*entry_text4;
	int		fps,Iq;

	check_click();

top:

	dialog = gtk_dialog_new_with_buttons(gettext("Setting"), GTK_WINDOW(window),
		       	GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),
			GTK_RESPONSE_YES,gettext("CANCEL"),GTK_RESPONSE_NO, NULL);

	label = gtk_label_new(gettext("Max Send FPS(0~200)"));
	entry1 = gtk_entry_new ();
//	gtk_widget_set_size_request(entry1,50,50);
	gtk_entry_set_max_length (GTK_ENTRY (entry1), 3);

	sprintf(buf,"%d",MaxSendFps);
	gtk_entry_set_text (GTK_ENTRY (entry1), buf);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) , label);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry1);

	label = gtk_label_new(gettext("Hot line IP"));
	entry2 = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (entry2), 64);
	gtk_entry_set_text (GTK_ENTRY (entry2), hot_ip);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) , label);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry2);

	sprintf(buf,"%d",ImageQuality);
	label = gtk_label_new(gettext("Image Quality(0~100)"));
	entry3 = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (entry3), 3);
	gtk_entry_set_text (GTK_ENTRY (entry3), buf);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) , label);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry3);


	check1 = gtk_check_button_new_with_label(gettext("Ignore key share"));
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), check1);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check1),Ignoreflg);


	check2 = gtk_check_button_new_with_label(gettext("Multi mode"));
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), check2);
	if(mode == MODE_SINGLE){
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check2),FALSE);
	}else{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check2),TRUE);
	}

	check3 = gtk_check_button_new_with_label(gettext("Allow no crypto"));
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), check3);
	if(allow_no_crypto){
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check3),TRUE);
	}else{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check3),FALSE);
	}


	button4 = gtk_button_new_with_label(gettext("Sound setting"));
	g_signal_connect(G_OBJECT(button4), "clicked", G_CALLBACK(cb_control_center), NULL);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), button4);

//	g_signal_connect (entry1, "activate", G_CALLBACK (cb_dialog_entry1), dialog);

	gtk_widget_set_size_request(dialog, 50, 60);
	gtk_widget_show_all(dialog);


	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check3))){
		OKDialog(gettext("Unencrypted communication is allowed.\nThis poses a risk of communication being intercepted and It makes attacking this phone easy.\nNot recommended.\n"));
	}


	response = gtk_dialog_run(GTK_DIALOG(dialog));


	if(response == GTK_RESPONSE_YES){
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check1))){
			Ignoreflg = TRUE;
			my_strncpy(buf,"ON",sizeof(buf));
		}else{
			Ignoreflg = FALSE;
			my_strncpy(buf,"OFF",sizeof(buf));
		}
		update_params("IGNORE_KEY_SHARING",buf);

		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check2))){
			mode = MODE_MULTI;
			my_strncpy(buf,"ON",sizeof(buf));
		}else{
			mode = MODE_SINGLE;
			my_strncpy(buf,"OFF",sizeof(buf));
		}
		update_params("MULTI_MODE",buf);

		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check3))){
			allow_no_crypto = 1;
			my_strncpy(buf,"ON",sizeof(buf));
			OKDialog(gettext("Unencrypted communication is allowed.\nThis poses a risk of communication being intercepted and It makes attacking this phone easy.\nNot recommended.\n"));
		}else{
			allow_no_crypto = 0;
			my_strncpy(buf,"OFF",sizeof(buf));
		}
		update_params("ALLOW_NO_CRYPTO",buf);

		entry_text1 = gtk_entry_get_text (GTK_ENTRY (entry1));
		if(IsDigit((char *)entry_text1) == 0){
			gtk_widget_destroy(dialog);
			goto top;
		}
		fps = atoi(entry_text1);
		if((0 <= fps) && (fps <= 200)){
			MaxSendFps = fps;
			sprintf(buf,"%d",fps);
			update_params("MAX_SEND_FPS",buf);
		}else{
			gtk_widget_destroy(dialog);
			goto top;
		}

		entry_text2 = gtk_entry_get_text (GTK_ENTRY (entry2));
		if(entry_text2[0]){
			if(IsIP((char *)entry_text2) == 0){
				OKDialog(gettext("Incorrect IP address\n"));
				gtk_widget_destroy(dialog);
				goto top;
			}
			if(list_from_ip((char *)entry_text2) == NULL){
				OKDialog(gettext("Please specify the HOT IP registered in the address book.\n"));
				gtk_widget_destroy(dialog);
				goto top;
			}

			if(strcmp(hot_ip,entry_text2)){
				my_strncpy(hot_ip,entry_text2,sizeof(hot_ip));
				update_address_book2();
			}
		}else{
			hot_ip[0] = 0;
			update_address_book2();
		}

		entry_text3 = gtk_entry_get_text (GTK_ENTRY (entry3));
		if(IsDigit((char *)entry_text3) == 0){
			gtk_widget_destroy(dialog);
			goto top;
		}
		Iq = atoi(entry_text3);
		if((0 <= Iq) && (Iq <= 100)){
			ImageQuality = Iq;
			sprintf(buf,"%d",Iq);
			update_params("CAMERA_IMAGE_QUALITY",buf);
		}else{
			gtk_widget_destroy(dialog);
			goto top;
		}
	}
	gtk_widget_destroy(dialog);
}

static	int	set_delay(char *fname)
{
	char			buf[256],*str,*stop;
        RECVLIST		*list;
	struct	timespec	now1;
	time_t			now2,delta;
	int			rv,i;

	my_strncpy(buf,fname,sizeof(buf));

	str = strtok_r(buf,"-",&stop);
	str = strtok_r(NULL,"w",&stop);
	*(str + strlen(str) -1) = 0x00;
//printf("IP=%s\n",str);
	if(IsIP(str) == 0){
		return 0;
	}
	if(strcmp(str,my_ip) == 0){
		return 0;
	}

	clock_gettime(CLOCK_REALTIME,&now1);
	now2 = time(NULL);
	delta = (now2 - rec_start_time);

	list = find_recv_list(str);
	if(list == NULL){
//
// 退席者配列を検索する
//
		for(i=0;i<32;i++){
			if(strcmp(RecArray[i].Ip,str) == 0){
				if(delta <= (now1.tv_sec - RecArray[i].time.tv_sec)){
					rv = 0;
				}else{
					rv = (delta -  (now1.tv_sec - RecArray[i].time.tv_sec));
				}
				return rv;
			}
		}

		return 0;
	}

	if(delta <= (now1.tv_sec - list->recv_time.tv_sec)){
		rv = 0;
	}else{
		rv = (delta -  (now1.tv_sec - list->recv_time.tv_sec));
	}
	return rv;
}

void *rec_thread(void *pParam)
{
	char		*str,*stop,buf[1024];
	FILE		*fp;
	char 		filename[512];
	struct tm 	*local;
	RECVLIST	*list;
	int		cnt,delay[32],i;
	struct stat	st;

//
// 音声ファイルの書き込み終了を指示する
//
	pthread_mutex_lock(&m_recv_list);
	list = recv_list_top;
	while(list){
		list->recflg = 0;
		list = list->next;
	}
	pthread_mutex_unlock(&m_recv_list);

	usleep(1000*1000);				// Wait write_thread close file.
//
// 音声ファイルを１個に合成する
//
	my_system("ls -1 /tmp/ihaphone_V-*.wav > /tmp/ihaphone_voicelist.txt");

	fp = fopen("/tmp/ihaphone_voicelist.txt","r");
	if(fp == NULL){
		fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
		my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
		g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
		return NULL;
	}

	my_strncpy(cmdbuf,"ffmpeg -y",CMD_BUF_SIZE);
	cnt = 0;
	while(1){
		if(fgets(buf,sizeof(buf),fp) == NULL){
			break;
		}
		str = strtok_r(buf,"\n",&stop);
		if(str == NULL){
			fprintf(stderr,"Internal contradiction!!(file=%s line=%d)\n",__FILE__,__LINE__);
			g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
			my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
			g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
			return NULL;
		}
		if(cnt >= 32){
			fprintf(stderr,"Too many sound files.(file=%s line=%d)\n",__FILE__,__LINE__);
			break;
		}
		strcat(cmdbuf," -i ");
		strcat(cmdbuf,str);
		my_strncpy(filename,str,sizeof(filename));

		delay[cnt] = set_delay(filename);
//printf("delay[%d]=%d\n",cnt,delay[cnt]);
		if(strlen(cmdbuf) > (CMD_BUF_SIZE - 256 - 40*32)){
			fprintf(stderr,"Buffer over flow.(file=%s line=%d)\n",__FILE__,__LINE__);
			g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
			my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
			g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
			return NULL;
		}
		cnt++;
	}

	fclose(fp);

	strcat(cmdbuf," -filter_complex \"");
	for(i=0;i<cnt;i++){
		sprintf(buf,"[%d:a]adelay=%ds:all=1[a%d];",i,delay[i],i);
		strcat(cmdbuf,buf);
	}
	for(i=0;i<cnt;i++){
		sprintf(buf,"[a%d]",i);
		strcat(cmdbuf,buf);
	}
	sprintf(buf,"amix=inputs=%d:duration=longest\" /tmp/ihaphone_voice.wav > /dev/null 2>&1",cnt);
	strcat(cmdbuf,buf);

//printf("cmd=%s\n",cmdbuf);

	if(cnt > 1){
		if(my_system(cmdbuf) != 0 ){		// 音声ファイルを合成する。
			fprintf(stderr,"ffmpeg error.(file=%s line=%d)\n",__FILE__,__LINE__);
			g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
			my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
			g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
			return NULL;
		}
	}else if(cnt == 1){				// 合成の必要なし。コピーしておしまい。
		sprintf(cmdbuf,"cp %s /tmp/ihaphone_voice.wav",filename);
		my_system(cmdbuf);
	}else{
		fprintf(stderr,"voice file not found.(file=%s line=%d)\n",__FILE__,__LINE__);
		g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
		my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
		g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
		return NULL;
	}
//
// thread毎のvoiceファイルを削除する。
//
	my_system("rm /tmp/ihaphone_V-*.wav > /dev/null 2>&1");
	my_system("rm /tmp/ihaphone_voicelist.txt > /dev/null 2>&1");
	my_system("sync");
//
// ffmpegをクリーンに終了させる。
//
	fwrite("q",1,1,ffp);
	if(pclose(ffp) < 0){
		fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
	}
	ffp = NULL;
//
// voice,imageファイルができているかをチェックする。
//
	stat("/tmp/ihaphone_voice.wav",&st);
	if(st.st_size == 0){
		fprintf(stderr,"ffmpeg error.(file=%s line=%d)\n",__FILE__,__LINE__);
		g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
		my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
		g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
		return NULL;
	}
	stat("/tmp/ihaphone_image.mp4",&st);
	if(st.st_size == 0){
		fprintf(stderr,"ffmpeg error.(file=%s line=%d)\n",__FILE__,__LINE__);
		g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);
		my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
		g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
		return NULL;
	}
//
// 画像動画と音声ファイルを合成する
//
	local = localtime(&rec_start_time);

	sprintf(filename,"%s/%d_%02d_%02d_%02d_%02d_%02d.mp4",rec_file_dir,
			local->tm_year + 1900,
			local->tm_mon + 1,local->tm_mday,
			local->tm_hour,local->tm_min,local->tm_sec);
	sprintf(buf,"ffmpeg -i /tmp/ihaphone_image.mp4 -i /tmp/ihaphone_voice.wav -c:v copy -c:a aac   /tmp/ihaphone_image_voice.mp4 > /dev/null 2>&1");
//NG	sprintf(buf,"ffmpeg -i /tmp/ihaphone_image.mp4 -i /tmp/ihaphone_voice.wav -c:v copy -c:a aac   %s > /dev/null 2>&1",filename);
	if(my_system(buf) != 0){
		fprintf(stderr,"ffmpeg error.(file=%s line=%d)\n",__FILE__,__LINE__);
		my_strncpy(DialogMessage,gettext("Recording failed.\n"),sizeof(DialogMessage));
		g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
	}
//NG	sprintf(buf,"mv /tmp/ihaphone_image_voice.mp4 %s",filename);
//NG	if(my_system(buf) == -1){
	if(copy_file("/tmp/ihaphone_image_voice.mp4",filename) == -1){
		my_strncpy(DialogMessage,gettext("Recording failed.\nCheck the setting of REC_FILE_DIR in the parameter file.\n"),sizeof(DialogMessage));
		g_signal_emit_by_name(G_OBJECT(window ), "MessageDialog",NULL);
	}
	my_system("rm /tmp/ihaphone_image.mp4 > /dev/null 2>&1");
	my_system("rm /tmp/ihaphone_voice.wav > /dev/null 2>&1");
	my_system("rm /tmp/ihaphone_image_voice.mp4 > /dev/null 2>&1");
	my_system("sync");

	g_signal_emit_by_name(G_OBJECT(window ), "DeleteWdialog",NULL);

	return NULL;
}
static void cb_rec (GtkWidget *bt, gpointer user_data)
{
	char		buf[512];
	FILE		*fp;
	int		x,y,w,h,rx,ry;
	RECVLIST	*list;
	time_t		timer;
        GtkWidget       *label1;
	pthread_t	tid;

	check_click();

//	if((recflg == 0) && (cur_sess_no == 0)){
//		return;
//	}

	if(bt || user_data){			// 録画開始から５秒以内に終了された場合を考慮
		timer = time(NULL);
		if(timer - g_timer < 5){	// 連打の禁止！！
			return;
		}
	}
	g_timer = time(NULL);
//
// ffmpegがインストールされているか確認する。
//
	if((fp = popen("ffmpeg -version","r")) == NULL){
		fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		OKDialog(gettext("Ffmpeg is not installed\n"));
		return;
	}
	fgets(buf,sizeof(buf),fp);
	if(pclose(fp) < 0){
		fprintf(stderr,"pclose error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
		return;
	}
	if(memcmp(buf,"ffmpeg",6) != 0){
		fprintf(stderr,"ffmpeg not found.(file=%s line=%d)\n",__FILE__,__LINE__);
		OKDialog(gettext("Ffmpeg is not installed\n"));
		return;
	}


	if(recflg == 0){

		my_system("rm /tmp/ihaphone_V-*.wav  > /dev/null 2>&1");
		my_system("rm /tmp/ihaphone_voicelist.txt  > /dev/null 2>&1");
		my_system("rm /tmp/ihaphone_voice.wav > /dev/null 2>&1");
		my_system("rm /tmp/ihaphone_image.mp4  > /dev/null 2>&1");
		my_system("sync");

		if(ffp){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		list = recv_list_top;
		while(list){
			list->recflg = 1;
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);

		gtk_window_set_keep_above(GTK_WINDOW(window),TRUE);

		gtk_window_set_decorated(GTK_WINDOW(window),FALSE);
// 画面の乱れをなくすためにサイズを少し変える？？
		gtk_widget_set_size_request(GTK_WIDGET(drawingarea),(int)(width*pow(mag,zoom_cnt))-1,(int)(height*pow(mag,zoom_cnt))-1);

		gtk_window_get_position(GTK_WINDOW(window),&x,&y);

		GetResolution(&rx,&ry);
//printf("x=%d y=%d rx=%d ry=%d\n",x,y,rx,ry);
	
		if(y < 0){		// return -10 when full screen
			y = MENU_HEIGHT;
		}

		if(FullFlg){
			h = ry - y;
			w = rx;
		}else{
			h = min(ry-y,(int)((double)height*pow(mag,zoom_cnt)+MENU_HEIGHT));
			w = min(rx-x,(int)((double)width*pow(mag,zoom_cnt)));
		}
//		h -= 1;			// 小さくした分
//		w -= 1;			// 小さくした分
		h -= h%2;		// 偶数にする
		w -= w%2;		// 偶数にする

//printf("h=%d w=%d \n",h,w);

		sprintf(buf,"ffmpeg -y -f x11grab -video_size %dx%d -i :0.0+%d,%d -r %d -vcodec h264 -pix_fmt yuv420p /tmp/ihaphone_image.mp4 > /dev/null 2>&1",w,h,x,y,CaptureFps);
		ffp = popen(buf,"w");
		if(ffp == NULL){
			fprintf(stderr,"popen error errno=%d.(file=%s line=%d)\n",errno,__FILE__,__LINE__);
			return;
		}

		rec_start_time = time(NULL);

		memset(RecArray,0x00,sizeof(RecArray));

		gtk_button_set_label(GTK_BUTTON(button_rec),gettext("Stop Rec"));

		gtk_widget_show(button_rec);
		gtk_widget_show_all(window);
		recflg = 1;

	}else{
		if(pthread_create(&tid, NULL, rec_thread, (void *)NULL) == 0){

			Wdialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window),
                        	GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL,NULL,NULL);

			gtk_window_set_decorated(GTK_WINDOW(Wdialog),FALSE);
	
			label1 = gtk_label_new(gettext("\nWait for while"));
			gtk_container_add(
			GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Wdialog))) ,label1);
			gtk_widget_show_all(Wdialog);
			gtk_dialog_run(GTK_DIALOG(Wdialog));

//			pthread_join(tid,NULL);
		}

		gtk_button_set_label(GTK_BUTTON(button_rec),gettext("Start Rec"));
		gtk_window_set_decorated(GTK_WINDOW(window),TRUE);
		gtk_window_set_keep_above(GTK_WINDOW(window),FALSE);

		gtk_widget_set_size_request(GTK_WIDGET(drawingarea),width*pow(mag,zoom_cnt),height*pow(mag,zoom_cnt));

		gtk_widget_show(button_rec);

		recflg = 0;
	}
	return;
}
static void cb_hotline (GtkWidget *button, gpointer user_data)
{
	KEYLIST		*list;

	check_click();

	if(hot_ip[0]){
		list = key_list_top;
		while(list){
			if((list->Kind == KIND_HOT) && (find_send_list(list->Ip) == NULL)){
				
				if(list->KeyFileName[0]){
					add_send_list(hot_ip,PRO_SEND_IMG);
				}else{
					if(allow_no_crypto == 1){
						add_send_list(list->Ip,PRO_SEND_IMG_NO_CRYPTO);
					}else{
						OKDialog(gettext("Not allowed no crypto\n"));
						return;
					}
				}
				list->SaveKind = list->Kind;
				list->Kind = KIND_CALLING;
				list->CallCnt = CALL_TIME_OUT;
				HassinNo++;
				break;
			}
			list = list->next;
		}
	}else{
		OKDialog(gettext("\nDirect IP is not set.\n"));
	}
	return;
}
/*****************************************************************************************/
/* 送受信リスト処理関数*/
/*****************************************************************************************/
static	void create_model(GtkWidget *treeview)
{
	GtkListStore *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new(
		4,             //要素の個数
		G_TYPE_STRING, //Name
		G_TYPE_STRING, //IP Address
		G_TYPE_STRING, //Kind
		G_TYPE_STRING  //key
	);

	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(treeview));

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_object_unref(store);
}

static	void create_model2(GtkWidget *treeview)
{
  GtkListStore *store;

  store = gtk_list_store_new(
    2,             //要素の個数
    G_TYPE_STRING, //Name
    G_TYPE_STRING  //IP Address
  );

  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));

  g_object_unref(store);
}

static	void set_data(GtkWidget *treeview,gpointer user_data)
{
	GtkListStore	*store;
	GtkTreeIter	iter;
	char		name[256];
	char		IP[128],Kind[32],key[513];
	KEYLIST		*list;
	int		kind;

//printf("set_data user_data=%s\n",(char *)user_data);

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
	gtk_list_store_clear(store);


	list = key_list_top;
	while(list){
		memset((char *)name,0x00,sizeof(name));
		my_strncpy((char *)name,list->Name,sizeof(name));
		my_strncpy(IP,list->Ip,sizeof(IP));
		kind = list->Kind;
		my_strncpy(key,list->KeyFileName,sizeof(key));

		if(kind == KIND_ALLOW){
			my_strncpy(Kind,"ALLOW",sizeof(Kind));
		}else if(kind == KIND_DENY){
			my_strncpy(Kind,"DENY",sizeof(Kind));
		}else if(kind == KIND_HOT){
			my_strncpy(Kind,"HOT",sizeof(Kind));
		}else if(kind == KIND_CALLING){
			my_strncpy(Kind,"CALLING",sizeof(Kind));
		}else{
			Kind[0] = 0x00;
		}

		if(user_data == NULL){
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, name, 1, IP, 2, Kind,3,key, -1);
		}else{
//printf("set_data name=%s user=%s\n",(char *)name,(char *)user_data);
			if(strstr((const char *)name,(const char *)user_data)){
				gtk_list_store_append(store, &iter);
				gtk_list_store_set(store, &iter, 0, name, 1, IP, 2, Kind,3,key, -1);
			}
		}
		list = list->next;
	}
	return;
}

static	void set_data2(GtkWidget *treeview)
{
	GtkListStore	*store;
	GtkTreeIter	iter;
	int		i;
	RECVLIST	*list;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)));
	gtk_list_store_clear(store);

	pthread_mutex_lock(&m_recv_list);
	list = recv_list_top;
	for(i=0 ; (i < MAX_CLI) && list ;i++){
		if(list->ch == 0){
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, name_from_ip(list->Ip), 1, list->Ip, -1);
		}
		list = list->next;
	}
	pthread_mutex_unlock(&m_recv_list);
}

static	void append_column_to_treeview(GtkWidget *treeview, const char *title,
					 const int order)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  renderer = gtk_cell_renderer_text_new();

  column = gtk_tree_view_column_new_with_attributes( title, renderer, "text", order, NULL);

  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
}

static	void create_view(GtkWidget *treeview)
{
  append_column_to_treeview(treeview, gettext("Name"), 0);
  append_column_to_treeview(treeview, gettext("IP Address"), 1);
  append_column_to_treeview(treeview, gettext("Kind"), 2);
  append_column_to_treeview(treeview, gettext("KEY"), 3);
}

static	void create_view2(GtkWidget *treeview)
{
  append_column_to_treeview(treeview, gettext("Name"), 0);
  append_column_to_treeview(treeview, gettext("IP Address"), 1);
}

static void destroy_win( GtkWidget *widget, gpointer   data )
{
	KEYLIST		*klist,*klist2;
	int		i;

//
// キャプチャーウィンドウの停止
//
	if(window3){
		stop_capture = 1;
		pthread_join(tid_capture,NULL);
		destroy_capture(window3,NULL);
		window3 = NULL;
		capture_flg = 0;
	}
//
// 録画中の場合は終了させる
//
	if(recflg){
		cb_rec(NULL,NULL);
	}
//
//　その他スレッドの終了待ち
//
//	usleep(1000*100);
//
	exitflg = 1;
	pthread_join(tid1,NULL);
	pthread_join(tid2,NULL);
	pthread_join(tid3,NULL);
	pthread_join(tid4,NULL);
	pthread_join(tid5,NULL);
	pthread_join(tid6,NULL);
	pthread_join(tid7,NULL);
	pthread_join(tid8,NULL);
	pthread_join(tid9,NULL);
//
// 受信リストの開放(key listを開放する前にやること)
//
	pthread_mutex_lock(&m_recv_list);
	while(recv_list_top){
		del_recv_list(recv_list_top->Ip);
	}
	pthread_mutex_unlock(&m_recv_list);
//
// key list関連メモリの開放
//
	klist = key_list_top;
	klist2 = key_list_top;
	while(klist2){
		klist2 = klist->next;

		EVP_CIPHER_CTX_free(klist->en);
		EVP_CIPHER_CTX_free(klist->ed);
		finish_cipher(klist->ctx);
		free(klist);

		klist = klist2;
	}
//
// ソケットのクローズ
//
	for(i=0;i<MAX_CLI;i++){
		if(SendArray[i].sock != -1){
			close(SendArray[i].sock);
		}
	}
//
// その他メモリの開放
//
	if(init_image_buf){
	       free(init_image_buf);
	}
	if(edit_buffer){
		free(edit_buffer);
	}
	if(edit_buffer2){
		free(edit_buffer2);
	}
	if(edit_buffer3){
		free(edit_buffer3);
	}
	if(dest_buffer)
		free(dest_buffer);
	if(dest_buffer2){
		free(dest_buffer2);
	}
//	if(sA){
//		free(sA);
//	}
	if(FileBuffer1){
		free(FileBuffer1);
	}
	if(FileBuffer2){
		free(FileBuffer2);
	}
	if(readbuf){
		free(readbuf);
	}
	if(readbuf2){
		free(readbuf2);
	}
	if(jpeg_scanline){
		free(jpeg_scanline);
	}
	if(jpeg_scanline2){
		free(jpeg_scanline2);
	}
	if(cmdbuf){
		free(cmdbuf);
	}
	if(selected_list){
		free(selected_list);
	}
//
// 描画の後処理
//
	if(surf){
		cairo_surface_destroy(surf);
	}
	if(surf2){
		cairo_surface_destroy(surf2);
	}
	if(surf3){
		cairo_surface_destroy(surf3);
	}
	if(surf4){
		cairo_surface_destroy(surf4);
	}

#if GTK_CHECK_VERSION(3,22,0)
	cairo_region_destroy(clip);
	cairo_destroy(gcr);
#else
	cairo_destroy(gcr);
#endif

	gtk_main_quit ();
}

GtkWidget 	*s_entry1;
static void cb_search_list (GtkWidget *button, gpointer user_data)
{
        const           gchar *entry_text1;
	char		text[256];

	entry_text1 = gtk_entry_get_text (GTK_ENTRY (s_entry1));

	my_strncpy(text,entry_text1,sizeof(text));
        memset(selected_list,0x00,max_key_list_no*64);
        gtk_widget_destroy(list_dialog);

	cb_connect(NULL,(gpointer)text);
}
static void cb_selected_list (GtkWidget *button, gpointer user_data)
{
	int		index,cnt;

	KEYLIST		*list;
        GtkWidget       *label1;
	gint		response;
        GtkWidget       *dialog,*dialog_entry1;
        const           gchar *entry_text1;

	index = 0;
	cnt   = 0;
//	delflg = 0;

	while(1){
		if(index >= max_key_list_no){
			break;
		}
		list = list_from_ip(selected_list + index*64);
		if(list == NULL){
			break;
		}
//printf(" cb_selected_list : ip=%s\n",list->Ip);
		if(list->Kind == KIND_CALLING){
			list->Kind = list->SaveKind;
			list->CallCnt = 0;
			del_send_list(list->Ip);
			if(HassinNo >= 1){
				HassinNo--;
			}

			if(strcmp(list->Name,"specify ip") == 0){
				strcpy(list->Ip,"*");
			}
		}else{
			if(find_send_list(list->Ip)){
				goto skip;
			}

			if(strcmp(list->Name,"specify ip") == 0){
				if(strcmp(list->Ip,"*") != 0){
					goto skip;
				}
				if((list->Kind != KIND_DENY) && (strcmp(list->Ip,"*") == 0)){
					if(allow_no_crypto == 0){
						OKDialog(gettext("Not allowed no crypto\n"));
						goto skip;
					}
					dialog = gtk_dialog_new_with_buttons(gettext("Entry ip address"), GTK_WINDOW(window),
						GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						gettext("YES"),GTK_RESPONSE_YES, gettext("NO") , GTK_RESPONSE_NO,NULL);
					label1 = gtk_label_new(gettext("IP Address"));
					gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,label1);
					dialog_entry1 = gtk_entry_new ();
					gtk_entry_set_max_length (GTK_ENTRY (dialog_entry1), 64);
					gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,dialog_entry1);

					gtk_widget_show_all(dialog);
					response = gtk_dialog_run(GTK_DIALOG(dialog));
					if(response == GTK_RESPONSE_YES){

						entry_text1 = gtk_entry_get_text (GTK_ENTRY (dialog_entry1));
						if(IsIP((char *)entry_text1) == 0){
							gtk_widget_destroy(dialog);

							OKDialog(gettext("Incorrect IP address\n"));
							goto skip;
						}

						if(list_from_ip((char *)entry_text1)){
							strcpy(list->Ip,"*");
						
							gtk_widget_destroy(dialog);

							OKDialog(gettext("Ip address is in the list.\nPlease select from the list.\n"));
							goto skip;
						}
						my_strncpy(list->Ip,entry_text1,sizeof(list->Ip));
						gtk_widget_destroy(dialog);
						if(find_send_list(list->Ip)){	// 既に発信中
//							strcpy(list->Ip,"*");
							goto skip;
						}
					}else{
						gtk_widget_destroy(dialog);
						goto skip;
					}
				}
			}


			if((list->Kind != KIND_DENY) && (cnt < MAX_CLI)){
				if((strcmp(list->Name,"specify ip") == 0) || (list->KeyFileName[0] == 0)){
					if(allow_no_crypto == 0){
						OKDialog(gettext("Not allowed no crypto\n"));
						goto skip;
					}else{
						add_send_list(list->Ip,PRO_SEND_IMG_NO_CRYPTO);
					}
				}else{
					add_send_list(list->Ip,PRO_SEND_IMG);
				}
				list->SaveKind = list->Kind;
				list->Kind = KIND_CALLING;
				list->CallCnt = CALL_TIME_OUT;

				HassinNo++;
				cnt++;
//				delflg = 1;
			}
		}
skip:
		index++;
	}
 	memset(selected_list,0x00,max_key_list_no*64);

//	if(delflg){
		gtk_widget_destroy(list_dialog);
		list_dialog = NULL;
//	}
}
static void signal_changed_list (GtkTreeSelection *select, gpointer user_data)
{
	gint i,j;
	GtkTreeIter iter;
	GtkTreeModel *model;
  
	gchar *row_string;

	memset(selected_list,0x00,max_key_list_no*64);

	model = gtk_tree_view_get_model (gtk_tree_selection_get_tree_view (select));
	for (i = 0,j = 0; gtk_tree_model_iter_nth_child (model, &iter, NULL, i); i++){
		if (gtk_tree_selection_iter_is_selected (select, &iter)){
			gtk_tree_model_get (model, &iter, 1, &row_string, -1);
//printf("signal_changed_list i=%d j=%d row_string=%s\n",i,j,row_string);
			strcpy(selected_list + j*64,row_string);
			j++;
		}
	}
}

static void cb_selected_list_can (GtkWidget *button, gpointer user_data)
{
 	memset(selected_list,0x00,max_key_list_no*64);
	gtk_widget_destroy(list_dialog);
	list_dialog = NULL;

}
static void cb_connect (GtkWidget *button, gpointer user_data)
{
	GtkWidget 	*sw,*box,*box2,*box3;
	GtkWidget 	*button2,*button3,*button4;

//printf("cb_connect run!! user_data=%s\n",(char *)user_data);

	check_click();


	list_dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL,NULL,NULL);


	gtk_window_set_title(GTK_WINDOW(list_dialog), gettext("Connect list"));

	gtk_window_set_default_size (GTK_WINDOW (list_dialog), 600, 500);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request(sw, 600,480);
	gtk_widget_set_hexpand (sw, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
			 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	treeview = gtk_tree_view_new();

	create_model(treeview);

	set_data(treeview,user_data);

	create_view(treeview);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(list_dialog))) , box);

	gtk_container_add (GTK_CONTAINER (box), sw);
	gtk_container_add (GTK_CONTAINER (box), box3);
	gtk_container_add (GTK_CONTAINER (box), box2);


	button2 = gtk_button_new_with_label(gettext("OK"));
	g_signal_connect(G_OBJECT(button2), "clicked", G_CALLBACK(cb_selected_list), NULL);

	button3 = gtk_button_new_with_label(gettext("CANCEL"));
	g_signal_connect(G_OBJECT(button3), "clicked", G_CALLBACK(cb_selected_list_can), NULL);

	gtk_container_add (GTK_CONTAINER (box2), button2);
	gtk_container_add (GTK_CONTAINER (box2), button3);

	gtk_widget_set_size_request(button2,300,50);
	gtk_widget_set_size_request(button3,300,50);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button2), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button3), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button4 = gtk_button_new_with_label(gettext("Search"));
	g_signal_connect(G_OBJECT(button4), "clicked", G_CALLBACK(cb_search_list), NULL);
	gtk_widget_set_size_request(button4,100,30);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button4), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	s_entry1 = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (s_entry1), 60);
	gtk_widget_set_size_request(s_entry1,500,30);

	gtk_container_add (GTK_CONTAINER (box3), s_entry1);
	gtk_container_add (GTK_CONTAINER (box3), button4);


	GtkTreeSelection *select;
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (select), "changed",
                                    G_CALLBACK (signal_changed_list), NULL);
	
	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show_all(list_dialog);
}

static void cb_selected_list2 (GtkWidget *button, gpointer user_data)
{
	int		i;
        RECVLIST	*list;

//	pthread_mutex_lock(&m_recv_list);
//	list = recv_list_top;
        for(i=0 ; i < MAX_CLI ;){
		list = find_recv_list(selected_list + i*64);
		if(list == NULL){
			i++;
			continue;
		}
		set_kind_ip(list->Ip,KIND_DENY);	// 着信拒否を設定
		del_send_list(list->Ip);
		chgflg = 1;
		if(list->ch == 0){
			i++;
		}
//		list = list->next;
        }
//	pthread_mutex_unlock(&m_recv_list);


        memset(selected_list,0x00,max_key_list_no*64);
        gtk_widget_destroy(list_dialog);
        list_dialog = NULL;
}

static void signal_changed_list2 (GtkTreeSelection *select, gpointer user_data)
{
        gint i;
        GtkTreeIter iter;
        GtkTreeModel *model;

        gchar *row_string;

        memset(selected_list,0x00,max_key_list_no*64);

        model = gtk_tree_view_get_model (gtk_tree_selection_get_tree_view (select));
        for (i = 0; gtk_tree_model_iter_nth_child (model, &iter, NULL, i); i++){
                if (gtk_tree_selection_iter_is_selected (select, &iter)){

                        gtk_tree_model_get (model, &iter, 1, &row_string, -1);
//printf("signal_changed_list2 %d %s\n",i,row_string);
			strcpy(selected_list + i*64,row_string);
                }
        }

}

static void cb_disconnect (GtkWidget *button, gpointer user_data)
{
	GtkWidget	*sw,*box,*box2;
	GtkWidget 	*button2,*button3;

	check_click();

	list_dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL,NULL,NULL);

	gtk_window_set_title(GTK_WINDOW(list_dialog), gettext("Disconnect list"));

	gtk_window_set_default_size (GTK_WINDOW (list_dialog), 400, 500);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_size_request(sw, 400,480);
	gtk_widget_set_hexpand (sw, TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	treeview = gtk_tree_view_new();

	create_model2(treeview);

	set_data2(treeview);

	create_view2(treeview);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(list_dialog))) , box);
	gtk_container_add (GTK_CONTAINER (box), sw);
	gtk_container_add (GTK_CONTAINER (box), box2);

	button2 = gtk_button_new_with_label(gettext("OK"));
	g_signal_connect(G_OBJECT(button2), "clicked", G_CALLBACK(cb_selected_list2), NULL);
	button3 = gtk_button_new_with_label(gettext("CANCEL"));
	g_signal_connect(G_OBJECT(button3), "clicked", G_CALLBACK(cb_selected_list_can), NULL);


	gtk_container_add (GTK_CONTAINER (box2), button2);
	gtk_container_add (GTK_CONTAINER (box2), button3);


	gtk_widget_set_size_request(button2,200,50);
	gtk_widget_set_size_request(button3,200,50);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button2),
                GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button3),
                GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);


	GtkTreeSelection *select;
        select = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_set_mode (select, GTK_SELECTION_MULTIPLE);
        g_signal_connect (G_OBJECT (select), "changed",
                                    G_CALLBACK (signal_changed_list2), NULL);

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show_all(list_dialog);
}
static void cb_license (GtkWidget *button, gpointer user_data)
{
	check_click();

	gchar	translator[128];
	gchar	*auth[10] = {gettext("Katunori Iha"),NULL};
	gchar	*trans1 = gettext("Katunori Iha(English)\n");
	gchar	*trans2 = gettext("josipandgrace(Croatian)");
	gchar	*artists[2] = {gettext("OtoLogic"),NULL};
	my_strncpy(translator,trans1,sizeof(translator));
	strcat(translator,trans2);

	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file("./logo.jpg",NULL);

	GtkWidget *about_dialog = gtk_about_dialog_new();
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_dialog),"IHAPHONE");
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dialog),"2.0.8");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_dialog),"Copyright ©  2018-2023 Katunori Iha");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_dialog),gettext("secure calls for all people"));
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_dialog),"https://izumi.earth/");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_dialog),(const gchar **)auth);
	gtk_about_dialog_set_documenters(GTK_ABOUT_DIALOG(about_dialog),(const gchar **)auth);
	gtk_about_dialog_set_translator_credits(GTK_ABOUT_DIALOG(about_dialog),translator);

	gtk_about_dialog_set_artists(GTK_ABOUT_DIALOG(about_dialog),(const gchar **)artists);
	gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about_dialog),pixbuf);
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about_dialog),
		gettext("This application is distributed under the IHA license ver 1.0.\n"
			"This application is the first IHA license software.\n"));

	gtk_window_set_transient_for((GtkWindow *)about_dialog,(GtkWindow *)window);

	gtk_dialog_run(GTK_DIALOG(about_dialog));
	
	gtk_widget_destroy(about_dialog);
}

static void cb_comarce (GtkWidget *button, gpointer user_data)
{
//	GError		*error=NULL;
	char		url[256];

	my_strncpy(url,"https://izumi.earth/jump.php",sizeof(url));
#if GTK_CHECK_VERSION(3,22,0)
//	gtk_show_uri_on_window((GtkWindow *)window,url,0,&error);
#else
//	gtk_show_uri((GdkScreen *)gtk_window_get_screen (GTK_WINDOW (window)),url,0,&error);
#endif
}


static void check_click()
{
	clicked_no++;

	if(clicked_no >= CLICK_NO){
		clicked_no = 0;

		cb_comarce(NULL,NULL);
	}
}

gboolean cb_state_event( GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
	if(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED){
		iconified = 1;
	}
	if((event->new_window_state & GDK_WINDOW_STATE_FOCUSED) && 
		!(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED)){
		iconified = 0;
	}

	return TRUE;
}
static	void	set_menu_size(void)
{
	gtk_widget_set_size_request(button_connect,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_disconnect,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_respond,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_set,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_rec,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_hot,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_key,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_cap,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_lic,110*pow(mag,zoom_cnt),MENU_HEIGHT);
	gtk_widget_set_size_request(button_exit,110*pow(mag,zoom_cnt),MENU_HEIGHT);
}

gboolean cb_key_press( GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	int	x,y;
//	g_print("keyval=%d state=%d string=%s\n", event->keyval, event->state, event->string);

	if(recflg || PinPmode || (window == NULL) ){
		return FALSE;
	}

	if((strcmp(event->string,"f") == 0) || (strcmp(event->string,"F") == 0 )){
		if(FullFlg){
			return FALSE;
		}
//		my_event_flg = 1;

		GetResolution(&x,&y);

		gtk_widget_set_size_request(GTK_WIDGET(window),x,y);
		gtk_widget_set_size_request(GTK_WIDGET(drawingarea),x,y - MENU_HEIGHT);

		gtk_window_set_decorated(GTK_WINDOW(window),FALSE);

		FullFlg = 1;
	}
	if((strcmp(event->string,"e") == 0) || (strcmp(event->string,"E") == 0 )){
		if(!FullFlg){
			return FALSE;
		}
//		my_event_flg = 1;

		gtk_widget_set_size_request(window,width,height + MENU_HEIGHT);
		gtk_widget_set_size_request(drawingarea,width,height);

		gtk_window_move(GTK_WINDOW(window),10,10);

		gtk_window_set_decorated(GTK_WINDOW(window),TRUE);

		GetResolution(&x,&y);
		cairo_scale(gcr, (double)width/(double)x , (double)height/(double)(y)*(double)1.0/(double)0.97);

		zoom_cnt = 0.0;
		set_menu_size();
		gtk_widget_show_all(window);

		FullFlg = 0;
	}

	if(((strcmp(event->string,"b") == 0) || (strcmp(event->string,"B") == 0 )) && (FullFlg == 0)){
		if((-3.0 <= zoom_cnt) && (zoom_cnt <= 4.0)){
//			my_event_flg = 1;
			zoom_cnt += 1.0;
			gtk_widget_set_size_request(GTK_WIDGET(drawingarea),width*pow(mag,zoom_cnt),height*pow(mag,zoom_cnt));
			set_menu_size();
		}
	}
	if(((strcmp(event->string,"s") == 0) || (strcmp(event->string,"S") == 0 )) && (FullFlg == 0)){
		if((-2.0 <= zoom_cnt) && (zoom_cnt <= 5.0)){
//			my_event_flg = 1;
			zoom_cnt -= 1.0;
			gtk_widget_set_size_request(GTK_WIDGET(drawingarea),width*pow(mag,zoom_cnt),height*pow(mag,zoom_cnt));
			gtk_widget_set_size_request(GTK_WIDGET(window),width*pow(mag,zoom_cnt),height*pow(mag,zoom_cnt)+15);
			set_menu_size();
		}
	}

	chgflg = 1;

	return FALSE;
}
gboolean cb_draw( GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
//	printf("cb_draw run!! set chgflg!!\n");

	chgflg = 1;

////	g_signal_emit_by_name(G_OBJECT(window ), "Refresh",NULL);

//	gcr = cr;
//	Refresh(NULL);
//	cairo_paint(cr);

	return FALSE;
}
void	cb_realize_dummy( GtkWidget *widget, gpointer user_data)
{
//	printf("signal realize !!\n");
	return;
}
static	int	cb_size_allocate_called = 0;
void	cb_size_allocate( GtkWidget *widget, GdkRectangle *allocation, gpointer user_data)
{
	RECVLIST        *list;
	int		i;
	int		x,y;

//	printf("cb_size_allocate run!!\n");

	if(window == NULL){
		return;
	}

//	if(my_event_flg == 0){
//		printf("cb_size_allocate called from other thread\n");
//		return;
//	}

	if(cb_size_allocate_called == 0){	// 最初の呼び出しは無視する
		cb_size_allocate_called = 1;
		return;
	}

	gdk_window_end_draw_frame(gtk_widget_get_window(drawingarea),dc);
	cairo_region_destroy(clip);
	gtk_widget_show_all(window);

	clip = cairo_region_create();
	if(clip == NULL){
//		my_event_flg = 0;
		fprintf(stderr,"cairo_region_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	dc = gdk_window_begin_draw_frame(gtk_widget_get_window(drawingarea), clip);
	if(dc == NULL){
//		my_event_flg = 0;
		fprintf(stderr,"gdk_window_begin_draw_frame error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	gcr = gdk_drawing_context_get_cairo_context(dc);
	if(gcr == NULL){
//		my_event_flg = 0;
		fprintf(stderr,"gdk_drawing_context_get_cairo_context error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}


	if(cur_sess_no  <= 2){
		cairo_scale(gcr, (double)1.0/(double)1.0*pow(mag,zoom_cnt) , (double)1.0/(double)1.0*pow(mag,zoom_cnt) );
	}else if((2 < cur_sess_no ) && (cur_sess_no <= 4) ){
		cairo_scale(gcr, (double)1.0/(double)2.0*pow(mag,zoom_cnt) , (double)1.0/(double)2.0*pow(mag,zoom_cnt) );
	}else if((4 < cur_sess_no ) && (cur_sess_no <= 9) ){
		cairo_scale(gcr, (double)1.0/(double)3.0*pow(mag,zoom_cnt) , (double)1.0/(double)3.0*pow(mag,zoom_cnt) );
	}else if((9 < cur_sess_no ) && (cur_sess_no <= 16) ){
		cairo_scale(gcr, (double)1.0/(double)4.0*pow(mag,zoom_cnt) , (double)1.0/(double)4.0*pow(mag,zoom_cnt) );
	}else if((16 < cur_sess_no ) && (cur_sess_no <= 25) ){
		cairo_scale(gcr, (double)1.0/(double)5.0*pow(mag,zoom_cnt) , (double)1.0/(double)5.0*pow(mag,zoom_cnt) );
	}else if((25 < cur_sess_no ) && (cur_sess_no <= 36) ){
		cairo_scale(gcr, (double)1.0/(double)6.0*pow(mag,zoom_cnt) , (double)1.0/(double)6.0*pow(mag,zoom_cnt) );
	}else if((36 < cur_sess_no ) && (cur_sess_no <= 49) ){
		cairo_scale(gcr, (double)1.0/(double)7.0*pow(mag,zoom_cnt) , (double)1.0/(double)7.0*pow(mag,zoom_cnt) );
	}


	if(FullFlg){
		GetResolution(&x,&y);
		cairo_scale(gcr, (double)x/(double)width , (double)(y)/(double)height*(double)0.97);
		gtk_window_move(GTK_WINDOW(window),0,0);

		cairo_scale(gcr, 1.0/pow(mag,zoom_cnt), 1.0/pow(mag,zoom_cnt));

		zoom_cnt = 0.0;
	}


	pthread_mutex_lock(&m_recv_list);

	if(cur_sess_no == 2){
		list = recv_list_top;
		for(i=0;i<2;i++){
			if(list){
				list->startx = ent_table2[list->disp_no].startx;
				list->starty = ent_table2[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if(cur_sess_no == 3){
		list = recv_list_top;
		for(i=0;i<3;i++){
			if(list){
				list->startx = ent_table3[list->disp_no].startx;
				list->starty = ent_table3[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if(cur_sess_no == 4){
		list = recv_list_top;
		for(i=0;i<4;i++){
			if(list){
				list->startx = ent_table4[list->disp_no].startx;
				list->starty = ent_table4[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if((4 < cur_sess_no ) && (cur_sess_no < 10) ){
		list = recv_list_top;
		for(i=0;i<9;i++){
			if(list){
				list->startx = ent_table9[list->disp_no].startx;
				list->starty = ent_table9[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if((9 < cur_sess_no ) && (cur_sess_no < 17) ){
		list = recv_list_top;
		for(i=0;i<16;i++){
			if(list){
				list->startx = ent_table16[list->disp_no].startx;
				list->starty = ent_table16[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if((16 < cur_sess_no ) && (cur_sess_no < 26) ){
		list = recv_list_top;
		for(i=0;i<25;i++){
			if(list){
				list->startx = ent_table25[list->disp_no].startx;
				list->starty = ent_table25[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if((25 < cur_sess_no ) && (cur_sess_no < 37) ){
		list = recv_list_top;
		for(i=0;i<36;i++){
			if(list){
				list->startx = ent_table36[list->disp_no].startx;
				list->starty = ent_table36[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}else if((36 < cur_sess_no ) && (cur_sess_no < 50) ){
		list = recv_list_top;
		for(i=0;i<49;i++){
			if(list){
				list->startx = ent_table49[list->disp_no].startx;
				list->starty = ent_table49[list->disp_no].starty;
				list = list->next;
			}else{
				break;
			}
		}
	}

	pthread_mutex_unlock(&m_recv_list);

//	my_event_flg = 0;
	chgflg = 1;
	return;
}

gboolean UpdateBannar()
{
	GtkWidget *button_image = gtk_image_new_from_file("./bannar.jpg");

	if(button_image != NULL){
//// NG memory leak		g_object_ref(button_image);
		gtk_button_set_image(GTK_BUTTON(button_com),button_image);
		gtk_widget_show(button_com);
	}else{
		fprintf(stderr,"button_image is NULL.(file=%s line=%d)\n",__FILE__,__LINE__);
	}

        return FALSE;                           // importtant
}
static	int	togle_cnt = 0;
static	int	rec_button_provider = 0;
gboolean UpdateButton()
{
//	if(FullFlg){
//		return FALSE;
//	}

	if((recflg == 0) && (rec_button_provider == 0)){
		return FALSE;
	}

	pthread_mutex_lock(&m_rec_button);

	if(rec_button_provider == 0){
		gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
			GTK_STYLE_PROVIDER (provider));
	}else{
		gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
			GTK_STYLE_PROVIDER (provider2));
	}

//	gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
//		GTK_STYLE_PROVIDER (provider));
//	gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
//		GTK_STYLE_PROVIDER (provider2));


	togle_cnt++;
	if(recflg){
		
//pthread_mutex_lock(&m_recv_list);
//gtk_window_set_decorated(GTK_WINDOW(window),FALSE);
//pthread_mutex_unlock(&m_recv_list);

		if(togle_cnt % 2 == 0){
//			gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
//				GTK_STYLE_PROVIDER (provider));

			gtk_style_context_add_provider(gtk_widget_get_style_context(button_rec), 
				GTK_STYLE_PROVIDER (provider), G_MAXUINT);
//				GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
//				GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
			rec_button_provider = 0;
		}else{
//			gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
//				GTK_STYLE_PROVIDER (provider2));

			gtk_style_context_add_provider(gtk_widget_get_style_context(button_rec), 
				GTK_STYLE_PROVIDER (provider2), G_MAXUINT);
//				GTK_STYLE_PROVIDER (provider2), GTK_STYLE_PROVIDER_PRIORITY_USER);
			rec_button_provider = 1;
		}
	}else{
//		gtk_style_context_remove_provider(gtk_widget_get_style_context(button_rec), 
//			GTK_STYLE_PROVIDER (provider));

		gtk_style_context_add_provider(gtk_widget_get_style_context(button_rec), 
			GTK_STYLE_PROVIDER (provider), G_MAXUINT);
//			GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
		rec_button_provider = 0;
	}
	gtk_widget_show(button_rec);

	pthread_mutex_unlock(&m_rec_button);

        return FALSE;                           // importtant
}

gboolean DeleteWdialog()
{
	if(Wdialog){
		gtk_widget_destroy(Wdialog);
		Wdialog = NULL;
	}
	return FALSE;			// importtan
}
static int SessDialog_flg=0;
gboolean AcceptDialog()
{
	unsigned char	buf[3];
	char		tmpmessage[256];
	char		SaveIp[64];
	gint		response;
        GtkWidget       *label1,*label2,*dialog_entry1,*dialog_entry2;
        const           gchar *entry_text1=NULL,*entry_text2=NULL;
	char		*name;


	if(SessDialog_delete){
		SessDialog_delete = 0;
		SessDialog_flg = 0;
//printf("**************************************** AcceptDialog gtk_widget_destroy Sdialog\n");
		gtk_widget_destroy(Sdialog);
		Sdialog = NULL;
		return FALSE;			// importtan
	}

	if(SessDialog_flg){
		return FALSE;			// importtan
	}
	SessDialog_flg = 1;

	if(SessionIp[0] == 0){
		return FALSE;			// importtan
	}

	if(IsReject(SessionIp)){
		return FALSE;			// importtan
	}

	my_strncpy(SaveIp,SessionIp,sizeof(SaveIp));

	name = name_from_ip(SessionIp);

	if(name){
		sprintf(tmpmessage,gettext("Accept for key share from %s(%s)"),SessionIp,name);
	}else{

		if(key_entry_no >= max_key_list_no){
       			Sdialog = gtk_dialog_new_with_buttons(gettext("Warning"), GTK_WINDOW(window),
               			        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),
                      				  NULL, NULL, NULL);
			label2 = gtk_label_new(gettext("\nWarning : Address book is full. Can not register.\n"));
			gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label2);
			gtk_widget_show_all(Sdialog);
			response = gtk_dialog_run(GTK_DIALOG(Sdialog));

			if((response != -1) && (Sdialog != NULL)){
				gtk_widget_destroy(Sdialog);
				Sdialog = NULL;
			}
		
			buf[0] = PRO_SEND_SESSION;
			buf[1] = CMD_RESP_REQ;
			buf[2] = 0x01;
			send_sess(1000,buf,3,SaveIp,PORT_NUM);
			St = STATUS_IDLE;
			printf_st("*** END_SESSION ***\n");

			SessDialog_flg = 0;
			return FALSE;		// importtan
		}

		sprintf(tmpmessage,gettext("Accept for key share from %s"),SessionIp);
	}

        Sdialog = gtk_dialog_new_with_buttons(gettext("Key Share"), GTK_WINDOW(window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			 gettext("YES"),GTK_RESPONSE_YES, gettext("NO") , GTK_RESPONSE_NO,
			gettext("REJECT"),GTK_RESPONSE_CANCEL , gettext("REJECT ALL"),GTK_RESPONSE_REJECT,NULL);
	
        label1 = gtk_label_new(tmpmessage);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label1);


	gtk_widget_show_all(Sdialog);
	response = gtk_dialog_run(GTK_DIALOG(Sdialog));
	
	if((response != -1) && (Sdialog != NULL)){
		gtk_widget_destroy(Sdialog);
		Sdialog = NULL;
	}


	if(response == GTK_RESPONSE_YES){

retry:
		entry_text1=NULL;
		entry_text2=NULL;

        	Sdialog = gtk_dialog_new_with_buttons(gettext("Key Share"), GTK_WINDOW(window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "YES",
                        GTK_RESPONSE_YES, NULL);

        	label2 = gtk_label_new(gettext("Authentication code"));
        	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label2);
        	dialog_entry2 = gtk_entry_new ();
        	gtk_entry_set_max_length (GTK_ENTRY (dialog_entry2), 32);
        	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,dialog_entry2);

		if(name == NULL){
			label1 = gtk_label_new(gettext("Name for partner"));
			gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label1);
			dialog_entry1 = gtk_entry_new ();
			gtk_entry_set_max_length (GTK_ENTRY (dialog_entry1), MAX_KEY_NAME_LEN);
			gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,dialog_entry1);
		}


		gtk_widget_show_all(Sdialog);
		response = gtk_dialog_run(GTK_DIALOG(Sdialog));


		if((response != -1) && (Sdialog != NULL)){
			if(name == NULL){
				entry_text1 = gtk_entry_get_text (GTK_ENTRY (dialog_entry1));
				
				if(strlen(entry_text1) > MAX_KEY_NAME_LEN){
					gtk_widget_destroy(Sdialog);
					Sdialog = NULL;

					Sdialog = gtk_dialog_new_with_buttons(gettext("Warning"), GTK_WINDOW(window),
               			        		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),NULL, NULL, NULL);
					label2 = gtk_label_new(gettext("\nWarning : Name is too long.\n"));
					gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label2);
					gtk_widget_show_all(Sdialog);
					response = gtk_dialog_run(GTK_DIALOG(Sdialog));

					if((response != -1) && (Sdialog != NULL)){
						gtk_widget_destroy(Sdialog);
						Sdialog = NULL;
					}
					if(response == -1){
						SessDialog_flg = 0;
						return FALSE;		// importtan
					}	
					goto retry;
				}
			}
			entry_text2 = gtk_entry_get_text (GTK_ENTRY (dialog_entry2));

			if(strlen(entry_text2) > 32){
				gtk_widget_destroy(Sdialog);
				Sdialog = NULL;
       			
				Sdialog = gtk_dialog_new_with_buttons(gettext("Warning"), GTK_WINDOW(window),
               			        	GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),NULL, NULL, NULL);
				label2 = gtk_label_new(gettext("\nWarning : Authentication code is too long.\n"));
				gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label2);
				gtk_widget_show_all(Sdialog);
				response = gtk_dialog_run(GTK_DIALOG(Sdialog));

				if((response != -1) && (Sdialog != NULL)){
					gtk_widget_destroy(Sdialog);
					Sdialog = NULL;
				}
				if(response == -1){
					SessDialog_flg = 0;
					return FALSE;		// importtan
				}	
		
				goto retry;
			}
// Ipが無い時のnameの重複は不可
			if(name == NULL){
				if(entry_text1[0] == 0x00){
					gtk_widget_destroy(Sdialog);
					Sdialog = NULL;
					goto retry;
				}
				if(list_from_name((char *)entry_text1)){
					gtk_widget_destroy(Sdialog);
					Sdialog = NULL;

					Sdialog = gtk_dialog_new_with_buttons(gettext("Warning"), GTK_WINDOW(window),
               			        		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),NULL, NULL, NULL);
					label2 = gtk_label_new(gettext("\nWarning : This name is registered.\n"));
					gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Sdialog))) ,label2);
					gtk_widget_show_all(Sdialog);
					response = gtk_dialog_run(GTK_DIALOG(Sdialog));

					if((response != -1) && (Sdialog != NULL)){
						gtk_widget_destroy(Sdialog);
						Sdialog = NULL;
					}
					if(response == -1){
						SessDialog_flg = 0;
						return FALSE;		// importtan
					}	
		
					goto retry;
				}
			}
			if(entry_text2[0] == 0x00){
				gtk_widget_destroy(Sdialog);
				Sdialog = NULL;
				goto retry;
			}

			printf_st("event ACCEPT\n");
			buf[0] = PRO_SEND_SESSION;
			buf[1] = CMD_ACCEPT;
			if(name == NULL){
				my_strncpy(PartnerName,entry_text1,sizeof(PartnerName));
			}else{
				my_strncpy(PartnerName,name,sizeof(PartnerName));
			}
			memset(MessAuthCode,0x00,32);
			my_strncpy(MessAuthCode,entry_text2,32);
			msgsendque(buf,my_ip,2);

			gtk_widget_destroy(Sdialog);
			Sdialog = NULL;

	        	wait_dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL,NULL,NULL);

			gtk_window_set_decorated(GTK_WINDOW(wait_dialog),FALSE);

			label1 = gtk_label_new(gettext("\nWait for while"));
			gtk_container_add(
			GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(wait_dialog))) ,label1);
			gtk_widget_show_all(wait_dialog);
			gtk_dialog_run(GTK_DIALOG(wait_dialog));
		}
	}else if(response == GTK_RESPONSE_NO){
		buf[0] = PRO_SEND_SESSION;
		buf[1] = CMD_RESP_REQ;
		buf[2] = 0x01;
		send_sess(1000,buf,3,SaveIp,PORT_NUM);
		St = STATUS_IDLE;
		printf_st("*** END_SESSION ***\n");
	}else if(response == GTK_RESPONSE_CANCEL){
// IP アドレスの登録
		reg_reject_list(SaveIp);
		St = STATUS_IDLE;
		printf_st("*** END_SESSION ***\n");
	}else if(response == GTK_RESPONSE_REJECT){
		Ignoreflg = TRUE;
		St = STATUS_IDLE;
		printf_st("*** END_SESSION ***\n");
	}else{
		;				// タイマーで削除された場合はここ！！
	}

	SessDialog_flg = 0;

	return FALSE;                           // importtan
}


static	void *sess_thread(void *pParam)
{
        int msqid;
        key_t msgkey;
        struct msgbuf{
                long mtype;
                char mdata[1500+64+sizeof(int)];
        };
        struct msgbuf msgdata,*p;

        p = &msgdata;
        msgkey = ftok(".",'X');
        msqid = msgget(msgkey,IPC_CREAT|0666);


	while(1){
		if(exitflg){
			msgctl(msqid, IPC_RMID, 0);
			return 0;
		}

		if(msgrcv(msqid,p,sizeof(p->mdata),0,IPC_NOWAIT)<=0) {
                        usleep(1000);
                        continue;
                }
		do_session((unsigned char *)(&(p->mdata[64+sizeof(int)])),(char *)(p->mdata),*(int *)(&(p->mdata[64])));
	}
}

static	void *ring_thread(void *pParam)
{
	while(1){
		usleep(1000*500);

		if(exitflg){
			return 0;
		}

		if((outouflg == 0) && (HassinNo == 0) && (cur_sess_no >= 1)){
			play_wave("./ringtone1.wav");	// 着信音
		}
		if((cur_sess_no == 0) && (HassinNo > 0)){
			play_wave("./ringtone3.wav");	// 発信音
		}
////			play_wave("./ringtone2.wav");	// 切断音
	}
}
static	void *timer_thread(void *pParam)
{
	KEYLIST		*list3;
	unsigned char	cmd[3];

	while(1){
		usleep(1000*500);

		if(exitflg){
			return 0;
		}

		if(TimerValue){
			TimerValue--;
			if(TimerValue == 1){
				cmd[0] = PRO_SEND_SESSION;
				cmd[1] = CMD_TIMEOUT;
				msgsendque(cmd,my_ip,2);
			}
		}

		list3 = key_list_top;
		while(list3){
			if(list3->DenyCnt > 1){
				list3->DenyCnt--;
			}else if(list3->DenyCnt == 1){
				list3->Kind = list3->SaveKind;
				list3->DenyCnt = 0;
			}

			if(list3->CallCnt > 1){
				list3->CallCnt--;
			}else if(list3->CallCnt == 1){
				if(list3->Kind == KIND_CALLING){
					del_send_list(list3->Ip);
					if(HassinNo >= 1){
						HassinNo--;
					}
					if(strcmp(list3->Name,"specify ip") == 0){
						strcpy(list3->Ip,"*");
					}
				}
				list3->Kind = list3->SaveKind;
				list3->CallCnt = 0;
			}

			list3 = list3->next;
		}

		g_signal_emit_by_name(G_OBJECT(window ), "UpdateButton",NULL); // Rec buttonの点滅要求
	}
}
static	int	utf8bytes(char *str)
{
	if((*str & 0x80) == 0x00){
		return 1;
	}else if((*str & 0xe0) == 0xc0){
		return 2;
	}else if((*str & 0xf0) == 0xe0){
		return 3;
	}else if((*str & 0xf8) == 0xf0){
		return 4;
	}
	return 0;
}
static	void	cut_utf8_string(char *str)
{
	float	disp_no;
	int	bytes;
	char	*p;

	disp_no = 0.0;
	p = str;

	while(*p){
		bytes = utf8bytes(p);
		if(bytes <= 1){
			disp_no += 1.0;
		}else if(bytes >= 2 ){
			disp_no += 1.7;
		}
		if(disp_no > 30.0){
			*p = 0x00;
			return;
		}
		p += bytes;
	}
}
gboolean Refresh(gpointer data)
{
	RECVLIST	*list,*list2;
	cairo_surface_t	*save_surf;
	int		hei,i;
	unsigned char	hash[32];
	int		scaleflg;
	char		buf[256];
	

	pthread_mutex_lock(&m_recv_list);

//	if(my_event_flg){
//		pthread_mutex_unlock(&m_recv_list);
//		return FALSE;
//	}

top:
	list = recv_list_top;
	if(list == NULL){
		usleep(1000*100);
		goto end;
	}

	while(list){
		if(list->DispMark){
			list->DispMark = 0;			// 自分のマークを外す
			if(list->next){
				list->next->DispMark = 1;	// 次のマークを設定
			}else{
				recv_list_top->DispMark = 1;
			}
			break;
		}else{
			list = list->next;
		}
	}

	if(list == NULL){
		list = recv_list_top;				// マークがない場合は先頭から
		if(list->next){
			list->next->DispMark = 1;		// 次のマークを設定
		}else{
			recv_list_top->DispMark = 1;
		}
	}
//printf("disp_no=%d ImageRecv=%d\n",list->disp_no,list->ImageRecv);
/*	if(list->ImageRecv == 0){
printf("ImageRecv == 0\n");
		pthread_mutex_unlock(&m_recv_list);
		return FALSE;
	}
	if(list->ImageRecv == 2){
printf("ImageRecv == 2\n");
		pthread_mutex_unlock(&m_recv_list);
		return FALSE;
	}
*/
	if((PinPmode != 0) && (list->background == 0) && ((cur_sess_no >= 12) || (PinPDispMode != DISP_IMAGE))){
								// バックグラウンドの描画回数をかせぐため！！
		goto top;					// PinPmodeではバックグラウンド以外は以降の処理を行わない
								// フォアグラウンドの処理はmake_cairo_surfaceも飛ばす
	}
	if(list->ImageRecv == 1){
		if(list->total == 0){
			goto skip;
		}
		if(list->total < MIN_JPEG_SIZE){
			p_message("Warning : total size is too small from ",name_from_ip(list->Ip));
			list->total = 0;
			list->bufp = list->image_buf;
			list->ImageRecv = 2;
			goto end;
		}

		sha_cal(256,(unsigned char *)list->image_buf,list->total - 32,hash);

		if(memcmp(hash,list->image_buf + list->total -32,32) != 0){
////			p_message("Warning : Image data is lost from ",name_from_ip(list->Ip));
			list->total = 0;
			list->bufp = list->image_buf;
			list->ImageRecv = 2;
			goto end;
		}

		save_surf = list->surf;
		list->surf = make_cairo_surface(list,list->image_buf, list->total - 32,0);
		if(list->surf == NULL){
			fprintf(stderr,"make_cairo_surface error!! name=%s.(file=%s line=%d)\n",name_from_ip(list->Ip),__FILE__,__LINE__);
			list->total = 0;
			list->bufp = list->image_buf;
			list->surf = save_surf;
			list->ImageRecv = 2;
			goto end;
		}

		list->total = 0;
		list->bufp = list->image_buf;

		if(cairo_surface_status(list->surf) != CAIRO_STATUS_SUCCESS){
			fprintf(stderr,"cairo_surface_status error %s.(file=%s line=%d)",
				cairo_status_to_string(cairo_surface_status(list->surf)),__FILE__,__LINE__); 
			list->total = 0;
			list->bufp = list->image_buf;
			list->surf = save_surf;
			list->ImageRecv = 2;
			goto end;
		}

////		if(save_surf){
////			cairo_surface_destroy(save_surf);
////		}
	}
skip:
	if((PinPmode != 0) && (list->background == 0)){		// バックグラウンドの描画回数をかせぐため！！
		goto top;					// PinPmodeではバックグラウンド以外は以降の処理を行わない
								// フォアグラウンドの処理はmake_cairo_surfaceだけ
	}

	if(list->ImageRecv == 1){
		list->ImageRecv = 2;
	}

	if(list->SurfWritedCounter >= 1){
		goto end;
	}
//
// バックグラウンドの表示
//
	scaleflg = 0;

//printf("surf=%p ImageRecv=%d\n",list->surf,list->ImageRecv);
	if(PinPmode == 0){
		if(list->surf != NULL){
			if(list->ImageRecv == 100){
				if(cur_sess_no == 2){
					scaleflg = 1;
					cairo_scale(gcr,(double)1.0/(double)2.0,(double)1.0);
					cairo_set_source_surface (gcr, surf4, list->startx*2, list->starty);
				}else{
					cairo_set_source_surface (gcr, surf2, list->startx, list->starty);
				}
			}else{
				if(cur_sess_no == 2){
					scaleflg = 1;
					cairo_scale(gcr,(double)1.0/(double)2.0,(double)1.0);
					cairo_set_source_surface (gcr, list->surf, list->startx*2, list->starty);
				}else{
					cairo_set_source_surface (gcr, list->surf, list->startx, list->starty);
				}
			}
		}else{
			if(cur_sess_no == 2){
				scaleflg = 1;
				cairo_scale(gcr,(double)1.0/(double)2.0,(double)1.0);
				cairo_set_source_surface (gcr, surf4, list->startx*2, list->starty);
			}else{
				cairo_set_source_surface (gcr, surf2, list->startx, list->starty);
			}
		}
	}else if(PinPmode == 1){
		if((list->surf != NULL) ){
			if(list->ImageRecv == 100){
				cairo_set_source_surface (gcr, surf2, width/10,0);
			}else{
				cairo_set_source_surface (gcr, list->surf, width/10,0);
			}
		}else{
			cairo_set_source_surface (gcr, surf2, width/10, 0);
		}
	}else if(PinPmode == 2){
		if((list->surf != NULL) ){
			if(list->ImageRecv == 100){
				cairo_set_source_surface (gcr, surf2, 0,0);
			}else{
				cairo_set_source_surface (gcr, list->surf, 0,0);
			}
		}else{
			cairo_set_source_surface (gcr, surf2, 0, 0);
		}
	}

	cairo_paint(gcr);
	draws++;
	list->TotalSurfWritedCounter++;
	list->SurfWritedCounter++;

	if((PinPmode == 0) && (cur_sess_no > 2)){
		if(list->tid == -1){
			cairo_set_source_rgb(gcr, 1.0, 0.0, 0.0);
		}else{
			cairo_set_source_rgb( gcr, 0, 1, 0 );
		}
		cairo_rectangle(gcr,list->startx, list->starty,width, 5);			// 上
		cairo_rectangle(gcr,list->startx, list->starty + height -5,width,5);		// 下

		cairo_rectangle(gcr,list->startx, list->starty, 5,height);			// 左
		cairo_rectangle(gcr,list->startx + width -5, list->starty-5 ,5,height);		// 右
		cairo_fill(gcr);
	}

	if(scaleflg){
		cairo_scale(gcr,(double)2.0,(double)1.0);
		scaleflg = 0;
	}
//
// 名前の表示
//
	if(cur_sess_no >= 2){
		cairo_set_source_rgb(gcr, 0.0, 0.5, 0.0);

		if(cur_sess_no == 2){
			cairo_set_font_size(gcr, 50);
		}else{
			cairo_set_font_size(gcr, 70);
		}
		if(list->background == 1){
			if(PinPmode == 1){
				cairo_move_to(gcr, width/10 + 20, (height - 20));
			}else if(PinPmode == 2){
				cairo_move_to(gcr, 20, (height - 20));
			}
		}else{
			cairo_move_to(gcr, list->startx + 20, list->starty + (height - 20));
		}

		if(list->ch == 1){
			sprintf(buf,"%s%s",name_from_ip(&(list->Ip[2])),gettext("(capture)"));
		}else{
			sprintf(buf,"%s",name_from_ip(list->Ip));
		}

		if(list->tid == -1){
			cairo_set_source_rgb(gcr, 1.0, 0.0, 0.0);
		}

		cut_utf8_string(buf);

		cairo_show_text(gcr,buf);
	}
//
// フォアグラウンドの表示
//
	if(PinPmode == 1){
		list2 = recv_list_top;
		cairo_scale(gcr,(double)0.1,(double)0.1);

		hei = 0;
		for(i=0;i < (MAX_CLI + 2);){
			if(list2){
				if(list2->background == 0){
					if((PinPDispMode == DISP_IMAGE) && (cur_sess_no <= 11)){
						if((list2->surf == NULL) || (list2->ImageRecv == 100)){
//						if(list2->surf == NULL){
							if(i <= 11){
								cairo_set_source_surface (gcr, surf2,0,height*hei );
								cairo_paint(gcr);
								draws++;
							}
						}else{
							if(i <= 11){
								cairo_set_source_surface (gcr, list2->surf,0,height*hei );
								cairo_paint(gcr);
								draws++;
							}
						}

//						cairo_set_source_rgb(gcr, 0.0, 1.0, 0.0);

						if(list2->tid == -1){
							cairo_set_source_rgb(gcr, 1.0, 0.0, 0.0);
						}else{
							cairo_set_source_rgb( gcr, 0.0, 1.0, 0.0 );
						}

						cairo_set_font_size(gcr, width/10);

						cairo_move_to(gcr, 20, (height*(hei+1) - 20));
						if(list2->ch == 1){
							cairo_show_text(gcr, name_from_ip(&(list2->Ip[2])));
							cairo_show_text(gcr,"(cap)");
						}else{
							cairo_show_text(gcr, name_from_ip(list2->Ip));
						}

						if(list2->ImageRecv == 1){
							list2->ImageRecv = 2;	// フォアグラウンドの描画を進行させるため
						}

						i++;
					}else{
						cairo_set_source_rgb(gcr, 0.0, 0.0, 1.0);
						cairo_set_line_width(gcr, 1);
						cairo_rectangle(gcr, 0 , (height/5)*hei, width,height/5);
						cairo_stroke_preserve(gcr);
						cairo_fill(gcr);

//						cairo_set_source_rgb(gcr, 1.0, 1.0, 1.0);

						if(list2->tid == -1){
							cairo_set_source_rgb(gcr, 1.0, 0.0, 0.0);
						}else{
							cairo_set_source_rgb( gcr, 1.0, 1.0, 1.0 );
						}

						cairo_set_font_size(gcr, width/10);
						cairo_move_to(gcr, 0, (height/5)*(hei+1));
						if(list2->ch == 1){
							cairo_show_text(gcr, name_from_ip(&(list2->Ip[2])));
							cairo_show_text(gcr,"(cap)");
						}else{
							cairo_show_text(gcr, name_from_ip(list2->Ip));
						}
					}
					hei++;
				}
				list2 = list2->next;
			}else{
				if((PinPDispMode == DISP_IMAGE) && (cur_sess_no <= 11)){
					if(i <= 11){
						cairo_set_source_surface (gcr, surf,0,height*hei );
						cairo_paint(gcr);
						draws++;
					}
				}else{
					cairo_set_source_rgb(gcr, 0.0, 0.0, 1.0);
					cairo_set_line_width(gcr, 1);
					cairo_rectangle(gcr, 0 , (height/5)*hei, width,height/5);
					cairo_stroke_preserve(gcr);
					cairo_fill(gcr);
				}
				i++;
				hei++;
			}
		}
		cairo_scale(gcr,(double)10.0,(double)10.0);
	}
end:
	if((chgflg == 1) && (PinPmode == 0)){
//printf("befor set_init_image_all\n");
		set_init_image_all();
		chgflg = 0;
	}

	gdk_display_flush(gdk_display_get_default());

	pthread_mutex_unlock(&m_recv_list);
	return FALSE;
}
//static	int	CNt=0;
static	void	Refresh_notify(void)
{
//	printf("********** Refresh_notify(%d) *********\n",CNt++);

	Refresh_reg_flg = 0;
}
static	void *ref_thread(void *pParam)
{
//	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,(GSourceFunc)Refresh,NULL, (GDestroyNotify)Refresh_notify);
//	メインスレッドのCPU使用率が１００％！！
//	g_idle_add_full(G_PRIORITY_HIGH_IDLE + 20,(GSourceFunc)Refresh,NULL, (GDestroyNotify)Refresh_notify);

//NG	g_idle_add_full(G_PRIORITY_HIGH_IDLE,(GSourceFunc)Refresh,NULL, (GDestroyNotify)Refresh_notify);
//NG	g_idle_add_full(G_PRIORITY_DEFAULT, (int(*)(void *))Refresh,NULL, NULL);
//NG	g_idle_add_full(G_PRIORITY_HIGH_IDLE, (int(*)(void *))Refresh,NULL, NULL);

	while(1){
		if(cur_sess_no == 0){
			usleep(1000*100);
		}else{
			usleep(1000*5/cur_sess_no);
		}
		if(exitflg){
			return 0;
		}

//		pthread_mutex_lock(&m_recv_list);	// Refreshが走りきらないようにロックをかける！！
		if(Refresh_reg_flg == 0){

//			g_signal_emit_by_name(G_OBJECT(window ), "Refresh",NULL);
        		
			Refresh_reg_flg = 1;		// 登録する前に設定しないとロックが必要になる！！

			g_idle_add_full(G_PRIORITY_HIGH_IDLE + 20,(GSourceFunc)Refresh,NULL, (GDestroyNotify)Refresh_notify);
		}
//		pthread_mutex_unlock(&m_recv_list);

//NGNGNG	Refresh(NULL);

	}
}



static	int 		MessageDialog_flg=0;

gboolean MessageDialog()
{
        GtkWidget       *label1;
	GtkWidget       *Mdialog=NULL;
	gint		response;

	if(MessageDialog_flg){
		if(Mdialog){
			gtk_widget_destroy(Mdialog);
			Mdialog = NULL;
		}
		return FALSE;			// importtan
	}
	MessageDialog_flg = 1;


	if(wait_dialog){
		gtk_widget_destroy(wait_dialog);
		wait_dialog = NULL;
	}


        Mdialog = gtk_dialog_new_with_buttons(gettext(" "), GTK_WINDOW(window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),
                        GTK_RESPONSE_OK, NULL);
	
        label1 = gtk_label_new(DialogMessage);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(Mdialog))) ,label1);

	gtk_widget_show_all(Mdialog);
	response = gtk_dialog_run(GTK_DIALOG(Mdialog));
	
	if(response != -1){
		gtk_widget_destroy(Mdialog);
		Mdialog = NULL;
	}
	MessageDialog_flg = 0;

	return FALSE;                           // importtan
}

void OnAcceptDialog()
{
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)AcceptDialog, NULL, NULL);
}
void OnMessageDialog()
{
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)MessageDialog, NULL, NULL);
}
void OnUpdateBannar()
{
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)UpdateBannar, NULL, NULL);
}
void OnUpdateButton()
{
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)UpdateButton, NULL, NULL);
}

void OnRefresh(GtkWidget* widget, gpointer data)
{
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)Refresh,data, NULL);
}
void OnDeleteWdialog()
{
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)DeleteWdialog, NULL, NULL);
}

/*
static	int	get_my_ip()
{
	FILE	*fp;
	char	*str,*stop,buf[128];

	fp = popen("hostname -I","r");
	if(fp == NULL){
		my_ip[0] = 0;
		return -1;
	}
	fread(buf,sizeof(buf)-1,1,fp);
	pclose(fp);

	str = strtok_r(buf," \n",&stop);
	if(str != NULL){
		if(IsIP(str)){
			my_strncpy(my_ip,str,sizeof(my_ip));		// 複数ある場合は無視される
		}else{
			my_ip[0] = 0;
		}
	}else{
		my_ip[0] = 0;
		return -1;
	}
	return 0;
}
*/
static void cb_key_share (GtkWidget *button, gpointer user_data)
{
        GtkWidget       *dialog;
	gint		response;
        GtkWidget       *label1,*label2,*label3;
        GtkWidget       *dialog_entry1,*dialog_entry2,*dialog_entry3;
        const           gchar *entry_text1,*entry_text2,*entry_text3;
	unsigned char	cmd[256];
	char		save_text1[256]={0},save_text2[256]={0},save_text3[256]={0};
	char		*name;

	check_click();

	if(Ignoreflg){
		return;
	}

        dialog = gtk_dialog_new_with_buttons(gettext("Warning"), GTK_WINDOW(window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),
                        GTK_RESPONSE_OK, gettext("CANCEL") , GTK_RESPONSE_CANCEL , NULL);
        label2 = gtk_label_new(gettext("\nWarning : There is a risk that this method will be decrypted by a quantum computer.\n\nThe safest way is to hand it in directly.\n"));
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,label2);
        gtk_widget_show_all(dialog);
        response = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	if(response == GTK_RESPONSE_OK){
		;
	}else{
		return;
	}

top:
        dialog = gtk_dialog_new_with_buttons(gettext("Key Share"), GTK_WINDOW(window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),
                        GTK_RESPONSE_OK, gettext("CANCEL") , GTK_RESPONSE_CANCEL , NULL);


        label1 = gtk_label_new(gettext("Authentication code"));
        dialog_entry1 = gtk_entry_new ();
        gtk_entry_set_max_length (GTK_ENTRY (dialog_entry1), 32);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,label1);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),dialog_entry1);

        label2 = gtk_label_new(gettext("Name for partner"));
        dialog_entry2 = gtk_entry_new ();
        gtk_entry_set_max_length (GTK_ENTRY (dialog_entry2), MAX_KEY_NAME_LEN);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,label2);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),dialog_entry2);

        label3 = gtk_label_new(gettext("IP address for partner"));
        dialog_entry3 = gtk_entry_new ();
        gtk_entry_set_max_length (GTK_ENTRY (dialog_entry3), 64);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,label3);
        gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),dialog_entry3);


	gtk_entry_set_text (GTK_ENTRY (dialog_entry1),save_text1);
	gtk_entry_set_text (GTK_ENTRY (dialog_entry2),save_text2);
	gtk_entry_set_text (GTK_ENTRY (dialog_entry3),save_text3);

        gtk_widget_show_all(dialog);
        response = gtk_dialog_run(GTK_DIALOG(dialog));

	if(response == GTK_RESPONSE_OK){
		entry_text1 = gtk_entry_get_text (GTK_ENTRY (dialog_entry1));
		entry_text2 = gtk_entry_get_text (GTK_ENTRY (dialog_entry2));
		entry_text3 = gtk_entry_get_text (GTK_ENTRY (dialog_entry3));

		if(strlen(entry_text1) > 32){
			my_strncpy(save_text1,entry_text1,sizeof(save_text1));
			my_strncpy(save_text2,entry_text2,sizeof(save_text2));
			my_strncpy(save_text3,entry_text3,sizeof(save_text3));
			gtk_widget_destroy(dialog);

			OKDialog(gettext("\nWarning : Authentication code is too long.\n"));
			goto top;
		}
		if(strlen(entry_text2) > MAX_KEY_NAME_LEN){
			my_strncpy(save_text1,entry_text1,sizeof(save_text1));
			my_strncpy(save_text2,entry_text2,sizeof(save_text2));
			my_strncpy(save_text3,entry_text3,sizeof(save_text3));
			gtk_widget_destroy(dialog);

			OKDialog(gettext("\nWarning : Name is too long.\n"));
			goto top;
		}
		if(strlen(entry_text3) > 64){
			my_strncpy(save_text1,entry_text1,sizeof(save_text1));
			my_strncpy(save_text2,entry_text2,sizeof(save_text2));
			my_strncpy(save_text3,entry_text3,sizeof(save_text3));
			gtk_widget_destroy(dialog);

			OKDialog(gettext("Incorrect IP address\n"));
			goto top;
		}
		if((entry_text1[0] == 0x00) || (entry_text2[0] == 0x00) || (entry_text3[0] == 0x00)){
			my_strncpy(save_text1,entry_text1,sizeof(save_text1));
			my_strncpy(save_text2,entry_text2,sizeof(save_text2));
			my_strncpy(save_text3,entry_text3,sizeof(save_text3));
			gtk_widget_destroy(dialog);
			goto top;
		}
		if(strstr(entry_text2,",") || (IsIP((char *)entry_text3) == 0) ||
			 (strcmp(entry_text3,my_ip) == 0)){
			my_strncpy(save_text1,entry_text1,sizeof(save_text1));
			my_strncpy(save_text2,entry_text2,sizeof(save_text2));
			my_strncpy(save_text3,entry_text3,sizeof(save_text3));
			gtk_widget_destroy(dialog);

			OKDialog(gettext("Incorrect IP address\n"));
			goto top;
		}
// Ipがないならnameの重複は不可。
// Ipがあるならnameと一致しなければ不可。
		if(list_from_ip((char *)entry_text3) == NULL){
			if(list_from_name((char *)entry_text2)){
				my_strncpy(save_text1,entry_text1,sizeof(save_text1));
				save_text2[0] = 0x00;
				my_strncpy(save_text3,entry_text3,sizeof(save_text3));
				gtk_widget_destroy(dialog);
				goto top;
			}

			if(key_entry_no >= max_key_list_no){
				my_strncpy(save_text1,entry_text1,sizeof(save_text1));
				my_strncpy(save_text2,entry_text2,sizeof(save_text2));
				my_strncpy(save_text3,entry_text3,sizeof(save_text3));
				gtk_widget_destroy(dialog);

				OKDialog(gettext("\nWarning : Address book is full. Can not register.\n"));
				goto top;
			}

		}else{
			name = (char *)name_from_ip((char *)entry_text3);
			if(name == NULL){
				my_strncpy(save_text1,entry_text1,sizeof(save_text1));
				save_text2[0] = 0x00;
				my_strncpy(save_text3,entry_text3,sizeof(save_text3));
				gtk_widget_destroy(dialog);
				goto top;
			}
			if(strcmp(name,(char *)entry_text2)){
				my_strncpy(save_text1,entry_text1,sizeof(save_text1));
				my_strncpy(save_text2,name,sizeof(save_text2));
				my_strncpy(save_text3,entry_text3,sizeof(save_text3));
				gtk_widget_destroy(dialog);
				goto top;
			}
		}
// セッションの開始を要求する。
		cmd[0] = PRO_SEND_SESSION;
		cmd[1] = CMD_START_SESSION;
		my_strncpy(SessionIp,entry_text3,sizeof(SessionIp));
		my_strncpy(PartnerName,entry_text2,sizeof(PartnerName));
		memset(MessAuthCode,0x00,32);
		my_strncpy(MessAuthCode,entry_text1,32);
		msgsendque(cmd,my_ip,2);
	}
	gtk_widget_destroy(dialog);
	save_text1[0] = 0x00;
	save_text2[0] = 0x00;
	save_text3[0] = 0x00;


	if(response == GTK_RESPONSE_OK){
	        wait_dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, NULL,NULL,NULL);

		gtk_window_set_decorated(GTK_WINDOW(wait_dialog),FALSE);

		label1 = gtk_label_new(gettext("\nWait for while"));
		gtk_container_add(
		GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(wait_dialog))) ,label1);
		gtk_widget_show_all(wait_dialog);
		gtk_dialog_run(GTK_DIALOG(wait_dialog));
	}

	return;
}
/*****************************************************************************************/
/* シグナルハンドラ*/
/*****************************************************************************************/
static void sig_handler(int sig, siginfo_t *si, void *unused)
{
	fprintf(stderr,"SIGNAL catch!! sig=%d errno=%d addr=%p band=%ld.(file=%s line=%d)\n",
			sig,si->si_errno,si->si_addr,si->si_band,__FILE__,__LINE__);
	abort();
	exit(0);
}

//
// マウスイベント処理
//
void cb_mouse(GtkWidget *window, GdkEventButton *button, GtkWidget *menu)
{
	int		x,y,x2,y2,no,i;
	RECVLIST	*list;
	time_t		timer;
	float		z;


	x = (int)floor(button->x);
	y = (int)floor(button->y);
//printf("x=%d y=%d\n",x,y);

	if(cur_sess_no <= 1){
		return;
	}
	timer = time(NULL);
	if(timer - g_timer < 1){		// 連打の禁止！！
		return;
	}
	g_timer = time(NULL);

	if(PinPmode == 1){
		PinPmode = 2;
		cairo_scale(gcr, (double)1.1, (double)1.0);
		return;
	}

	if(cur_sess_no == 2){
//
// 2画面表示の場合
//

		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);

			list = recv_list_top;
			for(i=0;i<2 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}
//			cairo_scale(gcr, (double)1.1, (double)1.0);

			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = pow(mag,zoom_cnt);
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = (double)x2/(double)width;
		}
// 画面番号を決める
		if(x <= (int)(float)ent_table2[1].startx * z ){
			no = 0;
		}else{
			no = 1;
		}
		if(ent_table2[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		PinPmode = 1;
		chgflg = 1;
		pthread_mutex_unlock(&m_recv_list);
	}else if(cur_sess_no == 3){
//
// 3画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<3 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)0.5, (double)0.5);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = pow(mag,zoom_cnt) * 0.5;
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = 0.5 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table3[1].startx * z ) &&
		   (y <= (int)(float)ent_table3[2].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table3[1].startx * z ) &&
			 (y <= (int)(float)ent_table3[2].starty * z )){
			no = 1;
		}else if((x >= (int)(float)ent_table3[2].startx * z ) &&
			 (x <= (int)(float)ent_table3[3].startx * z ) &&
			 (y >= (int)(float)ent_table3[2].starty * z )){
			no = 2;
		}else{
			return;
		}
		if(ent_table3[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)2.0, (double)2.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}else if(cur_sess_no == 4){
//
// ４画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<4 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)0.5, (double)0.5);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = pow(mag,zoom_cnt) * 0.5;
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = 0.5 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table4[1].startx * z ) &&
		   (y <= (int)(float)ent_table4[2].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table4[1].startx * z ) &&
			 (y <= (int)(float)ent_table4[2].starty * z )){
			no = 1;
		}else if((x <= (int)(float)ent_table4[1].startx * z ) &&
			 (y >= (int)(float)ent_table4[2].starty * z )){
			no = 2;
		}else if((x >= (int)(float)ent_table4[1].startx * z ) &&
			 (y >= (int)(float)ent_table4[2].starty * z )){
			no = 3;
		}else{
			return;
		}
		if(ent_table4[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)2.0, (double)2.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}else if((5 <= cur_sess_no) && (cur_sess_no <= 9)){
//
// ９画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<9 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)1.0/(double)3.0, (double)1.0/(double)3.0);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = pow(mag,zoom_cnt) * (double)1.0/(double)3.0;
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = 1.0/3.0 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table9[1].startx * z ) &&
		   (y <= (int)(float)ent_table9[3].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table9[1].startx * z ) &&
		         (x <= (int)(float)ent_table9[2].startx * z ) &&
			 (y <= (int)(float)ent_table9[3].starty * z )){
			no = 1;
		}else if((x >= (int)(float)ent_table9[2].startx * z ) &&
			 (y <= (int)(float)ent_table9[3].starty * z )){
			no = 2;
		}else if((x <= (int)(float)ent_table9[1].startx * z ) &&
			 (y >= (int)(float)ent_table9[3].starty * z ) &&
			 (y <= (int)(float)ent_table9[6].starty * z )){
			no = 3;
		}else if((x >= (int)(float)ent_table9[1].startx * z ) &&
			 (x <= (int)(float)ent_table9[2].startx * z ) &&
			 (y >= (int)(float)ent_table9[3].starty * z ) &&
			 (y <= (int)(float)ent_table9[6].starty * z )){
			no = 4;
		}else if((x >= (int)(float)ent_table9[2].startx * z ) &&
			 (y >= (int)(float)ent_table9[3].starty * z ) &&
			 (y <= (int)(float)ent_table9[6].starty * z )){
			no = 5;
		}else if((x <= (int)(float)ent_table9[1].startx * z ) &&
			 (y >= (int)(float)ent_table9[6].starty * z )){
			no = 6;
		}else if((x >= (int)(float)ent_table9[1].startx * z ) &&
			 (x <= (int)(float)ent_table9[2].startx * z ) &&
			 (y >= (int)(float)ent_table9[6].starty * z )){
			no = 7;
		}else if((x >= (int)(float)ent_table9[2].startx * z ) &&
			 (y >= (int)(float)ent_table9[6].starty * z )){
			no = 8;
		}else{
			return;
		}
		if(ent_table9[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)3.0, (double)3.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}else if((10 <= cur_sess_no) && (cur_sess_no <= 16)){
//
// １６画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<16 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)0.25, (double)0.25);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = pow(mag,zoom_cnt) * 0.25;
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = 0.25 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table16[1].startx * z ) &&
		   (y <= (int)(float)ent_table16[4].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table16[1].startx * z ) &&
		         (x <= (int)(float)ent_table16[2].startx * z ) &&
			 (y <= (int)(float)ent_table16[4].starty * z )){
			no = 1;
		}else if((x >= (int)(float)ent_table16[2].startx * z ) &&
		         (x <= (int)(float)ent_table16[3].startx * z ) &&
			 (y <= (int)(float)ent_table16[4].starty * z )){
			no = 2;
		}else if((x >= (int)(float)ent_table16[3].startx * z ) &&
			 (y <= (int)(float)ent_table16[4].starty * z )){
			no = 3;
		}else if((x <= (int)(float)ent_table16[5].startx * z ) &&
			 (y >= (int)(float)ent_table16[4].starty * z ) &&
			 (y <= (int)(float)ent_table16[8].starty * z )){
			no = 4;
		}else if((x >= (int)(float)ent_table16[5].startx * z ) &&
			 (x <= (int)(float)ent_table16[6].startx * z ) &&
			 (y >= (int)(float)ent_table16[5].starty * z ) &&
			 (y <= (int)(float)ent_table16[9].starty * z )){
			no = 5;
		}else if((x >= (int)(float)ent_table16[6].startx * z ) &&
			 (x <= (int)(float)ent_table16[7].startx * z ) &&
			 (y >= (int)(float)ent_table16[6].starty * z ) &&
			 (y <= (int)(float)ent_table16[10].starty * z )){
			no = 6;
		}else if((x >= (int)(float)ent_table16[7].startx * z ) &&
			 (y >= (int)(float)ent_table16[7].starty * z ) &&
			 (y <= (int)(float)ent_table16[11].starty * z )){
			no = 7;
		}else if((x <= (int)(float)ent_table16[9].startx * z ) &&
			 (y >= (int)(float)ent_table16[8].starty * z ) &&
			 (y <= (int)(float)ent_table16[12].starty * z )){
			no = 8;
		}else if((x >= (int)(float)ent_table16[9].startx * z ) &&
			 (x <= (int)(float)ent_table16[10].startx * z ) &&
			 (y >= (int)(float)ent_table16[9].starty * z ) &&
			 (y <= (int)(float)ent_table16[13].starty * z )){
			no = 9;
		}else if((x >= (int)(float)ent_table16[10].startx * z ) &&
			 (x <= (int)(float)ent_table16[11].startx * z ) &&
			 (y >= (int)(float)ent_table16[10].starty * z ) &&
			 (y <= (int)(float)ent_table16[14].starty * z )){
			no = 10;
		}else if((x >= (int)(float)ent_table16[11].startx * z ) &&
			 (y >= (int)(float)ent_table16[11].starty * z ) &&
			 (y <= (int)(float)ent_table16[15].starty * z )){
			no = 11;
		}else if((x <= (int)(float)ent_table16[13].startx * z ) &&
			 (y >= (int)(float)ent_table16[12].starty * z )){
			no = 12;
		}else if((x >= (int)(float)ent_table16[13].startx * z ) &&
			 (x <= (int)(float)ent_table16[14].startx * z ) &&
			 (y >= (int)(float)ent_table16[13].starty * z )){
			no = 13;
		}else if((x >= (int)(float)ent_table16[14].startx * z ) &&
			 (x <= (int)(float)ent_table16[15].startx * z ) &&
			 (y >= (int)(float)ent_table16[14].starty * z )){
			no = 14;
		}else if((x >= (int)(float)ent_table16[15].startx * z ) &&
			 (y >= (int)(float)ent_table16[15].starty * z )){
			no = 15;
		}else{
			return;
		}
		if(ent_table16[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)4.0, (double)4.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}else if((17 <= cur_sess_no) && (cur_sess_no <= 25)){
//
// ２５画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<25 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)0.2, (double)0.2);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = pow(mag,zoom_cnt) * 0.2;
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = 0.2 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table25[1].startx * z ) &&
		   (y <= (int)(float)ent_table25[5].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table25[1].startx * z ) &&
		         (x <= (int)(float)ent_table25[2].startx * z ) &&
			 (y <= (int)(float)ent_table25[5].starty * z )){
			no = 1;
		}else if((x >= (int)(float)ent_table25[2].startx * z ) &&
		         (x <= (int)(float)ent_table25[3].startx * z ) &&
			 (y <= (int)(float)ent_table25[5].starty * z )){
			no = 2;
		}else if((x >= (int)(float)ent_table25[3].startx * z ) &&
		         (x <= (int)(float)ent_table25[4].startx * z ) &&
			 (y <= (int)(float)ent_table25[5].starty * z )){
			no = 3;
		}else if((x >= (int)(float)ent_table25[4].startx * z ) &&
			 (y <= (int)(float)ent_table25[5].starty * z )){
			no = 4;
		}else if((x <= (int)(float)ent_table25[6].startx * z ) &&
			 (y >= (int)(float)ent_table25[5].starty * z ) &&
			 (y <= (int)(float)ent_table25[10].starty * z )){
			no = 5;
		}else if((x >= (int)(float)ent_table25[6].startx * z ) &&
			 (x <= (int)(float)ent_table25[7].startx * z ) &&
			 (y >= (int)(float)ent_table25[6].starty * z ) &&
			 (y <= (int)(float)ent_table25[11].starty * z )){
			no = 6;
		}else if((x >= (int)(float)ent_table25[7].startx * z ) &&
			 (x <= (int)(float)ent_table25[8].startx * z ) &&
			 (y >= (int)(float)ent_table25[7].starty * z ) &&
			 (y <= (int)(float)ent_table25[12].starty * z )){
			no = 7;
		}else if((x >= (int)(float)ent_table25[8].startx * z ) &&
			 (x <= (int)(float)ent_table25[9].startx * z ) &&
			 (y >= (int)(float)ent_table25[8].starty * z ) &&
			 (y <= (int)(float)ent_table25[13].starty * z )){
			no = 8;
		}else if((x >= (int)(float)ent_table25[9].startx * z ) &&
			 (y >= (int)(float)ent_table25[9].starty * z ) &&
			 (y <= (int)(float)ent_table25[14].starty * z )){
			no = 9;
		}else if((x <= (int)(float)ent_table25[11].startx * z ) &&
			 (y >= (int)(float)ent_table25[10].starty * z ) &&
			 (y <= (int)(float)ent_table25[15].starty * z )){
			no = 10;
		}else if((x >= (int)(float)ent_table25[11].startx * z ) &&
			 (x <= (int)(float)ent_table25[12].startx * z ) &&
			 (y >= (int)(float)ent_table25[11].starty * z ) &&
			 (y <= (int)(float)ent_table25[16].starty * z )){
			no = 11;
		}else if((x >= (int)(float)ent_table25[12].startx * z ) &&
			 (x <= (int)(float)ent_table25[13].startx * z ) &&
			 (y >= (int)(float)ent_table25[12].starty * z ) &&
			 (y <= (int)(float)ent_table25[17].starty * z )){
			no = 12;
		}else if((x >= (int)(float)ent_table25[13].startx * z ) &&
			 (x <= (int)(float)ent_table25[14].startx * z ) &&
			 (y >= (int)(float)ent_table25[13].starty * z ) &&
			 (y <= (int)(float)ent_table25[18].starty * z )){
			no = 13;
		}else if((x >= (int)(float)ent_table25[14].startx * z ) &&
			 (y >= (int)(float)ent_table25[14].starty * z ) &&
			 (y <= (int)(float)ent_table25[19].starty * z )){
			no = 14;
		}else if((x <= (int)(float)ent_table25[16].startx * z ) &&
			 (y >= (int)(float)ent_table25[15].starty * z ) &&
			 (y <= (int)(float)ent_table25[20].starty * z )){
			no = 15;
		}else if((x >= (int)(float)ent_table25[16].startx * z ) &&
			 (x <= (int)(float)ent_table25[17].startx * z ) &&
			 (y >= (int)(float)ent_table25[16].starty * z ) &&
			 (y <= (int)(float)ent_table25[21].starty * z )){
			no = 16;
		}else if((x >= (int)(float)ent_table25[17].startx * z ) &&
			 (x <= (int)(float)ent_table25[18].startx * z ) &&
			 (y >= (int)(float)ent_table25[17].starty * z ) &&
			 (y <= (int)(float)ent_table25[22].starty * z )){
			no = 17;
		}else if((x >= (int)(float)ent_table25[18].startx * z ) &&
			 (x <= (int)(float)ent_table25[19].startx * z ) &&
			 (y >= (int)(float)ent_table25[18].starty * z ) &&
			 (y <= (int)(float)ent_table25[23].starty * z )){
			no = 18;
		}else if((x >= (int)(float)ent_table25[19].startx * z ) &&
			 (y >= (int)(float)ent_table25[19].starty * z ) &&
			 (y <= (int)(float)ent_table25[24].starty * z )){
			no = 19;
		}else if((x <= (int)(float)ent_table25[21].startx * z ) &&
			 (y >= (int)(float)ent_table25[20].starty * z )){
			no = 20;
		}else if((x >= (int)(float)ent_table25[21].startx * z ) &&
			 (x <= (int)(float)ent_table25[22].startx * z ) &&
			 (y >= (int)(float)ent_table25[21].starty * z )){
			no = 21;
		}else if((x >= (int)(float)ent_table25[22].startx * z ) &&
			 (x <= (int)(float)ent_table25[23].startx * z ) &&
			 (y >= (int)(float)ent_table25[22].starty * z )){
			no = 22;
		}else if((x >= (int)(float)ent_table25[23].startx * z ) &&
			 (x <= (int)(float)ent_table25[24].startx * z ) &&
			 (y >= (int)(float)ent_table25[23].starty * z )){
			no = 23;
		}else if((x >= (int)(float)ent_table25[24].startx * z ) &&
			 (y >= (int)(float)ent_table25[24].starty * z )){
			no = 24;
		}else{
			return;
		}
		if(ent_table25[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)5.0, (double)5.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}else if((26 <= cur_sess_no) && (cur_sess_no <= 36)){
//
// ３６画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<36 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)1.0/(double)6.0, (double)1.0/(double)6.0);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = (double)1.0/(double)6.0 * pow(mag,zoom_cnt);
		if(FullFlg){
			GetResolution(&x2,&y2);
			z = (double)1.0/(double)6.0 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table36[1].startx * z ) &&
		   (y <= (int)(float)ent_table36[6].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table36[1].startx * z ) &&
		         (x <= (int)(float)ent_table36[2].startx * z ) &&
			 (y <= (int)(float)ent_table36[7].starty * z )){
			no = 1;
		}else if((x >= (int)(float)ent_table36[2].startx * z ) &&
		         (x <= (int)(float)ent_table36[3].startx * z ) &&
			 (y <= (int)(float)ent_table36[8].starty * z )){
			no = 2;
		}else if((x >= (int)(float)ent_table36[3].startx * z ) &&
		         (x <= (int)(float)ent_table36[4].startx * z ) &&
			 (y <= (int)(float)ent_table36[9].starty * z )){
			no = 3;
		}else if((x >= (int)(float)ent_table36[4].startx * z ) &&
		         (x <= (int)(float)ent_table36[5].startx * z ) &&
			 (y <= (int)(float)ent_table36[10].starty * z )){
			no = 4;
		}else if((x >= (int)(float)ent_table36[5].startx * z ) &&
			 (y <= (int)(float)ent_table36[11].starty * z )){
			no = 5;
		}else if((x >= (int)(float)ent_table36[6].startx * z ) &&
			 (x <= (int)(float)ent_table36[7].startx * z ) &&
			 (y >= (int)(float)ent_table36[6].starty * z ) &&
			 (y <= (int)(float)ent_table36[12].starty * z )){
			no = 6;
		}else if((x >= (int)(float)ent_table36[7].startx * z ) &&
			 (x <= (int)(float)ent_table36[8].startx * z ) &&
			 (y >= (int)(float)ent_table36[7].starty * z ) &&
			 (y <= (int)(float)ent_table36[13].starty * z )){
			no = 7;
		}else if((x >= (int)(float)ent_table36[8].startx * z ) &&
			 (x <= (int)(float)ent_table36[9].startx * z ) &&
			 (y >= (int)(float)ent_table36[8].starty * z ) &&
			 (y <= (int)(float)ent_table36[14].starty * z )){
			no = 8;
		}else if((x >= (int)(float)ent_table36[9].startx * z ) &&
			 (x <= (int)(float)ent_table36[10].startx * z ) &&
			 (y >= (int)(float)ent_table36[9].starty * z ) &&
			 (y <= (int)(float)ent_table36[15].starty * z )){
			no = 9;
		}else if((x >= (int)(float)ent_table36[10].startx * z ) &&
			 (x <= (int)(float)ent_table36[11].startx * z ) &&
			 (y >= (int)(float)ent_table36[10].starty * z ) &&
			 (y <= (int)(float)ent_table36[16].starty * z )){
			no = 10;
		}else if((x >= (int)(float)ent_table36[11].startx * z ) &&
			 (y >= (int)(float)ent_table36[11].starty * z ) &&
			 (y <= (int)(float)ent_table36[17].starty * z )){
			no = 11;
		}else if((x >= (int)(float)ent_table36[12].startx * z ) &&
			 (x <= (int)(float)ent_table36[13].startx * z ) &&
			 (y >= (int)(float)ent_table36[12].starty * z ) &&
			 (y <= (int)(float)ent_table36[18].starty * z )){
			no = 12;
		}else if((x >= (int)(float)ent_table36[13].startx * z ) &&
			 (x <= (int)(float)ent_table36[14].startx * z ) &&
			 (y >= (int)(float)ent_table36[13].starty * z ) &&
			 (y <= (int)(float)ent_table36[19].starty * z )){
			no = 13;
		}else if((x >= (int)(float)ent_table36[14].startx * z ) &&
			 (x <= (int)(float)ent_table36[15].startx * z ) &&
			 (y >= (int)(float)ent_table36[14].starty * z ) &&
			 (y <= (int)(float)ent_table36[20].starty * z )){
			no = 14;
		}else if((x >= (int)(float)ent_table36[15].startx * z ) &&
			 (x <= (int)(float)ent_table36[16].startx * z ) &&
			 (y >= (int)(float)ent_table36[15].starty * z ) &&
			 (y <= (int)(float)ent_table36[21].starty * z )){
			no = 15;
		}else if((x >= (int)(float)ent_table36[16].startx * z ) &&
			 (x <= (int)(float)ent_table36[17].startx * z ) &&
			 (y >= (int)(float)ent_table36[16].starty * z ) &&
			 (y <= (int)(float)ent_table36[22].starty * z )){
			no = 16;
		}else if((x >= (int)(float)ent_table36[17].startx * z ) &&
			 (y >= (int)(float)ent_table36[17].starty * z ) &&
			 (y <= (int)(float)ent_table36[23].starty * z )){
			no = 17;
		}else if((x >= (int)(float)ent_table36[18].startx * z ) &&
			 (x <= (int)(float)ent_table36[19].startx * z ) &&
			 (y >= (int)(float)ent_table36[18].starty * z ) &&
			 (y <= (int)(float)ent_table36[24].starty * z )){
			no = 18;
		}else if((x >= (int)(float)ent_table36[19].startx * z ) &&
			 (x <= (int)(float)ent_table36[20].startx * z ) &&
			 (y >= (int)(float)ent_table36[19].starty * z ) &&
			 (y <= (int)(float)ent_table36[25].starty * z )){
			no = 19;
		}else if((x >= (int)(float)ent_table36[20].startx * z ) &&
			 (x <= (int)(float)ent_table36[21].startx * z ) &&
			 (y >= (int)(float)ent_table36[20].starty * z ) &&
			 (y <= (int)(float)ent_table36[26].starty * z )){
			no = 20;
		}else if((x >= (int)(float)ent_table36[21].startx * z ) &&
			 (x <= (int)(float)ent_table36[22].startx * z ) &&
			 (y >= (int)(float)ent_table36[21].starty * z ) &&
			 (y <= (int)(float)ent_table36[27].starty * z )){
			no = 21;
		}else if((x >= (int)(float)ent_table36[22].startx * z ) &&
			 (x <= (int)(float)ent_table36[23].startx * z ) &&
			 (y >= (int)(float)ent_table36[22].starty * z ) &&
			 (y <= (int)(float)ent_table36[28].starty * z )){
			no = 22;
		}else if((x >= (int)(float)ent_table36[23].startx * z ) &&
			 (y >= (int)(float)ent_table36[23].starty * z ) &&
			 (y <= (int)(float)ent_table36[29].starty * z )){
			no = 23;
		}else if((x >= (int)(float)ent_table36[24].startx * z ) &&
			 (x <= (int)(float)ent_table36[25].startx * z ) &&
			 (y >= (int)(float)ent_table36[24].starty * z ) &&
			 (y <= (int)(float)ent_table36[30].starty * z )){
			no = 24;
		}else if((x >= (int)(float)ent_table36[25].startx * z ) &&
			 (x <= (int)(float)ent_table36[26].startx * z ) &&
			 (y >= (int)(float)ent_table36[25].starty * z ) &&
			 (y <= (int)(float)ent_table36[31].starty * z )){
			no = 25;
		}else if((x >= (int)(float)ent_table36[26].startx * z ) &&
			 (x <= (int)(float)ent_table36[27].startx * z ) &&
			 (y >= (int)(float)ent_table36[26].starty * z ) &&
			 (y <= (int)(float)ent_table36[32].starty * z )){
			no = 26;
		}else if((x >= (int)(float)ent_table36[27].startx * z ) &&
			 (x <= (int)(float)ent_table36[28].startx * z ) &&
			 (y >= (int)(float)ent_table36[27].starty * z ) &&
			 (y <= (int)(float)ent_table36[33].starty * z )){
			no = 27;
		}else if((x >= (int)(float)ent_table36[28].startx * z ) &&
			 (x <= (int)(float)ent_table36[29].startx * z ) &&
			 (y >= (int)(float)ent_table36[28].starty * z ) &&
			 (y <= (int)(float)ent_table36[34].starty * z )){
			no = 28;
		}else if((x >= (int)(float)ent_table36[29].startx * z ) &&
			 (y >= (int)(float)ent_table36[29].starty * z ) &&
			 (y <= (int)(float)ent_table36[35].starty * z )){
			no = 29;
		}else if((x >= (int)(float)ent_table36[30].startx * z ) &&
			 (x <= (int)(float)ent_table36[31].startx * z ) &&
			 (y >= (int)(float)ent_table36[30].starty * z )){
			no = 30;
		}else if((x >= (int)(float)ent_table36[31].startx * z ) &&
			 (x <= (int)(float)ent_table36[32].startx * z ) &&
			 (y >= (int)(float)ent_table36[31].starty * z )){
			no = 31;
		}else if((x >= (int)(float)ent_table36[32].startx * z ) &&
			 (x <= (int)(float)ent_table36[33].startx * z ) &&
			 (y >= (int)(float)ent_table36[32].starty * z )){
			no = 32;
		}else if((x >= (int)(float)ent_table36[33].startx * z ) &&
			 (x <= (int)(float)ent_table36[34].startx * z ) &&
			 (y >= (int)(float)ent_table36[33].starty * z )){
			no = 33;
		}else if((x >= (int)(float)ent_table36[34].startx * z ) &&
			 (x <= (int)(float)ent_table36[35].startx * z ) &&
			 (y >= (int)(float)ent_table36[34].starty * z )){
			no = 34;
		}else if((x >= (int)(float)ent_table36[35].startx * z ) &&
			 (y >= (int)(float)ent_table36[35].starty * z )){
			no = 35;
		}else{
			return;
		}
		if(ent_table36[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)6.0, (double)6.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}else if((37 <= cur_sess_no) && (cur_sess_no <= 49)){
//
// ４９画面表示の場合
//
		if(PinPmode == 2){		// 復元する
			pthread_mutex_lock(&m_recv_list);
			list = recv_list_top;
			for(i=0;i<49 && list;i++){
				if(list->background == 1){
					list->background = 0;
					break;
				}
				list = list->next;
			}

			cairo_scale(gcr, (double)1.0/(double)7.0, (double)1.0/(double)7.0);
//			cairo_scale(gcr, (double)1.1, (double)1.0);
			PinPmode = 0;
			chgflg = 1;
			pthread_mutex_unlock(&m_recv_list);
			return;
		}
		z = (double)1.0/(double)7.0 * pow(mag,zoom_cnt);

		if(FullFlg){
			GetResolution(&x2,&y2);
			z = (double)1.0/(double)7.0 * (double)x2/(double)width;
		}
// 画面番号を決める
		if((x <= (int)(float)ent_table49[1].startx * z ) &&
		   (y <= (int)(float)ent_table49[7].starty * z )){
			no = 0;
		}else if((x >= (int)(float)ent_table49[1].startx * z ) &&
		         (x <= (int)(float)ent_table49[2].startx * z ) &&
			 (y <= (int)(float)ent_table49[8].starty * z )){
			no = 1;
		}else if((x >= (int)(float)ent_table49[2].startx * z ) &&
		         (x <= (int)(float)ent_table49[3].startx * z ) &&
			 (y <= (int)(float)ent_table49[9].starty * z )){
			no = 2;
		}else if((x >= (int)(float)ent_table49[3].startx * z ) &&
		         (x <= (int)(float)ent_table49[4].startx * z ) &&
			 (y <= (int)(float)ent_table49[10].starty * z )){
			no = 3;
		}else if((x >= (int)(float)ent_table49[4].startx * z ) &&
		         (x <= (int)(float)ent_table49[5].startx * z ) &&
			 (y <= (int)(float)ent_table49[11].starty * z )){
			no = 4;
		}else if((x >= (int)(float)ent_table49[5].startx * z ) &&
		         (x <= (int)(float)ent_table49[6].startx * z ) &&
			 (y <= (int)(float)ent_table49[12].starty * z )){
			no = 5;
		}else if((x >= (int)(float)ent_table49[6].startx * z ) &&
			 (y <= (int)(float)ent_table49[13].starty * z )){
			no = 6;
		}else if((x >= (int)(float)ent_table49[7].startx * z ) &&
			 (x <= (int)(float)ent_table49[8].startx * z ) &&
			 (y >= (int)(float)ent_table49[7].starty * z ) &&
			 (y <= (int)(float)ent_table49[14].starty * z )){
			no = 7;
		}else if((x >= (int)(float)ent_table49[8].startx * z ) &&
			 (x <= (int)(float)ent_table49[9].startx * z ) &&
			 (y >= (int)(float)ent_table49[8].starty * z ) &&
			 (y <= (int)(float)ent_table49[15].starty * z )){
			no = 8;
		}else if((x >= (int)(float)ent_table49[9].startx * z ) &&
			 (x <= (int)(float)ent_table49[10].startx * z ) &&
			 (y >= (int)(float)ent_table49[9].starty * z ) &&
			 (y <= (int)(float)ent_table49[16].starty * z )){
			no = 9;
		}else if((x >= (int)(float)ent_table49[10].startx * z ) &&
			 (x <= (int)(float)ent_table49[11].startx * z ) &&
			 (y >= (int)(float)ent_table49[10].starty * z ) &&
			 (y <= (int)(float)ent_table49[17].starty * z )){
			no = 10;
		}else if((x >= (int)(float)ent_table49[11].startx * z ) &&
			 (x <= (int)(float)ent_table49[12].startx * z ) &&
			 (y >= (int)(float)ent_table49[11].starty * z ) &&
			 (y <= (int)(float)ent_table49[18].starty * z )){
			no = 11;
		}else if((x >= (int)(float)ent_table49[12].startx * z ) &&
			 (x <= (int)(float)ent_table49[13].startx * z ) &&
			 (y >= (int)(float)ent_table49[12].starty * z ) &&
			 (y <= (int)(float)ent_table49[19].starty * z )){
			no = 12;
		}else if((x >= (int)(float)ent_table49[13].startx * z ) &&
			 (y >= (int)(float)ent_table49[13].starty * z ) &&
			 (y <= (int)(float)ent_table49[20].starty * z )){
			no = 13;
		}else if((x >= (int)(float)ent_table49[14].startx * z ) &&
			 (x <= (int)(float)ent_table49[15].startx * z ) &&
			 (y >= (int)(float)ent_table49[14].starty * z ) &&
			 (y <= (int)(float)ent_table49[21].starty * z )){
			no = 14;
		}else if((x >= (int)(float)ent_table49[15].startx * z ) &&
			 (x <= (int)(float)ent_table49[16].startx * z ) &&
			 (y >= (int)(float)ent_table49[15].starty * z ) &&
			 (y <= (int)(float)ent_table49[22].starty * z )){
			no = 15;
		}else if((x >= (int)(float)ent_table49[16].startx * z ) &&
			 (x <= (int)(float)ent_table49[17].startx * z ) &&
			 (y >= (int)(float)ent_table49[16].starty * z ) &&
			 (y <= (int)(float)ent_table49[23].starty * z )){
			no = 16;
		}else if((x >= (int)(float)ent_table49[17].startx * z ) &&
			 (x <= (int)(float)ent_table49[18].startx * z ) &&
			 (y >= (int)(float)ent_table49[17].starty * z ) &&
			 (y <= (int)(float)ent_table49[24].starty * z )){
			no = 17;
		}else if((x >= (int)(float)ent_table49[18].startx * z ) &&
			 (x <= (int)(float)ent_table49[19].startx * z ) &&
			 (y >= (int)(float)ent_table49[18].starty * z ) &&
			 (y <= (int)(float)ent_table49[25].starty * z )){
			no = 18;
		}else if((x >= (int)(float)ent_table49[19].startx * z ) &&
			 (x <= (int)(float)ent_table49[20].startx * z ) &&
			 (y >= (int)(float)ent_table49[19].starty * z ) &&
			 (y <= (int)(float)ent_table49[26].starty * z )){
			no = 19;
		}else if((x >= (int)(float)ent_table49[20].startx * z ) &&
			 (y >= (int)(float)ent_table49[20].starty * z ) &&
			 (y <= (int)(float)ent_table49[27].starty * z )){
			no = 20;
		}else if((x >= (int)(float)ent_table49[21].startx * z ) &&
			 (x <= (int)(float)ent_table49[22].startx * z ) &&
			 (y >= (int)(float)ent_table49[21].starty * z ) &&
			 (y <= (int)(float)ent_table49[28].starty * z )){
			no = 21;
		}else if((x >= (int)(float)ent_table49[22].startx * z ) &&
			 (x <= (int)(float)ent_table49[23].startx * z ) &&
			 (y >= (int)(float)ent_table49[22].starty * z ) &&
			 (y <= (int)(float)ent_table49[29].starty * z )){
			no = 22;
		}else if((x >= (int)(float)ent_table49[23].startx * z ) &&
			 (x <= (int)(float)ent_table49[24].startx * z ) &&
			 (y >= (int)(float)ent_table49[23].starty * z ) &&
			 (y <= (int)(float)ent_table49[30].starty * z )){
			no = 23;
		}else if((x >= (int)(float)ent_table49[24].startx * z ) &&
			 (x <= (int)(float)ent_table49[25].startx * z ) &&
			 (y >= (int)(float)ent_table49[24].starty * z ) &&
			 (y <= (int)(float)ent_table49[31].starty * z )){
			no = 24;
		}else if((x >= (int)(float)ent_table49[25].startx * z ) &&
			 (x <= (int)(float)ent_table49[26].startx * z ) &&
			 (y >= (int)(float)ent_table49[25].starty * z ) &&
			 (y <= (int)(float)ent_table49[32].starty * z )){
			no = 25;
		}else if((x >= (int)(float)ent_table49[26].startx * z ) &&
			 (x <= (int)(float)ent_table49[27].startx * z ) &&
			 (y >= (int)(float)ent_table49[26].starty * z ) &&
			 (y <= (int)(float)ent_table49[32].starty * z )){
			no = 26;
		}else if((x >= (int)(float)ent_table49[27].startx * z ) &&
			 (y >= (int)(float)ent_table49[27].starty * z ) &&
			 (y <= (int)(float)ent_table49[34].starty * z )){
			no = 27;
		}else if((x >= (int)(float)ent_table49[28].startx * z ) &&
			 (x <= (int)(float)ent_table49[29].startx * z ) &&
			 (y >= (int)(float)ent_table49[28].starty * z ) &&
			 (y <= (int)(float)ent_table49[35].starty * z )){
			no = 28;
		}else if((x >= (int)(float)ent_table49[29].startx * z ) &&
			 (x <= (int)(float)ent_table49[30].startx * z ) &&
			 (y >= (int)(float)ent_table49[29].starty * z ) &&
			 (y <= (int)(float)ent_table49[36].starty * z )){
			no = 29;
		}else if((x >= (int)(float)ent_table49[30].startx * z ) &&
			 (x <= (int)(float)ent_table49[31].startx * z ) &&
			 (y >= (int)(float)ent_table49[30].starty * z ) &&
			 (y <= (int)(float)ent_table49[37].starty * z )){
			no = 30;
		}else if((x >= (int)(float)ent_table49[31].startx * z ) &&
			 (x <= (int)(float)ent_table49[32].startx * z ) &&
			 (y >= (int)(float)ent_table49[31].starty * z ) &&
			 (y <= (int)(float)ent_table49[38].starty * z )){
			no = 31;
		}else if((x >= (int)(float)ent_table49[32].startx * z ) &&
			 (x <= (int)(float)ent_table49[33].startx * z ) &&
			 (y >= (int)(float)ent_table49[32].starty * z ) &&
			 (y <= (int)(float)ent_table49[39].starty * z )){
			no = 32;
		}else if((x >= (int)(float)ent_table49[33].startx * z ) &&
			 (x <= (int)(float)ent_table49[34].startx * z ) &&
			 (y >= (int)(float)ent_table49[33].starty * z ) &&
			 (y <= (int)(float)ent_table49[40].starty * z )){
			no = 33;
		}else if((x >= (int)(float)ent_table49[34].startx * z ) &&
			 (y >= (int)(float)ent_table49[34].starty * z ) &&
			 (y <= (int)(float)ent_table49[41].starty * z )){
			no = 34;
		}else if((x >= (int)(float)ent_table49[35].startx * z ) &&
			 (x <= (int)(float)ent_table49[36].startx * z ) &&
			 (y >= (int)(float)ent_table49[35].starty * z ) &&
			 (y <= (int)(float)ent_table49[42].starty * z )){
			no = 35;
		}else if((x >= (int)(float)ent_table49[36].startx * z ) &&
			 (x <= (int)(float)ent_table49[37].startx * z ) &&
			 (y >= (int)(float)ent_table49[36].starty * z ) &&
			 (y <= (int)(float)ent_table49[43].starty * z )){
			no = 36;
		}else if((x >= (int)(float)ent_table49[37].startx * z ) &&
			 (x <= (int)(float)ent_table49[38].startx * z ) &&
			 (y >= (int)(float)ent_table49[37].starty * z ) &&
			 (y <= (int)(float)ent_table49[44].starty * z )){
			no = 37;
		}else if((x >= (int)(float)ent_table49[38].startx * z ) &&
			 (x <= (int)(float)ent_table49[39].startx * z ) &&
			 (y >= (int)(float)ent_table49[38].starty * z ) &&
			 (y <= (int)(float)ent_table49[45].starty * z )){
			no = 38;
		}else if((x >= (int)(float)ent_table49[39].startx * z ) &&
			 (x <= (int)(float)ent_table49[40].startx * z ) &&
			 (y >= (int)(float)ent_table49[39].starty * z ) &&
			 (y <= (int)(float)ent_table49[46].starty * z )){
			no = 39;
		}else if((x >= (int)(float)ent_table49[40].startx * z ) &&
			 (x <= (int)(float)ent_table49[41].startx * z ) &&
			 (y >= (int)(float)ent_table49[40].starty * z ) &&
			 (y <= (int)(float)ent_table49[47].starty * z )){
			no = 40;
		}else if((x >= (int)(float)ent_table49[41].startx * z ) &&
			 (y >= (int)(float)ent_table49[41].starty * z ) &&
			 (y <= (int)(float)ent_table49[48].starty * z )){
			no = 41;
		}else if((x >= (int)(float)ent_table49[42].startx * z ) &&
			 (x <= (int)(float)ent_table49[43].startx * z ) &&
			 (y >= (int)(float)ent_table49[42].starty * z )){
			no = 42;
		}else if((x >= (int)(float)ent_table49[43].startx * z ) &&
			 (x <= (int)(float)ent_table49[44].startx * z ) &&
			 (y >= (int)(float)ent_table49[43].starty * z )){
			no = 43;
		}else if((x >= (int)(float)ent_table49[44].startx * z ) &&
			 (x <= (int)(float)ent_table49[45].startx * z ) &&
			 (y >= (int)(float)ent_table49[44].starty * z )){
			no = 44;
		}else if((x >= (int)(float)ent_table49[45].startx * z ) &&
			 (x <= (int)(float)ent_table49[46].startx * z ) &&
			 (y >= (int)(float)ent_table49[45].starty * z )){
			no = 45;
		}else if((x >= (int)(float)ent_table49[46].startx * z ) &&
			 (x <= (int)(float)ent_table49[47].startx * z ) &&
			 (y >= (int)(float)ent_table49[46].starty * z )){
			no = 46;
		}else if((x >= (int)(float)ent_table49[47].startx * z ) &&
			 (x <= (int)(float)ent_table49[48].startx * z ) &&
			 (y >= (int)(float)ent_table49[47].starty * z )){
			no = 47;
		}else if((x >= (int)(float)ent_table49[48].startx * z ) &&
			 (y >= (int)(float)ent_table49[48].starty * z )){
			no = 48;
		}else{
			return;
		}
		if(ent_table49[no].used == 0){
			return;
		}

		pthread_mutex_lock(&m_recv_list);
		cairo_scale(gcr, (double)7.0, (double)7.0);
		cairo_scale(gcr, (double)1.0/(double)1.1, (double)1.0);

		list = recv_list_top;
		for(i=0;i<cur_sess_no && list;i++){
			if(list->disp_no == no){
				list->background = 1;
				break;
			}
			list = list->next;
		}
		pthread_mutex_unlock(&m_recv_list);
		PinPmode = 1;
		chgflg = 1;
	}

	return;
}
static	void	set_area(int	flg)
{
	int		x,y;
	int		bx,by;


	GetResolution(&x,&y);

	if(x <= 800){
		zoom_cnt = -3.0;
	}else if(x <= 1024){
		zoom_cnt = -2.0;
	}else if(x <= 1280){
		zoom_cnt = -1.0;
	}else{
		zoom_cnt = 0.0;
	}

	if(flg == 1){
		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(window), "IHAPHONE");
		gtk_widget_set_size_request(window, width*pow(mag,zoom_cnt),(height+15)*pow(mag,zoom_cnt));
		gtk_window_set_resizable (GTK_WINDOW(window), FALSE);
//
//ウインドウに図形を描けるように設定
//
		gtk_widget_set_app_paintable(window, TRUE);
//
// windowのコールバックを登録する。
//
		g_signal_connect(G_OBJECT(window), "destroy",
			G_CALLBACK(destroy_win), NULL);
		g_signal_connect(G_OBJECT(window), "window-state-event",
			G_CALLBACK(cb_state_event), NULL);
		g_signal_connect(G_OBJECT(window), "key-press-event",
			G_CALLBACK(cb_key_press), NULL);

		vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
		if(vbox == NULL){
			fprintf(stderr,"gtk_box_new error.(file=%s line=%d)\n",__FILE__,__LINE__);
			return;
		}
		gtk_container_add (GTK_CONTAINER (window), vbox);
//
// provider
//
		provider = gtk_css_provider_new ();
		gtk_css_provider_load_from_data (GTK_CSS_PROVIDER(provider),css_data, -1, NULL);

		provider2 = gtk_css_provider_new ();
		gtk_css_provider_load_from_data (GTK_CSS_PROVIDER(provider2),css_data2, -1, NULL);
	}
//
// drawingarea
//
	drawingarea = gtk_drawing_area_new();
	if(drawingarea == NULL){
		fprintf(stderr,"gtk_drawing_area_new error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	gtk_widget_set_size_request(GTK_WIDGET(drawingarea),width*pow(mag,zoom_cnt),height*pow(mag,zoom_cnt));
	gtk_widget_add_events(drawingarea, GDK_BUTTON_PRESS_MASK);
// drawingareaのコールバックを登録する。
	g_signal_connect(G_OBJECT(drawingarea), "button_press_event", G_CALLBACK(cb_mouse),NULL);
	g_signal_connect(G_OBJECT(drawingarea), "draw", G_CALLBACK(cb_draw), NULL);
	g_signal_connect(G_OBJECT(drawingarea), "realize", G_CALLBACK(cb_realize_dummy), NULL);
	g_signal_connect(G_OBJECT(drawingarea), "size-allocate", G_CALLBACK(cb_size_allocate), NULL);


	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL,10 );
	if(hbox == NULL){
		fprintf(stderr,"gtk_box_new2 error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	gtk_container_add (GTK_CONTAINER (vbox), hbox);


	bx = 110*pow(mag,zoom_cnt);
//	by = 15*pow(mag,zoom_cnt);
	by = MENU_HEIGHT;

	button_connect = gtk_button_new_with_label(gettext("Connect"));
	gtk_widget_set_size_request(button_connect,bx,by);
	g_signal_connect(G_OBJECT(button_connect), "clicked",
			 G_CALLBACK(cb_connect), NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button_connect);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_connect), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_disconnect = gtk_button_new_with_label (gettext("Disconnect"));
	gtk_widget_set_size_request(button_disconnect,bx,by);
	gtk_container_add (GTK_CONTAINER (hbox), button_disconnect);;
	g_signal_connect (button_disconnect, "clicked",
		G_CALLBACK (cb_disconnect), NULL);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_disconnect), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_respond = gtk_button_new_with_label (gettext("Respond"));
	gtk_widget_set_size_request(button_respond,bx,by);
	gtk_container_add (GTK_CONTAINER (hbox), button_respond);;
	g_signal_connect (button_respond, "clicked", G_CALLBACK (cb_respond), NULL);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_respond), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_set = gtk_button_new_with_label (gettext("Set"));
	gtk_widget_set_size_request(button_set,bx,by);
	gtk_container_add (GTK_CONTAINER (hbox), button_set);;
	g_signal_connect (button_set, "clicked", G_CALLBACK (cb_set), NULL);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_set), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_rec = gtk_button_new_with_label (gettext("Start Rec"));
	gtk_widget_set_size_request(button_rec,bx,by);
	gtk_container_add (GTK_CONTAINER (hbox), button_rec);;
	g_signal_connect (button_rec, "clicked", G_CALLBACK (cb_rec), NULL);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_rec), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_hot = gtk_button_new_with_label(gettext("Hot Line"));
	gtk_widget_set_size_request(button_hot,bx,by);
	g_signal_connect(G_OBJECT(button_hot), "clicked", G_CALLBACK(cb_hotline), NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button_hot);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_hot), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_key = gtk_button_new_with_label(gettext("Key Share"));
	gtk_widget_set_size_request(button_key,bx,by);
	g_signal_connect(G_OBJECT(button_key), "clicked", G_CALLBACK(cb_key_share), NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button_key);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_key), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_cap = gtk_button_new_with_label(gettext("Capture"));
	gtk_widget_set_size_request(button_cap,bx,by);
	g_signal_connect(G_OBJECT(button_cap), "clicked", G_CALLBACK(cb_capture), NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button_cap);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_cap), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_lic = gtk_button_new_with_label(gettext("License"));
	gtk_widget_set_size_request(button_lic,bx,by);
	g_signal_connect(G_OBJECT(button_lic), "clicked", G_CALLBACK(cb_license), NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button_lic);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_lic), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

	button_exit = gtk_button_new_with_label(gettext("Exit"));
	gtk_widget_set_size_request(button_exit,bx,by);
	g_signal_connect(G_OBJECT(button_exit), "clicked", G_CALLBACK(destroy_win), NULL);
	gtk_container_add (GTK_CONTAINER (hbox), button_exit);
	gtk_style_context_add_provider(gtk_widget_get_style_context(button_exit), 
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);


	gtk_container_add (GTK_CONTAINER (vbox), drawingarea);

	if(flg == 1){
//
// ユーザ定義シグナルの登録
//
//		g_signal_new("user0", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
//		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,G_TYPE_POINTER);
//		g_signal_connect(window, "user0", G_CALLBACK(OnUpdateBannar), NULL);

		g_signal_new("AcceptDialog", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,G_TYPE_POINTER);
		g_signal_connect(window, "AcceptDialog", G_CALLBACK(OnAcceptDialog), NULL);

		g_signal_new("MessageDialog", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,G_TYPE_POINTER);
		g_signal_connect(window, "MessageDialog", G_CALLBACK(OnMessageDialog), NULL);

		g_signal_new("UpdateButton", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,G_TYPE_POINTER);
		g_signal_connect(window, "UpdateButton", G_CALLBACK(OnUpdateButton), NULL);

		g_signal_new("Refresh", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,G_TYPE_POINTER);
		g_signal_connect(window, "Refresh", G_CALLBACK(OnRefresh), window);

		g_signal_new("DeleteWdialog", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,G_TYPE_POINTER);
		g_signal_connect(window, "DeleteWdialog", G_CALLBACK(OnDeleteWdialog), NULL);
	}

	gtk_widget_show_all(window);		// cairo_region_createの前に実行すること！！
//
// cairoの作成
//
#if GTK_CHECK_VERSION(3,22,0)
	clip = cairo_region_create();
	if(clip == NULL){
		fprintf(stderr,"cairo_region_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	dc = gdk_window_begin_draw_frame(gtk_widget_get_window(drawingarea), clip);
	if(dc == NULL){
		fprintf(stderr,"gdk_window_begin_draw_frame error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
	gcr = gdk_drawing_context_get_cairo_context(dc);
	if(gcr == NULL){
		fprintf(stderr,"gdk_drawing_context_get_cairo_context error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return;
	}
#else
	gcr  = (cairo_t *)gdk_cairo_create(gtk_widget_get_window(drawingarea));
#endif

	cairo_scale(gcr, (double)1.0*pow(mag,zoom_cnt) , (double)1.0*pow(mag,zoom_cnt) );

	set_init_image(0,0);
}

static	int	set_params(void)
{
	FILE		*fp;
	char		buf[1024],buf2[1024],*str,*stop;
	int		val,r,rv;


	rv = 0;

	fp = fopen("./ihaphone-params.txt","r");
	if(fp == NULL){
		fprintf(stderr,"Warning : parameter file not found.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	while(1){
		if((r = my_fgets(buf,sizeof(buf),fp)) == 0){
			break;
		}
		if(r == -1){
			rv = 1;
		}
		if((buf[0] == '#') || buf[0] == '\n'){
			continue;
		}
		my_strncpy(buf2,buf,sizeof(buf2));

		str = strtok_r(buf,"=\n",&stop);
		if(!str){
			continue;
		}
		str = strtok_r(NULL,"=\n",&stop);
		if(!str){
			continue;
		}

		if(memcmp(buf2,"SCREEN_RESO",11) ==0){
			val = atoi(str);

			if(val == 1280){
				width = val;
				height = 720;
			}else if(val == 1920){
				width = val;
				height = 1080;
			}else if(val == 3840){
				width = val;
				height = 2160;
			}else{
				fprintf(stderr,"Warning : Invalid value for screen resolution.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"CAMERA_DEVICE",13) ==0){
			if(strcmp(str,"AUTO") == 0){
				camera_device[0] = 0x00;
			}else{
				my_strncpy(camera_device,str,sizeof(camera_device));
			}
		}else if(memcmp(buf2,"CAMERA_RESO",11) ==0){
			val = atoi(str);
			if(val == 1280){
				camera_width = val;
				camera_height = 720;
			}else if(val == 1920){
				camera_width = val;
				camera_height = 1080;
			}else if(val == 3840){
				camera_width = val;
				camera_height = 2160;
			}else{
				fprintf(stderr,"Warning : Invalid value for camera resolution.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"CAMERA_COMPRESS",15) ==0){
			if((memcmp(str,"YUV422",5) == 0)){
				camera_compress = COMPRESS_YUV422;
			}else if((memcmp(str,"MJPEG",5) == 0)){
				camera_compress = COMPRESS_MJPEG;
			}else{
				fprintf(stderr,"Warning : Invalid value for camera compress.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"CAMERA_IMAGE_QUALITY",20) ==0){
			val = atoi(str);
			if((val < 0) || (100 < val)){
				fprintf(stderr,"Warning : Invalid value for image quality.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
			ImageQuality = val;
		}else if(memcmp(buf2,"MAX_SEND_FPS",12) ==0){
			val = atoi(str);
			if((val < 0) || (200 < val)){
				fprintf(stderr,"Warning : Invalid value for Max send fps.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
			MaxSendFps = val;
		}else if(memcmp(buf2,"CAPTURE_FPS",10) ==0){
			val = atoi(str);
			if((val < 1) || (200 < val)){
				fprintf(stderr,"Warning : Invalid value for captute fps.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
			CaptureFps = val;
		}else if(memcmp(buf2,"CAPTURE_QUALITY",15) ==0){
			val = atoi(str);
			if((val < 0) || (100 < val)){
				fprintf(stderr,"Warning : Invalid value for captute quarity.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
			CaptureQuarity = val;
		}else if(memcmp(buf2,"IGNORE_KEY_SHARING",18) ==0){
			if((memcmp(str,"ON",2) == 0)){
				Ignoreflg = TRUE;
			}else if((memcmp(str,"OFF",3) == 0)){
				Ignoreflg = FALSE;
			}else{
				fprintf(stderr,"Warning : Invalid value for ignore flag.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"MULTI_MODE",10) ==0){
			if((memcmp(str,"ON",2) == 0)){
				 mode = MODE_MULTI;
			}else if((memcmp(str,"OFF",3) == 0)){
				mode = MODE_SINGLE;
			}else{
				fprintf(stderr,"Warning : Invalid value for multi-mode flag.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"ALLOW_NO_CRYPTO",15) ==0){
			if((memcmp(str,"ON",2) == 0)){
				 allow_no_crypto = 1;
			}else if((memcmp(str,"OFF",3) == 0)){
				 allow_no_crypto = 0;
			}else{
				fprintf(stderr,"Warning : Invalid value for allow no crypto flag.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"MAX_ADDRESS",10) ==0){
			val = atoi(str);
			if((val < 1) || (MAX_KEY_LIST < val)){
				fprintf(stderr,"Warning : Invalid value for max address.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}

			max_key_list_no = val;
		}else if(memcmp(buf2,"SOUND_CLI",8) ==0){
			val = atoi(str);
			if((val < 1) || (32 < val)){
				fprintf(stderr,"Warning : Invalid value for sound effective threads.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
			max_sound_threads = val;
		}else if(memcmp(buf2,"PINP_DISP_MODE",14) ==0){
			if((memcmp(str,"TEXT",4) == 0)){
				PinPDispMode = DISP_TEXT;
			}else if((memcmp(str,"IMAGE",5) == 0)){
				PinPDispMode = DISP_IMAGE;
			}else{
				fprintf(stderr,"Warning : Invalid value for PinP diplay mode.(file=%s line=%d)\n",__FILE__,__LINE__);
				rv = 1;
				continue;
			}
		}else if(memcmp(buf2,"KEY_FILE_DIR",12) == 0){
			if((*str == '"') && (*(str + strlen(str) -1) == '"')){
				*(str + strlen(str) - 1) = 0;
				if(strlen(str) > 257){
					fprintf(stderr,"Warning : Too long key file dir.(file=%s line=%d)\n",__FILE__,__LINE__);
					rv = 1;
					*(str+1) = 0x00;
				}
				my_strncpy(key_file_dir,str+1,sizeof(key_file_dir));
			}
		}else if(memcmp(buf2,"REC_FILE_DIR",12) == 0){
			if((*str == '"') && (*(str + strlen(str) -1) == '"')){
				*(str + strlen(str) - 1) = 0;
				if(strlen(str) > 257){
					fprintf(stderr,"Warning : Too long rec file dir.(file=%s line=%d)\n",__FILE__,__LINE__);
					rv = 1;
					*(str+1) = 0x00;
				}
				my_strncpy(rec_file_dir,str+1,sizeof(rec_file_dir));
			}
		}else if(memcmp(buf2,"ADDR_BOOK_FILE",14) == 0){
			if((*str == '"') && (*(str + strlen(str) -1) == '"')){
				*(str + strlen(str) - 1) = 0;
				if(strlen(str) > 257){
					fprintf(stderr,"Warning : Too long address book file.(file=%s line=%d)\n",__FILE__,__LINE__);
					rv = 1;
					*(str+1) = 0x00;
				}
				my_strncpy(addr_book_file,str+1,sizeof(addr_book_file));
			}
		}
	}

	return rv;
}
static	int	allocate_mem(void)
{
	init_image_buf = NULL;
	edit_buffer = NULL;
	edit_buffer2 = NULL;
	edit_buffer3 = NULL;
	dest_buffer = NULL;
	dest_buffer2 = NULL;
//	sA = NULL;
	FileBuffer1 = NULL;
	FileBuffer2 = NULL;
	readbuf = NULL;
	readbuf2 = NULL;
	jpeg_scanline = NULL;
	jpeg_scanline2 = NULL;
	cmdbuf = NULL;
	selected_list = NULL;


	init_image_buf = calloc(1,MAX_JPEG_SIZE);
	if(init_image_buf == NULL){
		return -1;
	}
	edit_buffer = calloc(1,EDIT_BUF_SIZE);
	if(edit_buffer == NULL){
		return -1;
	}
	edit_buffer2 = calloc(1,EDIT_BUF_SIZE);
	if(edit_buffer2 == NULL){
		return -1;
	}
	edit_buffer3 = calloc(1,EDIT_BUF_SIZE);
	if(edit_buffer3 == NULL){
		return -1;
	}
	dest_buffer = calloc(1,EDIT_BUF_SIZE + 32);
	if(dest_buffer == NULL){
		return -1;
	}
	dest_buffer2 = calloc(1,EDIT_BUF_SIZE + 32);
	if(dest_buffer2 == NULL){
		return -1;
	}
	FileBuffer1 = calloc(1,FILE_BUF_SIZE);
	if(FileBuffer1 == NULL){
		return -1;
	}
	FileBuffer2 = calloc(1,FILE_BUF_SIZE);
	if(FileBuffer2 == NULL){
		return -1;
	}
	readbuf = calloc(1,2048);
	if(readbuf == NULL){
		return -1;
	}
	readbuf2 = calloc(1,2048);
	if(readbuf2 == NULL){
		return -1;
	}
	jpeg_scanline = calloc(1,SCAN_LINE_SIZE);
	if(jpeg_scanline == NULL){
		return -1;
	}
	jpeg_scanline2 = calloc(1,SCAN_LINE_SIZE);
	if(jpeg_scanline2 == NULL){
		return -1;
	}
	cmdbuf = calloc(1,CMD_BUF_SIZE);
	if(cmdbuf == NULL){
		return -1;
	}
	selected_list = calloc(1,max_key_list_no*64);
	if(selected_list == NULL){
		return -1;
	}

	return 0;
}
static	void	OKDialog(char *message)
{
	GtkWidget 		*dialog,*label;

	dialog = gtk_dialog_new_with_buttons(gettext("Warning"), GTK_WINDOW(window),
	       	GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, gettext("OK"),
		GTK_RESPONSE_YES,NULL);
	label = gtk_label_new(message);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))) ,label);
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}
/*****************************************************************************************/
/* 以下はメインスレッド*/
/*****************************************************************************************/
int main(int argc, char *argv[])
{
	int			fd,rv,rv2;
	struct sigaction 	sa;
	sigset_t                block;
	char			buf[512];
	FILE			*fp;
//
// 多重起動防止
//
	if ((fd = open("./lock",O_WRONLY)) == -1){
		fprintf(stderr,"lock file not found.(file=%s line=%d)\n",__FILE__,__LINE__);
		usleep(1000*1000*3);
		return -1;
	}
	if(flock(fd,LOCK_EX | LOCK_NB) == -1){
		fprintf(stderr,"Another process found.(file=%s line=%d)\n",__FILE__,__LINE__);
		usleep(1000*1000*3);
		return -1;
	}
//
// シグナルハンドラの登録
//
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigemptyset(&block);
	sigaddset(&block,SIGSEGV);
	sa.sa_mask = block;
	sa.sa_sigaction = sig_handler;
	if (sigaction(SIGSEGV, &sa, NULL) == -1){
		fprintf(stderr,"signal hander error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}

	sigaddset(&block,SIGINT);		// Ctrl+C
	sa.sa_mask = block;
	sa.sa_sigaction = sig_handler;
	if (sigaction(SIGINT, &sa, NULL) == -1){
		fprintf(stderr,"signal hander error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}

	sigaddset(&block,SIGTERM);
	sa.sa_mask = block;
	sa.sa_sigaction = sig_handler;
	if (sigaction(SIGTERM, &sa, NULL) == -1){
		fprintf(stderr,"signal hander error.(file=%s line=%d)\n",__FILE__,__LINE__);
	}
//
// ロケールの設定
//
	setlocale(LC_ALL,"");
	bindtextdomain("IHAPHONE","/usr/share/locale");
	textdomain("IHAPHONE");

	rv = set_params();
	if(rv == -1){
		fprintf(stderr,"ihaphone-params.txt file not found.(file=%s line=%d)\n",__FILE__,__LINE__);
		usleep(1000*1000*3);
		return -1;
	}else if(rv > 0){
		;
	}

	rv2 = init_key_list();
	if(rv2 == -1){
		fprintf(stderr,"Key list init error.(file=%s line=%d)\n",__FILE__,__LINE__);
		usleep(1000*1000*3);
		return -1;
	}
//
// Xのマルチスレッドの初期化
//
	XInitThreads();
//
// GTKの初期化
//
	gtk_init(&argc, &argv);

	if(allocate_mem()){
		fprintf(stderr,"Out of memory.(file=%s line=%d)\n",__FILE__,__LINE__);
		usleep(1000*1000*3);
		return -1;
	}

	set_ent_tables();

	init_auth_code(MessAuthCode);

	set_area(1);


	if(rv > 0){
		OKDialog(gettext("There is a defect in the ihaphone-params.txt file.\n"));
	}
	if(rv2 > 0){
		sprintf(buf,gettext("There is a defect in the address book(%s).\n"),addr_book_file);
		OKDialog(buf);
	}
//
// ディレクトリの確認
//
//	sprintf(buf,"ls %s/*",key_file_dir);
//	if(my_system(buf) == -1){
	sprintf(buf,"%s/.",key_file_dir);
	if((fp = fopen(buf,"r")) == NULL){
		OKDialog(gettext("No file permissions.\nCheck the setting of KEY_FILE_DIR in the parameter file.\nAlso check the directory permissions.\n"));
	}
	if(fp){
		fclose(fp);
	}

//	sprintf(buf,"ls %s/*",rec_file_dir);
//	if(my_system(buf) == -1){
	sprintf(buf,"%s/.",rec_file_dir);
	if((fp = fopen(buf,"r")) == NULL){
		OKDialog(gettext("No file permissions.\nCheck the setting of REC_FILE_DIR in the parameter file.\nAlso check the directory permissions.\n"));
	}
	if(fp){
		fclose(fp);
	}


	handle2 = init_sound(SND_PCM_STREAM_CAPTURE);
	if(handle2 == NULL){
		fprintf(stderr,"init_sound error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	printf("chunk_size=%ld chunk_bytes=%ld\n",chunk_size,chunk_bytes);

	if(pthread_create(&tid9, NULL, wait_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid8, NULL, sess_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid7, NULL, ring_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid6, NULL, ref_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid5, NULL, sound_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid4, NULL, video_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid3, NULL, timer_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid2, NULL, fps_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}
	if(pthread_create(&tid1, NULL, recv_thread, (void *)NULL) != 0){
		fprintf(stderr,"pthread_create error.(file=%s line=%d)\n",__FILE__,__LINE__);
		return -1;
	}

	gtk_main();

	return 0;
}
