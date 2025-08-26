#include "rabbitmq-c.h"
void main(){
    //todo 设置缓冲类型 无缓冲
    setbuf(stdout, NULL);

    //todo 初始化锁、条件变量、barrier
    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond_running, NULL);
    pthread_cond_init(&cond_stop, NULL);
    pthread_cond_init(&cond_exit,NULL);

    //todo 启动任务
    rabbitmq_start_consumers();
    rabbitmq_start_producers();

    //todo 等待同步

    //todo 如果某个线程出现异常并结束
    {
        warn("main: get lock");
        pthread_mutex_lock(&mutex);
        warn("main: locked");

        work_status=0;
        //todo 等待 运行
        while(flag_running!=1){
            warn("main: wait cond_running,work_status=%d",work_status);
            pthread_cond_wait(&cond_running, &mutex);
        }
        warn("main: 1");
        work_status=1;

        //todo 等待 stop
        while(flag_stop!=1){
            warn("main: wait cond_stop,work_status=%d",work_status);
            pthread_cond_wait(&cond_stop,&mutex);
        }
        warn("main: 2");
        work_status=2;

        warn("main: close all threads,work_status=%d",work_status);

        //todo 等待 所有子线程结束任务 exit
        while(flag_exit!=1){
            warn("main: wait cond_exit,work_status=%d",work_status);
            pthread_cond_wait(&cond_exit,&mutex);
        }
        warn("main: 3");
        work_status=3;

//        //todo 等待 terminated
//        while(work_status==3){
//            warn("main: wait cond_terminated,work_status=%d",work_status);
//            pthread_cond_wait(&cond_terminated,&mutex);
//        }

        warn("main: release lock,work_status=%d",work_status);
        pthread_mutex_unlock(&mutex);
        warn("main: unlocked,work_status=%d",work_status);


        //todo 打印退出信息
        log_threads_exitInfo();

    }
    //todo 释放锁和条件变量资源
    pthread_cond_destroy(&cond_stop);
    pthread_mutex_destroy(&mutex);
}