#include <bits/types/struct_timeval.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "rabbitmq-c.h"
//#include "stdio.h"
#include "time.h"
pthread_mutex_t lock1;
pthread_cond_t cond1;
int flag=0;
void *test(void * args){
//    char buf[10];
//    warn("thread: start");
//    fgets(buf,10,stdin);


    pthread_mutex_lock(&lock1);
    warn("thread: get lock ");

    flag=1;
//    pthread_cond_broadcast(&cond1);
    warn("thread: pre signal");
//    print_synchronized_info(&lock1,&cond1);

//    pthread_cond_signal(&cond1);
    pthread_cond_wait(&cond1,&lock1);

    warn("thread: signal");
//    print_synchronized_info(&lock1,&cond1);

    pthread_mutex_unlock(&lock1);

//    warn("thread: input");
//    fgets(buf,10,stdin);

    pthread_mutex_lock(&lock1);
    warn("thread: get lock ");
    pthread_mutex_unlock(&lock1);

    return NULL;
}
void release_resources(void *arg){
    info("thread exit");
    pthread_mutex_t *p_lock = (pthread_mutex_t *) arg;
    if( p_lock->__data.__owner==syscall(SYS_gettid)){
        warn("clean: unlock");
        pthread_mutex_unlock(p_lock);
    }
}
void *test01(void * args){
    pthread_cleanup_push(release_resources,&lock1);

    pthread_mutex_lock(&lock1);
    info("thread wait");

//    pthread_cond_wait(&cond1,&lock1);
//    sleep(100);

    info("thread wake up");

    pthread_mutex_unlock(&lock1);
    info("thread unlock");

    pthread_cleanup_pop(0);

    return NULL;
}
int main(){

//todo rabbitmq客户端程序
    {
        rabbitmq_start_client();
    }

//todo 验证：
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
////            warn("main: unlock 1");
////            pthread_mutex_unlock(&lock1);
////            print_synchronized_info(&lock1,&cond1);
//
////            struct timespec timeout;
////
////            while(flag!=1){
////                timeout.tv_sec=time(NULL)+5;
////                warn("main: wait");
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


int wait_ack(taskInfo_t *taskInfo) {
    amqp_publisher_confirm_t result = {0};
    struct timeval timeout = {0, 0};

    amqp_connection_state_t connState = rabbitmqConnsInfo.conns[producersInfo.producers[taskInfo->index].conn_index].connState;

    for (;;) {

        amqp_maybe_release_buffers(connState);

        //todo 非阻塞
        amqp_rpc_reply_t ret = amqp_publisher_confirm_wait(
            connState,
            &timeout,
            &result
        );

        //todo 异常处理
        if (AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type) {
            //todo 收到非ack帧
            if (AMQP_STATUS_UNEXPECTED_STATE == ret.library_error) {
                taskInfo->execInfo.info="AMQP_STATUS_UNEXPECTED_STATE is not ack frame";
            }
            //todo 等待确认已超时
            else if (AMQP_STATUS_TIMEOUT == ret.library_error) {
                // Timeout means you're done; no publisher confirms were waiting!
                taskInfo->execInfo.info="AMQP_STATUS_TIMEOUT";
            }
            return -1;
        }


        //todo 需要检测该任务是否被通知stop
        if(work_status==4){
            return 1;//todo 非阻塞等待确认，处于死循环中需要实时检测Rabbitmq的工作状态
        }

        //todo 处理rabbitmq服务发来的响应
        {
            switch (result.method) {
                case 0:
                    //todo 非阻塞等待确认，还未收到任何消息
                    break;
                case AMQP_BASIC_ACK_METHOD:
                    //todo 来自rabbitmq服务的ack
                    return 0;
                case AMQP_BASIC_RETURN_METHOD:
                    //mandatory=1时,消息会被服务器发回到生产者
                    taskInfo->execInfo.info="wait confirm but AMQP_BASIC_RETURN_METHOD";
                    return -1;
                case AMQP_BASIC_REJECT_METHOD:
                    taskInfo->execInfo.info="wait confirm but AMQP_BASIC_REJECT_METHOD";
                    return -1;
                case AMQP_BASIC_NACK_METHOD:
                    taskInfo->execInfo.info="wait confirm but AMQP_BASIC_NACK_METHOD";
                    return -1;
                case AMQP_CHANNEL_CLOSE_METHOD:
                    taskInfo->execInfo.info="wait confirm but AMQP_CHANNEL_CLOSE_METHOD";
                    return -1;
                case AMQP_CONNECTION_CLOSE_METHOD:
                    taskInfo->execInfo.info="wait confirm but AMQP_CONNECTION_CLOSE_METHOD";
                    return -1;
                default:
                    taskInfo->execInfo.info="wait confirm but unknown frame type";
                    return -1;
            };
        }
    }
}
void *rabbitmq_task(void * arg){

    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
    //todo 注册退出回调函数
    pthread_cleanup_push(clean_synchronize_resources,&lock1);

    //todo 通知 线程已就绪
    task_notify_main_run();
    {
        while(1){
            int conn_index;
            int channel_index;

            //0-生产者 1-消费者
            if(taskInfo->type==0){
                conn_index=producersInfo.producers[taskInfo->index].conn_index;
                channel_index=producersInfo.producers[taskInfo->index].channel_index;
            }
            else{
                conn_index=consumersInfo.consumers[taskInfo->index].conn_index;
                channel_index=consumersInfo.consumers[taskInfo->index].channel_index;
            }
            //todo 0-就绪
            if(work_status==0){
                sleep(1);
                info("thread producer[%d]:wait running",taskInfo->index);
                continue;
            }
            //todo 1-运行
            else if(
                work_status==1    //todo 1-running
                || work_status==2 //todo 2-conn重置
                || work_status==3 //todo 3-channel重置
            ){
                //todo （被动）当检测到main状态处于重置时 确保任务所对应的连接是可用的
                if(work_status==2){
                    task_wait_main_reset_conn(conn_index);
                }
                else if(work_status==3){
                    task_wait_main_reset_channel(conn_index, channel_index);
                }
                //todo 业务处理
                taskInfo->execInfo.code = taskInfo->task(taskInfo);//任务只需要修改连接状态

                //todo 执行结果处理
                if(taskInfo->execInfo.code == 0){
                    continue;
                }
                else if(
                    taskInfo->execInfo.code == -1   //执行异常
                    || taskInfo->execInfo.code == 1 //被通知停止 生产者ack在非阻塞等待
                ){
                    //todo 通知main 线程已停止
                    task_notify_main_stop(taskInfo);
                    break;
                }
                //todo （主动）通知main 连接重置
                else if(
                    taskInfo->execInfo.code == 3    //3-连接已关闭
                ){
                    task_notify_main_reset_conn(conn_index);
                }
                //todo （主动）通知main 通道重置
                else if(
                    taskInfo->execInfo.code == 4    //4-通道已关闭
                ){
                    task_notify_main_reset_channel(conn_index,channel_index);
                }
            }
            //todo 4-stop
            else if(work_status==4){
                taskInfo->execInfo.code=0;
                taskInfo->status=5;//5-结束

                //0-生产者 1-消费者
                if(taskInfo->type==0){
                    info("thread producer[%d]:stopped",taskInfo->index);
                }
                else if(taskInfo->type==1){
                    info("thread consumer[%d]:stopped",taskInfo->index);
                }
                //todo 退出前处理
                {
                    //todo 停止前进行资源释放
                    if(taskInfo->execInfo.info == NULL){
                        taskInfo->execInfo.info=get_code_info(taskInfo->execInfo.code);
                    }
                }

                //todo 通知任务已全部结束
                task_notify_main_exit();
                break;
            }
        }
    }

    //todo 清除回调
    pthread_cleanup_pop(0);

    return NULL;
}

//todo 保证该函数能够处于一个while(1)并被调用
void *rabbitmq_consumer_task(void * arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    switch (taskInfo->status) {
        //0-等待中
        case 0:{

            break;
        }
        //1-发布中
        case 1: {
            break;
        }
        //2-确认中
        case 2: {
            break;
        }
        //3-发送完
        case 3: {
            break;
        }
        //4-闲置状态
        case 4: {
            
        }
        default: {
            break;
        }
    }
}
void *rabbitmq_producer_task(void * arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
    producerEntity_t_t producer = producersInfo.producers[taskInfo->index];
    switch (taskInfo->status) {
        //0-等待中
        case 0:{
            taskInfo->execInfo.code=0;

            //todo 状态修改
            //收到需要数据
            if(){
                taskInfo->status=1;
            }
            break;
        }
        //1-发布中
        case 1: {
            //todo 状态修改
            //已发送且需要等待确认
            if(){
                taskInfo->status=2;
            }
            //已发送无需等待确认
            else if(){
                taskInfo->status=3;
            }
            break;
        }
        //2-发布确认中
        case 2: {
            //确认失败
            if(){
                
            }
            //确认成功
            else{
                
            }
            break;
        }
        //3-发布结果处理中
        case 3: {
            break;
        }
        //4-闲置状态
        case 4: {
            if(taskInfo->execInfo.code==2){//重置连接
                //等待 连接 或 连接 均无问题
                if(
                    rabbitmqConnsInfo.conns[producer.conn_index].status==5
                    &&rabbitmqConnsInfo.conns[producer.conn_index].channelsInfo.channels[producer.channel_index].status==2
                ){
                    taskInfo->status=0;
                }
            }
            if(taskInfo->execInfo.code==1){//1-被通知暂停执行

            }
        }
        default: {
            break;
        }
    }
}

