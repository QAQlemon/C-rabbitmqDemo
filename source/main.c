#include <bits/types/struct_timeval.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <malloc.h>
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
//    {
//        rabbitmq_start_client();
//    }

    {
        rabbitmq_init_conns();


    }


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



    amqp_maybe_release_buffers(connState);

    //todo 限时等待
    timeout.tv_sec=10;
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
        return 6;
    }

    //todo 处理rabbitmq服务发来的响应
    {
        switch (result.method) {
            case 0:
                //todo 非阻塞等待确认，还未收到任何消息
                return 0;//0-正常处理(保持状态)
            case AMQP_BASIC_ACK_METHOD:
                //todo 来自rabbitmq服务的ack
                taskInfo->execInfo.code=0;
                return 1;//1-结果正常
//            case AMQP_BASIC_RETURN_METHOD:
//                //mandatory=1时,消息会被服务器发回到生产者
//                taskInfo->execInfo.info="wait confirm but AMQP_BASIC_RETURN_METHOD";
//                return 1;
            case AMQP_BASIC_REJECT_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_BASIC_REJECT_METHOD";
                return 6;//6-发布ACK失败
            case AMQP_BASIC_NACK_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_BASIC_NACK_METHOD";
                return 6;//6-发布ACK失败

            case AMQP_CHANNEL_CLOSE_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_CHANNEL_CLOSE_METHOD";
                return 3;//3-通道已关闭
            case AMQP_CONNECTION_CLOSE_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_CONNECTION_CLOSE_METHOD";
                return 2;//2-连接已关闭
            default:
                taskInfo->execInfo.info="wait confirm but unknown frame type";
                return 6;//6-发布ACK失败
        };
    }
}
void *rabbitmq_task(void * arg){

    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
    
    //todo 注册退出回调函数
    pthread_cleanup_push(clean_synchronize_resources,&lock1);

    //todo 通知 线程已就绪
    task_notify_main_run();

    while(1){
        //0-生产者 1-消费者
        if(taskInfo->type==0){
            rabbitmq_producer_deal(taskInfo);
        }
        else if(taskInfo->type==1){
            rabbitmq_consumer_deal(taskInfo);
        }
        else{
            break;
        }
    }

    //todo 清除回调
    pthread_cleanup_pop(0);
    return NULL;
}

