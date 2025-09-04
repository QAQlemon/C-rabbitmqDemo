#include <bits/types/struct_timeval.h>
#include <unistd.h>
#include "rabbitmq-c.h"
//#include "stdio.h"
#include "time.h"
pthread_mutex_t lock1;
pthread_cond_t cond1;
int flag=0;
void *test(void * args){
    char buf[10];
    warn("thread: start");
    fgets(buf,10,stdin);


    pthread_mutex_lock(&lock1);
    warn("thread: get lock ");

    flag=1;
//    pthread_cond_broadcast(&cond1);
    warn("thread: pre signal");
    print_synchronized_info(&lock1,&cond1);

    pthread_cond_signal(&cond1);
//    pthread_cond_wait(&cond1,&lock1);

    warn("thread: signal");
    print_synchronized_info(&lock1,&cond1);

    pthread_mutex_unlock(&lock1);
    return NULL;
}
int main(){
    {
//        rabbitmq_start_client();
    }
    {

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&lock1, &attr);

//        pthread_mutex_init(&lock1,NULL);

        pthread_cond_init(&cond1, NULL);


        pthread_t t;
        pthread_create(&t,NULL,test,NULL);

        //gdb调试
        //查看锁持有情况：print lock.__data.__owner
        //查看条件变量：print cond

        {
            pthread_mutex_lock(&lock1);
            warn("main: get locked");
            print_synchronized_info(&lock1,&cond1);

            pthread_mutex_lock(&lock1);
            warn("main: get locked");
            print_synchronized_info(&lock1,&cond1);





            warn("main: unlock");
            pthread_mutex_unlock(&lock1);
            print_synchronized_info(&lock1,&cond1);

            pthread_cond_wait(&cond1,&lock1);
            warn("main: signaled");
            print_synchronized_info(&lock1,&cond1);

            warn("main: unlock");
            pthread_mutex_unlock(&lock1);
            print_synchronized_info(&lock1,&cond1);

        }
        warn("ok");
//            struct timespec timeout;
//            while(flag!=1){
//                clock_gettime(CLOCK_REALTIME, &timeout);
//                timeout.tv_sec += 5; // 等待5秒
//                warn("main: wait");
////                lock1.__data.__count==1;
//                pthread_cond_timedwait(&cond1, &lock1,&timeout);
//                print_synchronized_info(&lock1,&cond1);
//            }
    }
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
void *rabbitmq_task(void *arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;


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
                    taskInfo->execInfo.code == -1   //异常退出
                    || taskInfo->execInfo.code == 1 //被通知停止 生产者ack在非阻塞等待
                ){
                    //todo 通知main 线程已停止
                    task_notify_main_stop(taskInfo);
                    break;
                }
                //todo （主动）通知main 连接重置
                else if(
                    taskInfo->execInfo.code == 2    //重置连接
                ){
                    task_notify_main_reset_conn(conn_index);
                }
                //todo （主动）通知main 通道重置
                else if(
                    taskInfo->execInfo.code == 3    //重置通道
                ){
                    task_notify_main_reset_channel(conn_index,channel_index);
                }
            }
            //todo 4-stop
            else if(work_status==4){
                taskInfo->execInfo.code=1;
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
}
