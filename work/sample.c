/*ver.からの改造 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <malloc.h>

#define MAX_CHILD_NUM   10

#define LOCK            -1
#define UNLOCK          1
#define WAIT            0

#define ERR_TIMER       60

#define PIPE_READ       0
#define PIPE_WRITE      1

#define SIGNAL          666

/* 定数 */
const char *OP_RW = "w+";                //新規読み書き
const char *OP_EXT_W = "a";              //追加書き込み
const char *use_file = "exec4.txt";      //利用ファイル
const char *int_file = "int_data.txt";

/* 変数定義 */
union semun{
    int             val;                /* SETVALの値 */
    struct          semid_ds *buf;      /* IPC_STAT, IPC_SET用の配列 */
    unsigned short  *array;             /* GETALL, SETALL用の配列 */
    };

int int_count = 0;


/*= 関数プロトタイプ宣言 =*/
void MySemop( int p_semid, int SemOp, int i, int pid );
FILE* FileOp( const char *file_name, const char *opr, int pid, int i );
void FileCl( FILE *file_name, int pid, int i);
static void func_int();
static void exit_func();
static void raise_func();

/* シグナル受信の検証 */
int main()
{
    pid_t child_pid[MAX_CHILD_NUM];
    pid_t result_pid[MAX_CHILD_NUM];
    char buf[2][1024];
    char str[4][1024], str2[4][1024];
    FILE *write_file, *write_file_int;
    union semun ctrl_arg;
    int semid, i, code,t;
    struct timeval start_time, time1, time2;
    int process_com[2];
    char p_buf[1024];
    struct sigaction act, act2;
    char *mall, *work_mall;
    int data_size;

    /* 開始時間の取得 */
    gettimeofday(&start_time, NULL);

    /* メモリ領域の確保 */
    mall = (char*)malloc(sizeof(char) * 100);
    if(mall == NULL ){     //返り値が*voidなので必要な形にキャストする
        perror("main :malloc");
        _exit(EXIT_FAILURE);
    }

    data_size = malloc_usable_size(mall);
    work_mall = mall;           //メモリを開放する際に元の位置の情報がいるため、別に作業用の変数を確保する

    /* シグナルハンドラの作成 */
    memset(&act, 0, sizeof act);    //シグナルハンドラ用構造体の初期化
    act.sa_handler = func_int;
    if( sigaction(SIGCHLD, &act, NULL) < 0 ){        //子プロセスが終わるたびに関数の呼び出し
        perror("main :signal");
        _exit(EXIT_FAILURE);
    }

    memset(&act2, 0, sizeof act2);    //シグナルハンドラ用構造体の初期化
    act2.sa_handler = raise_func;
    if( sigaction(SIGUSR1, &act2, NULL) < 0 ){        //子プロセスが終わるたびに関数の呼び出し
        perror("main :signal raise");
        _exit(EXIT_FAILURE);
    }


    /* 終了関数の登録 */
    if( atexit(exit_func) != 0 ){       //左記の関数の引数に別の関数を与えると、プログラム終了時に実行される
        perror("main: end func");
    }

    /* パイプの作成 */
    if( (pipe(process_com)) <0 ){
        perror("main: pipe");
        _exit(EXIT_FAILURE);
    }

    /* セマフォの作成 */
    if( (semid = semget(IPC_PRIVATE, 1, 0600))  == -1 ){
        perror("main : semget");
        _exit(EXIT_FAILURE);
    }

    /* セマフォの初期値設定 */
    ctrl_arg.val = 1;       /* セマフォの値を設定 */
    if( semctl(semid, 0, SETVAL, ctrl_arg) == -1 ){
        perror("main :semctrl");
        _exit(EXIT_FAILURE);
    }

    data_size = malloc_usable_size(mall);
    /* 標準入力からの文字入力 */
    if( fgets(mall, data_size, stdin) == NULL ){
        perror("main :fgets");
        _exit(EXIT_FAILURE);
    }

    /* 入力文字に改行コードがあるかどうかの確認 */
    if(strchr(mall, '\n') != NULL){
        mall[data_size -1] = '\0';
        printf("####\n");
    }
    else{
        while(getchar() != '\n');
        printf(">>>>\n");
    }


    printf("size = %d\n", data_size);

    write_file = FileOp(use_file, OP_RW, 999, getpid());
    write_file_int = FileOp(int_file, OP_RW, 777, getpid());
    fprintf(write_file, "★  %s\n", mall);
    fprintf(write_file, "★ exec4 START!---------->\n");
    FileCl(write_file, 999, getpid());
    free(mall);                 //mallocを使ったあとはfreeでメモリを必ず開放すること

    /* プロセスの作成*/
    for( i = 0; i < MAX_CHILD_NUM ; i++ ){

        if( !(result_pid[i] = fork()) ){
            child_pid[i] = getpid();
            code = i;

            close(process_com[PIPE_READ]);
//排他制御
            MySemop( semid, LOCK, i, getpid() );
            write_file = FileOp(use_file, OP_EXT_W, i, getpid());

            fprintf( write_file, "★ process.\tpid = %d.\tmy ppid = %d \ti =%d \n", getpid(), getppid(),i );
            fprintf( write_file, "★ result_pid[%d] = %d\n", i, result_pid[i]);

            FileCl(write_file, i, getpid());
            MySemop( semid, UNLOCK, i, getpid() );
//排他制御

            sleep(1);
            break;
        }
        else if( result_pid[i] == -1 ){
            printf("fork failed.\n");
            _exit(-1);
        }
        else{
//排他制御
            MySemop( semid, LOCK, i, getpid() );
            write_file = FileOp(use_file, OP_EXT_W, i, getpid());

            fprintf( write_file, "● process.\tpid = %d.\tmy ppid = %d \ti =%d \n", getpid(), getppid(),i );
            fprintf( write_file, "● result_pid[%d] = %d\n", i, result_pid[i]);
            code = i;

            FileCl(write_file, i, getpid());
            MySemop( semid, UNLOCK, i, getpid() );
//排他制御

            sleep(1);
        }
    }

    if(result_pid[code] != 0){
        /* 親プロセス */
        int status;
        int child_pid;
        int n,timer;
        i=0;

        gettimeofday(&time1, NULL);


        MySemop( semid, LOCK, 999, getpid() );
        write_file = FileOp(use_file, OP_EXT_W, 999, getpid());
        fprintf(write_file, "parent processs\n");            
        FileCl(write_file, 999, getpid());
        MySemop( semid, UNLOCK, 999, getpid() );

//        close(process_com[PIPE_WRITE]);

        while(i < MAX_CHILD_NUM){
            child_pid = waitpid(-1, &status, WNOHANG);
            raise(SIGUSR1);     //シグナルの立ち上げ(ユーザー独自で立ち上げる場合は、"SIGUSR1 or "SIGUSR2"を使うこと)
            MySemop( semid, LOCK, 999, getpid() );
            write_file = FileOp(use_file, OP_EXT_W, 999, getpid());
            
            if(child_pid > 0){
                i++;
                fprintf(write_file, "PID %d done\n", child_pid);
            }
            else{
                fprintf(write_file, "No child exited\n");
            }


            if( child_pid == result_pid[5] ){
                n = read(process_com[PIPE_READ], p_buf, sizeof p_buf);
                if( n < 0 ){
                    perror("main: pipe_read");
                    _exit(EXIT_FAILURE);
                }
                else if( n > 0 ){
                    fprintf(write_file, "▼  %s", p_buf);
                    close(process_com[PIPE_READ]);
                }
            }

            FileCl(write_file, 999, getpid());

            printf("=");
            sleep(1);


            gettimeofday(&time2, NULL);
            timer = time2.tv_sec - time1.tv_sec;

            if( timer > ERR_TIMER ){
                printf("\ntime up");
                perror("time out!\n");
                _exit(EXIT_FAILURE);
            }

            MySemop( semid, UNLOCK, 999, getpid() );

        }
    }
    else{
        /* 子プロセス */
        pid_t own_pid;
        pid_t parent_pid;
        int sleeptime;

        own_pid = getpid();
        parent_pid = getppid();
        sleeptime = (rand() %9) * 10;


        MySemop( semid, LOCK, code, getpid() );
        write_file = FileOp(use_file, OP_EXT_W, code, own_pid);

        switch(code){
            case 0: 
                fprintf(write_file, "test_0: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                sleep(sleeptime);   
                _exit(1);
                break;
            
            case 1:
                fprintf(write_file, "test_1: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                sleep(sleeptime);   
                _exit(1);
                break;
            
            case 2:
                fprintf(write_file, "test_2: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                sleep(sleeptime);  
                _exit(1);
                break;
            
            case 3:
                fprintf(write_file, "test_3: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                sleep(sleeptime);  
                _exit(1);
                break;
            
            case 4:
                fprintf(write_file, "test_4: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                if( execl("/home/maedi/デスクトップ/work/study4/test_3", "/home/maedi/デスクトップ/work/study4/test_3", NULL) == -1 ){
                    printf("error:execl()");
                    _exit(-1);
                }
                break;
            
            case 5:
                sprintf(p_buf, "test_5: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                write(process_com[PIPE_WRITE], p_buf, strlen(p_buf)+1);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                sleep(sleeptime);  
                _exit(1);
                break;
            
            default:
                fprintf(write_file, "unknown: %d\t own= %d\t parent = %d\n", code, own_pid, parent_pid);
                FileCl(write_file, code, own_pid);
                MySemop( semid, UNLOCK, code, own_pid );
                sleep(sleeptime);  
                _exit(1);
                break;
        }
    }

    /* セマフォを削除 */
    if( semctl(semid, 0, IPC_RMID, ctrl_arg) == -1 ){
        perror("main : semctl");
        _exit(EXIT_FAILURE);
    }

    write_file = FileOp(use_file, OP_EXT_W, 999, getpid());
    fprintf(write_file,"END------------>\n");
    FileCl(write_file, 999, getpid());
    FileCl(write_file_int, 777, getpid());

    gettimeofday(&time2, NULL);
    printf("passed time: %ld\n", time2.tv_sec - start_time.tv_sec);
    printf("debug\n");
    printf("exec4 END!!!!\n");

    return 0;

}


/* セマフォ操作用関数 */
void MySemop( int p_semid, int SemOp, int i, int pid )
{
    struct sembuf   sops[1];

    sops[0].sem_num = 0;
    sops[0].sem_op = SemOp;
    sops[0].sem_flg = 0;

    if( semop(p_semid, sops, 1) == -1 ){
        printf("i = %d \t, pid = %d\n", i, pid );
        perror("MySemop");
        _exit(EXIT_FAILURE);
    }
    return;
}

/*ファイルオープン用関数*/
FILE* FileOp( const char *file_name, const char *opr, int pid, int i )
{
    FILE *fp;

    if( (fp = fopen(file_name, opr)) == NULL ){     //"a"で追加書き込み
        printf("i = %d \t pid = %d", i, pid);
        perror("file_not open\n");
        _exit(EXIT_FAILURE);
    }

    return fp;
}

/*ファイルクローズ用関数*/
void FileCl( FILE *fp, int pid, int i)
{
    if( fclose(fp) == EOF ){
        printf("i = %d\t pid = %d\n", i, pid);
        printf("file can not close\n");
    }
}

/* シグナル処理 */
static void func_int()
{
    FILE *fp;

    fp = FileOp(int_file, OP_EXT_W, 777, getpid());
    int_count++;
    fprintf(fp, "◎ child_end count = %d\n", int_count);
    FileCl( fp, 777, getpid());
}


/* 終了関数 */
static void exit_func()
{
    printf("テスト終了\n");
}

static void raise_func()
{
    printf("Raise!!!\n");
}
