#include <bits/types/struct_timeval.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <malloc.h>
#include "stdio.h"
#include "time.h"
#include "memory.h"

#include "rabbitmq-c.h"


pthread_mutex_t lock1;
pthread_cond_t cond1;
int flag=0;
void *test(void * args){
//    char buf[10];
//    warn("thread: start");
//    fgets(buf,10,stdin);


    pthread_mutex_lock(&lock1);
    warnLn("thread: get lock ");

    flag=1;
//    pthread_cond_broadcast(&cond1);
    warnLn("thread: pre signal");
//    print_synchronized_info(&lock1,&cond1);

//    pthread_cond_signal(&cond1);
    pthread_cond_wait(&cond1,&lock1);

    warnLn("thread: signal");
//    print_synchronized_info(&lock1,&cond1);

    pthread_mutex_unlock(&lock1);

//    warn("thread: input");
//    fgets(buf,10,stdin);

    pthread_mutex_lock(&lock1);
    warnLn("thread: get lock ");
    pthread_mutex_unlock(&lock1);

    return NULL;
}
void release_resources(void *arg){
    infoLn("thread exit");
    pthread_mutex_t *p_lock = (pthread_mutex_t *) arg;
    if( p_lock->__data.__owner==syscall(SYS_gettid)){
        warnLn("clean: unlock");
        pthread_mutex_unlock(p_lock);
    }
}
void *test01(void * args){
    pthread_cleanup_push(release_resources,&lock1);

    pthread_mutex_lock(&lock1);
            infoLn("thread wait");

//    pthread_cond_wait(&cond1,&lock1);
//    sleep(100);

            infoLn("thread wake up");

    pthread_mutex_unlock(&lock1);
            infoLn("thread unlock");

    pthread_cleanup_pop(0);

    return NULL;
}
int main(){

//todo rabbitmq客户端程序
    {
        while(1){
            if(rabbitmq_start_client()==0){
                break;
            }
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
        return EXEC_PRODUCER_CONFIRM_FAIL;//6-发布ACK失败
    }

    //todo 处理rabbitmq服务发来的响应
    {
        switch (result.method) {
            case 0:
                //todo 非阻塞等待确认，还未收到任何消息
                return EXEC_NORMAL;//0-正常处理(保持状态)
            case AMQP_BASIC_ACK_METHOD:
                //todo 来自rabbitmq服务的ack
                return EXEC_CORRECT;//1-结果正常

            case AMQP_BASIC_RETURN_METHOD:
                //mandatory=1时,消息会被服务器发回到生产者
                taskInfo->execInfo.info="wait confirm but AMQP_BASIC_RETURN_METHOD";
                return EXEC_PRODUCER_CONFIRM_FAIL;
            case AMQP_BASIC_REJECT_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_BASIC_REJECT_METHOD";
                return EXEC_PRODUCER_CONFIRM_FAIL;//6-发布ACK失败
            case AMQP_BASIC_NACK_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_BASIC_NACK_METHOD";
                return EXEC_PRODUCER_CONFIRM_FAIL;//6-发布ACK失败

            case AMQP_CHANNEL_CLOSE_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_CHANNEL_CLOSE_METHOD";
                return EXEC_CHANNEL_CLOSED;//3-通道已关闭
            case AMQP_CONNECTION_CLOSE_METHOD:
                taskInfo->execInfo.info="wait confirm but AMQP_CONNECTION_CLOSE_METHOD";
                return EXEC_CONN_CLOSED;//2-连接已关闭

            default:
                taskInfo->execInfo.info="wait confirm but unknown frame type";
                return EXEC_UNKNOWN_FRAME;//9-非预期帧
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
        if (work_status == CLIENT_STATUS_READY) {
            sleep(1);
            infoLn("thread consumer[%d]:wait running", taskInfo->index);
            return NULL;
        }

        //todo 1-运行
//        if(work_status==CLIENT_STATUS_RUNNING){
//            //无处理
//        }

        //todo 检查连接状态
        //（被动）当检测到main状态处于重置时 确保任务所对应的连接是可用的
        //todo 考虑改为限时等待（非阻塞）
        else if (work_status == CLIENT_STATUS_RESETTING_CONN) {
            task_wait_main_reset_conn(conn_index);
            taskInfo->status = CONSUMER_TASK_WAIT_MESSAGE;
        }
        else if (work_status == CLIENT_STATUS_RESETTING_CHANNEL) {
            task_wait_main_reset_channel(conn_index, channel_index);
            taskInfo->status = CONSUMER_TASK_WAIT_MESSAGE;
        }
        //todo 4-stop
        else if (work_status == CLIENT_STATUS_STOPPING_TASKS) {
            taskInfo->status = CONSUMER_TASK_EXIT;//4-结束
            taskInfo->execInfo.code= EXEC_NOTIFIED_STOP;//10-被通知停止运行
        }
    }

    switch (taskInfo->status) {
        //0-闲置（等待main重置连接或通道）
        case CONSUMER_TASK_IDLE:{

            break;
        }
        //1-等待中
        case CONSUMER_TASK_WAIT_MESSAGE: {
            int res=0;

            //分配空间
            if(taskInfo->execInfo.data==NULL){
                taskInfo->execInfo.data=malloc(sizeof(amqp_envelope_t));
            }
            memset(taskInfo->execInfo.data,0, sizeof(amqp_envelope_t));
            taskInfo->execInfo.code=0;

            //todo 获取消息
            res = get_an_message(taskInfo);//1 0 | 7 3 2 9
            taskInfo->execInfo.code=res;

            //0-正常处理(保持状态)
            if(res==0){
                break;
            }
            //1-结果正常
            else if(res==1){
                taskInfo->status=CONSUMER_TASK_HANDLE_MESSAGE;//2-处理消息中
                break;
            }
            else{
                taskInfo->status=CONSUMER_TASK_HANDLE_EXCEPTION;//3-异常处理
                break;
            }
        }
        //2-处理消息中
        case CONSUMER_TASK_HANDLE_MESSAGE: {
            int res=0;

            //todo 注册的消费端函数-解析和处理消息内容
            res = taskInfo->task(taskInfo);//-1 0 1 8
            taskInfo->execInfo.code = res;


            //0-正常处理(保持状态)
            if(res==0){
                break;
            }
            else{
                if(res==EXEC_ERROR){
                    taskInfo->status=CONSUMER_TASK_HANDLE_EXCEPTION;//3-异常处理
                }
                else{
                    //todo 手动ack处理
                    if(consumer.no_ack==0){
                        //todo 确认消息
                        if(res==EXEC_CORRECT){
                            amqp_basic_ack(
                                    rabbitmqConnsInfo.conns[conn_index].connState,
                                    rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num,//channel
                                    ((amqp_envelope_t *)(taskInfo->execInfo.data))->delivery_tag,//需要被确认消息的标识符
                                    0//批量确认
                            );

                            infoLn("consumer[%d]: manual ack", taskInfo->index);

                            taskInfo->status=CONSUMER_TASK_WAIT_MESSAGE;//1-等待中
                        }
                        //todo 拒绝消息
                        else if(res==EXEC_CONSUMER_MESSAGE_REJECT){
                            amqp_basic_reject(
                                    rabbitmqConnsInfo.conns[conn_index].connState,
                                    rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num,//channel
                                    ((amqp_envelope_t *)(taskInfo->execInfo.data))->delivery_tag,//需要被拒绝消息的标识符
                                    1 //requeue
                            );
//                            amqp_basic_nack(
//                                connState,
//                                1,//channel
//                                envelope.delivery_tag,//需要被拒绝消息的标识符
//                                1,//multiple 批量拒绝比当前标识小的未确认的消息
//                                1//requeue
//                            );
                            infoLn("consumer[%d]: reject requeue", taskInfo->index);

                            taskInfo->status=CONSUMER_TASK_WAIT_MESSAGE;//1-等待中
                        }
                    }
                    else if(consumer.no_ack==1){
                        infoLn("consumer[%d]: auto ack", taskInfo->index);
                        taskInfo->status=CONSUMER_TASK_WAIT_MESSAGE;//1-等待中
                    }
                }

                //todo 释放消息资源
                amqp_destroy_envelope(taskInfo->execInfo.data);

                break;
            }
        }
        //3-异常处理
        case CONSUMER_TASK_HANDLE_EXCEPTION: {
            //todo 可处理异常
            if(
                taskInfo->execInfo.code == 2    //2-连接已关闭
            ){
                //todo （主动）通知main 连接重置
                task_notify_main_reset_conn(conn_index);
                taskInfo->status=CONSUMER_TASK_IDLE;//0-闲置状态（用于等待main重置连接或通道）
                break;
            }
            else if(
                taskInfo->execInfo.code == 3    //3-通道已关闭
            ){
                //todo （主动）通知main 通道重置
                task_notify_main_reset_channel(conn_index,channel_index);
                taskInfo->status=CONSUMER_TASK_IDLE;//0-闲置状态（用于等待main重置连接或通道）
                break;
            }
            //todo 不可处理异常
            else{
                //todo 通知main 停止所有线程
                task_notify_main_stop(taskInfo);

                taskInfo->status=CONSUMER_TASK_EXIT;//4-结束
                break;
            }
        }
        //4-结束
        case CONSUMER_TASK_EXIT: {
            infoLn("thread consumer[%d]: stopped, wait exit", taskInfo->index);

            //todo 退出前处理
            {
                //todo 停止前进行资源释放
                if(taskInfo->execInfo.data!=NULL){
                    free(taskInfo->execInfo.data);
                    taskInfo->execInfo.data=NULL;
                }
//                //todo 清除运行状态数据,便于下次启动
//                rabbitmqConnsInfo.conns[conn_index].flag_reset=0;
//                rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].flag_reset=0;

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

    return arg;
}

void notify_message_publish_result(taskInfo_t *taskInfo,int success){
    char *msg=NULL;
    if(taskInfo->status==PRODUCER_TASK_PUBLISH){
        msg="publish";
    }
    else if(taskInfo->status==PRODUCER_TASK_CONFIRM){
        msg="confirm";
    }
    warnLn("-------------------------------------------");
    warnLn("producer[%d]:%s",taskInfo->index,msg,success?"success":"fail");

    //todo 通知控制板数据发布情况
//    if(success){
//
//    }
//    else{
//
//    }
}

int publish_an_message(void *arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    producerEntity_t_t *producerInfo = &(producersInfo.producers[taskInfo->index]);
    connectionEntity *connInfo = &(rabbitmqConnsInfo.conns[producerInfo->conn_index]);

    //参数
    amqp_connection_state_t connState = connInfo->connState;
    int channel = connInfo->channelsInfo.channels[producerInfo->channel_index].num;
    amqp_bytes_t exchange = amqp_cstring_bytes(exchangesInfo.exchanges[producerInfo->exchange_index].name);
    amqp_bytes_t routingKey = amqp_cstring_bytes(producerInfo->routingKey);
    int confirmMode = producerInfo->confirmMode;
    int mandatory = producerInfo->mandatory;
    int immediate = producerInfo->immediate;

    amqp_basic_properties_t *props = &(producerInfo->props);

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
        infoLn("=======================================");
        infoLn("producer[%d] send: len=%d", taskInfo->index, strlen(taskInfo->execInfo.data));
        infoLn(">%s",taskInfo->execInfo.data);
        return EXEC_CORRECT;
    }
    else{
        return EXEC_CONN_CLOSED;
//        amqp_frame_t frame={};
//        amqp_rpc_reply_t reply = amqp_get_rpc_reply(connState);
//        if(reply.library_error==AMQP_STATUS_UNEXPECTED_STATE){
//            if (AMQP_STATUS_OK != amqp_simple_wait_frame(connState, &frame)) {
//                return EXEC_CONSUMER_MESSAGE_GET_FAIL;
//            }
//            //1.METHOD帧 方法帧
//            if (AMQP_FRAME_METHOD == frame.frame_type) {
//                switch (frame.payload.method.id) {
//                    //todo 需要处理的帧(连接、通道的关闭)
//                    case AMQP_CHANNEL_CLOSE_METHOD:
//                    {
//                        warnLn("producer[%d]: AMQP_CHANNEL_CLOSE_METHOD", taskInfo->index);
//                        //todo 解析关闭原因
//                        amqp_channel_close_t *r = (amqp_channel_close_t *) frame.payload.method.decoded;
//                        warnLn(r->reply_text.bytes);
//
//                        return EXEC_CHANNEL_CLOSED;
//                    }
//                    //connect已关闭
//                    case AMQP_CONNECTION_CLOSE_METHOD:
//                    {
//                        //对同一个deliveryTag进行多次ack也会触发
//                        warnLn("producer[%d]: AMQP_CONNECTION_CLOSE_METHOD", taskInfo->index);
//                        //todo 需要重新打开连接
//                        amqp_connection_close_t *r = (amqp_connection_close_t *) frame.payload.method.decoded;
//                        warnLn(r->reply_text.bytes);
//
//                        return EXEC_CONN_CLOSED;//2-连接已关闭
//                    }
//                        //其它非预期帧(除AMQP_BASIC_DELIVER_METHOD和连接相关以外的帧)
//                    default:
//                    {
//                        //todo 非预期帧(以下帧无需消费端特别处理)
////                            AMQP_BASIC_ACK_METHOD
////                            AMQP_BASIC_RETURN_METHOD
////                            AMQP_BASIC_DELIVER_METHOD
//                        warnLn("producer[%d]: received other frame", taskInfo->index);
//                        return EXEC_UNKNOWN_FRAME;
//                    }
//                }
//            }
//        }
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
        if (work_status == CLIENT_STATUS_READY) {
            sleep(2);
            infoLn("thread producer[%d]:wait running", taskInfo->index);
            return NULL;
        }
        //todo 1-运行
//    if(work_status==1){
//        //无处理
//    }
        //todo 检查连接状态
        //（被动）当检测到main状态处于重置时 确保任务所对应的连接是可用的
        //todo 考虑改为限时等待（非阻塞）
        else if (work_status == CLIENT_STATUS_RESETTING_CONN) {
            task_wait_main_reset_conn(conn_index);
            taskInfo->status = PRODUCER_TASK_WAIT_DATA;//1-等待中
        }
        else if (work_status == CLIENT_STATUS_RESETTING_CHANNEL) {
            task_wait_main_reset_channel(conn_index, channel_index);
            taskInfo->status = PRODUCER_TASK_WAIT_DATA;//1-等待中
        }
        //todo 4-stop
        else if (work_status == CLIENT_STATUS_STOPPING_TASKS) {
            taskInfo->status = PRODUCER_TASK_EXIT;//5-结束
            taskInfo->execInfo.code = EXEC_NOTIFIED_STOP;//10-被通知停止运行
        }
    }

    //todo 生产端状态机处理
    {
        switch (taskInfo->status) {
            //0-闲置（用于等待main重置连接或通道）
            case PRODUCER_TASK_IDLE:{
                sleep(1);
                break;
            }

            //1-等待中
            case PRODUCER_TASK_WAIT_DATA:{
                int res=0;
//              taskInfo->execInfo.code=0;
                //todo 防止连接断开时一直在等待数据，导致无法完成连接重置
                if(rabbitmqConnsInfo.conns[conn_index].flag_reset==1){
                    taskInfo->execInfo.code = EXEC_CONN_CLOSED;//2-连接已关闭
                    taskInfo->status=PRODUCER_TASK_HANDLE_EXCEPTION;//4-异常处理
                    break;
                }
                //todo 通道暂无检测方式，只能一直等待数据
//                else if(rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[conn_index].flag_reset==1){
//                    taskInfo->execInfo.code = EXEC_CHANNEL_CLOSED;//3-通道已关闭
//                    taskInfo->status=PRODUCER_TASK_HANDLE_EXCEPTION;//4-异常处理
//                    break;
//                }

                //todo 分配和重置空间
                if(taskInfo->execInfo.data==NULL){
                    taskInfo->execInfo.data=malloc(255);
                }
                memset(taskInfo->execInfo.data,0,255);
                taskInfo->execInfo.code=0;

                //todo 注册的发布端函数-准备消息
                res = taskInfo->task(taskInfo);
                taskInfo->execInfo.code = res;

                //0-正常处理(保持状态,本次执行无消息数据)
                if(res==EXEC_NORMAL){
                    break;
                }
                //1-结果正常（数据准备成功）
                else if(res==EXEC_CORRECT){
                    taskInfo->status=PRODUCER_TASK_PUBLISH;//2-发布中
                    break;
                }
                else{
                    taskInfo->status=PRODUCER_TASK_HANDLE_EXCEPTION;//4-异常处理
                    break;
                }

            }
            //2-发布中
            case PRODUCER_TASK_PUBLISH: {
                producerEntity_t_t *producerInfo = &(producersInfo.producers[taskInfo->index]);
                connectionEntity *connInfo = &(rabbitmqConnsInfo.conns[producerInfo->conn_index]);
                //todo 发布一条消息
                int res=publish_an_message(taskInfo);
                taskInfo->execInfo.code=res;

                if(res==EXEC_CORRECT){
                    //todo 检查发布确认是否开启
                    if(producerInfo->confirmMode == 1) {
                        //已发送且需要等待确认
                        taskInfo->status=PRODUCER_TASK_CONFIRM;//3-发布确认中
                        break;
                    }
                    else{
                        notify_message_publish_result(taskInfo,1);
                        //已发送无需等待确认
                        taskInfo->status=PRODUCER_TASK_WAIT_DATA;//1-等待中
                        break;
                    }
                }
                else{
                    notify_message_publish_result(taskInfo,0);

                    taskInfo->status=PRODUCER_TASK_HANDLE_EXCEPTION;//4-异常处理中
                    break;
                }
            }
            //3-发布确认中
            case PRODUCER_TASK_CONFIRM: {
                //todo 发布端confirm处理
                int res = wait_ack(taskInfo);//返回0 1 2 3 6 9
                taskInfo->execInfo.code=res;

                //0-正常处理(保持状态)
                if(res==EXEC_NORMAL){
                    break;
                }
                //1-结果正常
                else if(res==EXEC_CORRECT){
                    notify_message_publish_result(taskInfo,1);
                    taskInfo->status=PRODUCER_TASK_WAIT_DATA;//1-等待中
                    break;
                }
                else{
                    notify_message_publish_result(taskInfo,0);
                    taskInfo->status=PRODUCER_TASK_HANDLE_EXCEPTION;//4-异常处理
                    break;
                }
            }
            //4-异常处理中
            case PRODUCER_TASK_HANDLE_EXCEPTION: {
                //todo 可处理异常
                if(
                    taskInfo->execInfo.code == EXEC_CONN_CLOSED    //2-连接已关闭
                ){
                    //todo （主动）通知main 连接重置
                    task_notify_main_reset_conn(conn_index);
                    taskInfo->status=PRODUCER_TASK_IDLE;//0-闲置状态（用于等待main重置连接或通道）
                    break;
                }
                else if(
                    taskInfo->execInfo.code == EXEC_CHANNEL_CLOSED    //3-通道已关闭
                ){
                    //todo （主动）通知main 通道重置
                    task_notify_main_reset_channel(conn_index,channel_index);
                    taskInfo->status=PRODUCER_TASK_IDLE;//0-闲置状态（用于等待main重置连接或通道）
                    break;
                }
                //todo 不可处理异常
                else{
                    //todo 通知main 停止所有线程
                    task_notify_main_stop(taskInfo);

                    taskInfo->status=PRODUCER_TASK_EXIT;//5-结束
                    break;
                }
            }

            //5-结束
            case PRODUCER_TASK_EXIT:{

                infoLn("thread producer[%d]: stopped, wait exit", taskInfo->index);

                //todo 退出前处理
                {
                    //todo 停止前进行资源释放
                    if(taskInfo->execInfo.data!=NULL){
                        free(taskInfo->execInfo.data);
                        taskInfo->execInfo.data=NULL;
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
    return arg;
}



/*
@desc   解析收到的定时任务消息
@param  arg 包含任务信息，以及消息内容((amqp_envelope_t *))(((taskInfo_t *) arg)->execInfo_t.data)
@return EXEC_CORRECT    消息正常ACK处理
        EXEC_CONSUMER_MESSAGE_REJECT    拒绝消息
        EXEC_ERROR  消息处理异常
*/
int consumer_task_handle_cron_message(void *arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    //todo 提取消息内容
    amqp_envelope_t *envelope = (amqp_envelope_t *) taskInfo->execInfo.data;

    //打印消息信息
    print_rcv_message(envelope);


//    //todo 消息处理异常
//    return EXEC_ERROR;

//    //TODO 拒绝消息
//    return EXEC_CONSUMER_MESSAGE_REJECT;

    //todo 消息处理没问题
    return EXEC_CORRECT;
}
/*
@desc   解析收到的定时任务消息
@param  arg 包含任务信息，以及消息内容((amqp_envelope_t *))(((taskInfo_t *) arg)->execInfo_t.data)
@return EXEC_NORMAL     正常执行（本次执行暂无数据）
        EXEC_CORRECT    数据准备成功
        EXEC_PRODUCE_DATA_PREPARE_FAIL  数据准备失败
 */
int producer_task_prepare_device_message(void *arg) {
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    //todo 填充数据
    //正常返回数据
    {
        taskInfo->execInfo.data;
//    info("producer[%d]>")
        info(stdout, "producer[%d]>",taskInfo->index);
        fgets(taskInfo->execInfo.data,255,stdin);


//        strcat(taskInfo->execInfo.data,"test1");
//        sleep(3);


        return EXEC_CORRECT;
    }

//    //暂无数据
//    {
//        return EXEC_NORMAL;
//    }

//    //数据准备失败
//    {
//        return EXEC_PRODUCE_DATA_PREPARE_FAIL;
//    }

}
//准备 设备故障数据
int producer_task_prepare_fault_message(void *arg){
    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    //todo 填充数据
    //正常返回数据
//    {
////        taskInfo->execInfo.data;
//////    info("producer[%d]>")
////        info(stdout, "producer[%d]>",taskInfo->index);
////        fgets(taskInfo->execInfo.data,255,stdin);
//
//
//        strcat(taskInfo->execInfo.data,"test2");
//        sleep(5);
//
//
//        return EXEC_CORRECT;
//    }

    //暂无数据
    {
        return EXEC_NORMAL;
    }

//    //数据准备失败
//    {
//        return EXEC_PRODUCE_DATA_PREPARE_FAIL;
//    }

}