//todo 保证该函数能够处于一个while(1)并被调用
void *rabbitmq_consumer_deal(void *arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
    consumerEntity_t consumer = consumersInfo.consumers[taskInfo->index];
    int conn_index = consumer.conn_index;
    int channel_index = consumer.channel_index;

    //todo 检查客户端运行状态
    {
        //todo 0-就绪
        if (work_status == 0) {
            sleep(1);
            info("thread producer[%d]:wait running", taskInfo->index);
            return NULL;
        }

            //todo 1-运行
//        if(work_status==1){
//            //无处理
//        }

        //todo 检查连接状态
        //（被动）当检测到main状态处于重置时 确保任务所对应的连接是可用的
        //todo 考虑改为限时等待（非阻塞）
        else if (work_status == 2) {
            task_wait_main_reset_conn(conn_index);
        } else if (work_status == 3) {
            task_wait_main_reset_channel(conn_index, channel_index);
        }
        //todo 4-stop
        else if (work_status == 4) {
            taskInfo->status = 4;
            taskInfo->execInfo.code=9;
        }
    }

    switch (taskInfo->status) {
        //0-闲置（等待main重置连接或通道）
        case 0:{

            break;
        }
        //1-等待中
        case 1: {
            int res=0;

            //分配空间
            taskInfo->execInfo.data=NULL;
            taskInfo->execInfo.data=malloc(255);

            //todo 获取消息
            res=get_an_message(taskInfo);
            //0-正常处理(保持状态)
            if(res==0){
                break;
            }
            //1-结果正常
            else if(res==1){
                taskInfo->status=2;//2-等待中
                break;
            }
            else{
                taskInfo->status=3;//3-异常处理
                break;
            }

        }
        //2-处理消息中
        case 2: {
            int res=0;

            //todo 注册的消费端函数-解析和处理消息内容
            res = taskInfo->task(taskInfo);
            taskInfo->execInfo.code = res;

            //0-正常处理(保持状态)
            if(res==0){
                break;
            }
            //1-结果正常
            else if(res==1){
                taskInfo->status=1;//1-等待中
                break;
            }
            else{
                taskInfo->status=3;//3-异常处理
                break;
            }
        }
        //3-异常处理
        case 3: {
            //todo 可处理异常
            if(
                taskInfo->execInfo.code == 2    //2-连接已关闭
            ){
                //todo （主动）通知main 连接重置
                task_notify_main_reset_conn(conn_index);
                taskInfo->status=0;//0-闲置状态（用于等待main重置连接或通道）
                break;
            }
            else if(
                taskInfo->execInfo.code == 3    //3-通道已关闭
            ){
                //todo （主动）通知main 通道重置
                task_notify_main_reset_channel(conn_index,channel_index);
                taskInfo->status=0;//0-闲置状态（用于等待main重置连接或通道）
                break;
            }
            //todo 不可处理异常
            else{
                //todo 通知main 停止所有线程
                task_notify_main_stop(taskInfo);

                taskInfo->status=4;//4-结束
                break;
            }
        }
        //4-结束
        case 4: {
            info("thread consumer[%d]: stopped, wait exit",taskInfo->index);

            //todo 退出前处理
            {
                //todo 停止前进行资源释放
                if(taskInfo->execInfo.data!=NULL){
                    free(taskInfo->execInfo.data);
                }
                //todo 退出信息设置
                if(taskInfo->execInfo.info == NULL){
                    taskInfo->execInfo.info=get_code_info(taskInfo->execInfo.code);
                }
            }

            //todo 通知任务已全部结束
            task_notify_main_exit();

            //todo 结束线程
            pthread_exit(NULL);
        }
        default: {
            break;
        }
    }
}



