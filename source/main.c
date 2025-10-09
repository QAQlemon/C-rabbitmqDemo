#include <bits/types/struct_timeval.h>
#include <unistd.h>
//#include <sys/syscall.h>
#include <malloc.h>
#include "stdio.h"
//#include "time.h"
#include "memory.h"

#include "rabbitmq-c.h"



int main(){

//todo rabbitmq客户端程序
    {
        char cmd[10]={0};
        sprintf(cmd,"%s","start");
        while(1){
            //todo 操作
            if(
                strcmp(cmd,"r")==0
                || strcmp(cmd,"start")==0
            ){
                printf("****************************\n");
                printf("client running\n");
                printf("****************************\n");
                rabbitmq_start_client();
            }
            else if(strcmp(cmd,"stop")==0){
                return 0;
            }
            else{
                printf("unknown cmd\n");
            }
//            //todo 重新获取操作指令
//            memset(cmd,0,10);
//            printf("input:");
//            fflush(stdout);
//            scanf("%s",cmd);

            sleep(2);
        }
    }

//    {
//        rabbitmq_start_client();
//    }

//todo 验证：线程退出时注册的资源释放函数
//    {
//
//        pthread_t t;
//        pthread_create(&t,NULL,test01,NULL);
//
//        char buf[10];
//        warn("main: input");
//        fgets(buf,10,stdin);
//
//        info("main: cancel thread");
//        pthread_cancel(t);
//
//        pthread_mutex_lock(&lock1);
//        info("main: get lock");
//
//        pthread_mutex_unlock(&lock1);
//    }

//todo 验证：可重入锁内层同步代码块中的条件变量wait操作导致的死锁现象
//    {
//
//        pthread_mutexattr_t attr;
//        pthread_mutexattr_init(&attr);
//        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
//        pthread_mutex_init(&lock1, &attr);
//
//        pthread_mutex_init(&lock1,NULL);
//
//        pthread_cond_init(&cond1, NULL);
//
//
//        pthread_t t;
//        pthread_create(&t,NULL,test,NULL);
//
//        //gdb调试
//        //查看锁持有情况：print lock.__data.__owner
//        //查看条件变量：print cond
//
//        {
//            pthread_mutex_lock(&lock1);
//            warn("main: get locked 1");
//            print_synchronized_info(&lock1,&cond1);
//
//            pthread_mutex_lock(&lock1);
//            warn("main: get locked 2");
//            print_synchronized_info(&lock1,&cond1);
//
////            warnLn("main: unlock 1");
////            pthread_mutex_unlock(&lock1);
////            print_synchronized_info(&lock1,&cond1);
//
////            struct timespec timeout;
////
////            while(flag!=1){
////                timeout.tv_sec=time(NULL)+5;
////                warnLn("main: wait");
////                print_synchronized_info(&lock1,&cond1);
////                pthread_cond_timedwait(&cond1,&lock1,&timeout);
////            }
//
//
//            warn("main: signaled");
//            print_synchronized_info(&lock1,&cond1);
//
//            warn("main: unlock");
//            pthread_mutex_unlock(&lock1);
//            print_synchronized_info(&lock1,&cond1);
//
//        }
//        warn("ok");
//    }
}











