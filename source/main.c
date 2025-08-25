#include "rabbitmq-c.h"
void main(){
    //todo 设置缓冲类型 无缓冲
    setbuf(stdout, NULL);

    //todo 初始化锁、条件变量、barrier
    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond_running, NULL);
    pthread_cond_init(&cond_stop, NULL);
    pthread_cond_init(&cond_exit,NULL);
    pthread_cond_init(&cond_terminated, NULL);
//    pthread_barrier_init(&barrier,NULL,producersInfo.size+consumersInfo.size);

    //todo 启动任务
    rabbitmq_start_consumers();
    rabbitmq_start_producers();

    //todo 等待同步

    //todo 如果某个线程出现异常并结束
    {
        warn("main:get lock");
        pthread_mutex_lock(&mutex);
        warn("main:locked");

        work_status=0;
        //todo 等待 运行
        while(work_status==0){

            warn("main: wait cond_running,work_status=%d",work_status);
            pthread_cond_wait(&cond_running, &mutex);
        }
        warn("main: 1");

        //todo 等待 stop
        while(work_status==1){
            warn("main: wait cond_stop,work_status=%d",work_status);
            pthread_cond_wait(&cond_stop,&mutex);
        }
        warn("main: 2");

        warn("main:close all threads,work_status=%d",work_status);

        //todo 通知 子线程结束任务 exit
        work_status=3;
        warn("main: signal cond_exit");
        pthread_cond_broadcast(&cond_exit);

        //todo 等待 terminated
        while(work_status==3){
            warn("main: wait cond_terminated,work_status=%d",work_status);
            pthread_cond_wait(&cond_terminated,&mutex);
        }

        warn("main:release lock,work_status=%d",work_status);
        pthread_mutex_unlock(&mutex);
        warn("main:unlocked,work_status=%d",work_status);


        warn("exitInfo:type=%d,index=%d,info=%s",
             exitInfo.type,exitInfo.index,exitInfo.info
        );

    }


}