void *rabbitmq_producer_deal(void *arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
    producerEntity_t_t producer = producersInfo.producers[taskInfo->index];
    int conn_index = producer.conn_index;
    int channel_index = producer.channel_index;

    //todo 检查客户端运行状态
    {
        //todo 0-就绪
        if (work_status == 0) {
            sleep(1);
            info("thread producer[%d]:wait running", taskInfo->index);
            return NULL;
        }
        //todo 1-运行
//    if(work_status==1){
//        //无处理
//    }
        //todo 检查连接状态
        //（被动）当检测到main状态处于重置时 确保任务所对应的连接是可用的
        //todo 考虑改为限时等待（非阻塞）
        else if (work_status == 2) {
            task_wait_main_reset_conn(conn_index);
        } else if (work_status == 3) {
            task_wait_main_reset_channel(conn_index, channel_index);
        }
        //todo 4-stop
        else if (work_status == 4) {
            taskInfo->status = 5;
            taskInfo->execInfo.code=9;
        }
    }

    //todo 生产端状态机处理
    {
        switch (taskInfo->status) {
            //0-闲置（用于等待main重置连接或通道）
            case 0:{
                break;
            }

            //1-等待中
            case 1:{
                int res=0;
//              taskInfo->execInfo.code=0;
                //todo 业务函数（生产端-准备待发送的数据）
                //分配空间
//                taskInfo->execInfo.data.len=0;
//                taskInfo->execInfo.data.bytes=NULL;
//                taskInfo->execInfo.data = amqp_bytes_malloc(255);
                taskInfo->execInfo.data=NULL;
                taskInfo->execInfo.data=malloc(255);

                //todo 注册的发布端函数-准备消息
                res = taskInfo->task(taskInfo);
                taskInfo->execInfo.code = res;


                //0-正常处理(保持状态)
                if(res==0){
                    break;
                }
                //1-结果正常
                else if(res==1){
                    taskInfo->status=2;//2-发布中
                    break;
                }
                else{
                    taskInfo->status=4;//4-异常处理
                    break;
                }

            }
            //2-发布中
            case 2: {
                producerEntity_t_t producerInfo = producersInfo.producers[taskInfo->index];
                connectionEntity connInfo = rabbitmqConnsInfo.conns[producerInfo.conn_index];

                //参数
                amqp_connection_state_t connState = connInfo.connState;
                int channel = connInfo.channelsInfo.channels[producerInfo.channel_index].num;
                amqp_bytes_t exchange = amqp_cstring_bytes(exchangesInfo.exchanges[producerInfo.exchange_index].name);
                amqp_bytes_t routingKey = amqp_cstring_bytes(producerInfo.routingKey);
                int confirmMode = producerInfo.confirmMode;
                int mandatory = producerInfo.mandatory;
                int immediate = producerInfo.immediate;

                amqp_basic_properties_t *props = &producerInfo.props;

                //todo 消息发布
                int res=amqp_basic_publish(
                        connState,
                        channel,//channel
                        exchange,   //exchange
                        routingKey, //routingKey
                        mandatory,//mandatory
                        immediate,//immediate
                        props,//properties 消息头帧属性
//                       taskInfo->execInfo.data//body
                        amqp_cstring_bytes(taskInfo->execInfo.data)
                );
                if(res==AMQP_STATUS_OK){
                    info("producer send:");
                    info(taskInfo->execInfo.data);
                    //todo 检查发布确认是否开启
                    if(confirmMode == 1) {
                        //已发送且需要等待确认
                        taskInfo->status=3;//3-发布确认中
                        return NULL;
                    }
                    else{
                        //已发送无需等待确认
                        taskInfo->status=4;//4-异常处理中
                    }
                }
                else{
                    taskInfo->status=4;//4-异常处理中
                    taskInfo->execInfo.code=5;//5-数据发布失败
                }

                break;
            }
            //3-发布确认中
            case 3: {
                int res = wait_ack(taskInfo);//返回0 1 2 3 6
                taskInfo->execInfo.code=res;

                //0-正常处理(保持状态)
                if(res==0){
                    return NULL;
                }
                //1-结果正常
                else if(res==1){
                    taskInfo->status=0;
                    break;
                }
                else{
                    taskInfo->status=4;//异常处理
                    break;
                }
            }
            //4-异常处理中
            case 4: {
                //todo 可处理异常
                if(
                    taskInfo->execInfo.code == 2    //2-连接已关闭
                ){
                    //todo （主动）通知main 连接重置
                    task_notify_main_reset_conn(conn_index);
                    taskInfo->status=0;//0-闲置状态（用于等待main重置连接或通道）
                    break;
                }
                else if(
                    taskInfo->execInfo.code == 3    //3-通道已关闭
                ){
                    //todo （主动）通知main 通道重置
                    task_notify_main_reset_channel(conn_index,channel_index);
                    taskInfo->status=0;//0-闲置状态（用于等待main重置连接或通道）
                    break;
                }
                //todo 不可处理异常
                else{
                    //todo 通知main 停止所有线程
                    task_notify_main_stop(taskInfo);

                    taskInfo->status=5;//5-结束
                    break;
                }
            }

            //5-结束
            case 5:{

                info("thread producer[%d]: stopped, wait exit",taskInfo->index);

                //todo 退出前处理
                {
                    //todo 停止前进行资源释放
                    if(taskInfo->execInfo.data!=0){
                        free(taskInfo->execInfo.data);
                    }
                    //todo 退出信息设置
                    if(taskInfo->execInfo.info == NULL){
                        taskInfo->execInfo.info=get_code_info(taskInfo->execInfo.code);
                    }
                }

                //todo 通知任务已全部结束
                task_notify_main_exit();

                //todo 结束线程
                pthread_exit(NULL);
            }
            default: {
                break;
            }
        }
    }
}


//解析 收到的定时任务消息
int consumer_task_handle_cron_message(void * arg){

}
//准备 采集设备数据
int producer_task_prepare_device_message(void * arg) {


}
//准备 设备故障数据
int producer_task_prepare_fault_message(void * arg){
    
}
