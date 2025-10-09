#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <bits/types/struct_timeval.h>
#include <sys/syscall.h>
#include <malloc.h>

#include "rabbitmq-c.h"
//全局锁
pthread_mutex_t log_mutex;
pthread_mutex_t global_task_mutex;

volatile int work_status=0;//0-ready就绪 1-running运行 2-stop停止 3-exit 4-terminated
//条件变量和全局标志
volatile int flag_running=0;
pthread_cond_t cond_running;
volatile int flag_stop=0;
pthread_cond_t cond_deal;
volatile int flag_exit=0;
pthread_cond_t cond_exit;
volatile int thread_counts=0;
volatile int flag_reset_conn;//
volatile int flag_reset_channel;//-1

RabbitmqConfig_t rabbitmqConfigInfo={
    .hostname="192.168.200.135",//虚拟机
//    .hostname="192.168.138.241",//本机ip,热点分配
    .port=5672,
    .user="root",
    .passwd="123123"
};

connectionsInfo_t rabbitmqConnsInfo={
    .size=2,
    .conns={
        //todo 专用连接-生产者
        {
            .connState=NULL,
            .socket=NULL,
            .status=0,
            .channelsInfo={
                .size=3,
                .channels={
                    //todo index=0 专用通道-非生产相关操作
                    {
                        .num=1,
                        .status=0
                    },
                    //todo index>0 生产通道-线程专用
                    {
                        .num=2,
                        .status=0
                    },
                    {
                        .num=3,
                        .status=0
                    }
                }
            }
        },
        //todo 专用连接-消费者
        {
            .connState=NULL,
            .socket=NULL,
            .status=0,
            .channelsInfo={
                .size=3,
                .channels={
                    //todo index=0 专用通道-非消费相关操作
                    {
//                        .num=1,
                        .status=0
                    },
                    //todo index>0 消费通道-线程专用
                    {
//                        .num=2,
                        .status=0
                    },
                    {
//                        .num=3,
                        .status=0
                    }
                }
            }
        }
    }
};

RabbitmqExchanges_t exchangesInfo={
    .size=2,
    .exchanges={
        //todo 上传 设备数据消息 设备故障消息
        {
            .name="myExchange",
            .type="topic",
//            .passive=1,//1-仅仅检查
            .durability=1,
            .autoDelete=0,
            .internal=0,
            .args={0}
        },
        //todo 死信交换机
        {
            .name="deadLetterExchange",
            .type="direct",
//            .passive=1,//1-仅仅检查
            .durability=1,
            .autoDelete=0,
            .internal=0,
            .args={0}
        }
    }
};//交换机

RabbitmqQueues_t queuesInfo={
    .size=4,
    .queues={
        //todo 设备数据消息队列
        {
            .name="plc_data",
            .type=0,
//            .passive=1,//1-仅仅检查
            .durability=1,
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
            .args={
//                {.type=0,.key=ARGUMENT_QUEUE_00,.value.integer=},
                {.type=0,.key=ARGUMENT_QUEUE_01,.value.integer=3600000},
//                {.type=1,.key=ARGUMENT_QUEUE_02,.value.str="[reject-publish|]"},
                {.type=1,.key=ARGUMENT_QUEUE_04,.value.str="deadLetterExchange"},
                {.type=1,.key=ARGUMENT_QUEUE_05,.value.str="deadRoutingKeyExample"}
//                {.type=0,.key=ARGUMENT_QUEUE_06,.value.str=},
//                {.type=0,.key=ARGUMENT_QUEUE_07,.value.str=},
//                {.type=0,.key=ARGUMENT_QUEUE_08,.value.str=},
            }
        },
        //todo 设备故障消息队列
        {
            .name="Fault_Reports",
            .type=0,
            .durability=1,
//            .passive=1,//1-仅仅检查
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
            .args={
                {.type=1,.key=ARGUMENT_QUEUE_04,.value.str="deadLetterExchange"},
                {.type=1,.key=ARGUMENT_QUEUE_05,.value.str="deadRoutingKeyExample"}
            }
        },
        //todo 死信队列
        {
            .name="deadLetterQueue",
            .type=0,
            .durability=1,
//            .passive=1,//1-仅仅检查
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
//            .args={0}
        },
        //todo 定时数据队列
        {
            .name="cornDataQueue",
            .type=0,
            .durability=1,
//            .passive=1,//1-仅仅检查
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
//            .args={0}
        }
    }
};//队列

RabbitmqBinds_t bindsInfo= {
    .size=3,
    .binds={
        //连接状态 通道 路由键 队列 交换机
        {
            .routingKey=BINDING_KEY_PLC_DATA,
            .exchange_index=0,
            .queue_index=0//设备数据消息队列
        },
        {
            .routingKey=BINDING_KEY_FAULT_REPORTS,
            .exchange_index=0,
            .queue_index=1//设备故障消息队列
        },
        {
            .routingKey=BINDING_KEY_DEAD_LETTER,
            .exchange_index=1,//死信交换机
            .queue_index=2//死信队列
        },
    }
};
consumers_t consumersInfo={
    .size=1,//注：需要保证至少一个任务，否则client启动失败
    .consumers={
        //todo 消费者
        {
            .conn_index=1,
//            .channel_index=1,//第1个通道不使用,自动根据连接分配可用通道

            //参数设置
            .queue_index=3,//todo 绑定队列（3-定时数据队列）
            .consumer_tag="consumer00",//可由rabbitmq服务端自动分配标识符
//            .no_local=0,
            .no_ack=1,
//            .exclusive=0,
//            .requeue=0,

            .taskInfo={
                .task=consumer_task_handle_cron_message,
            }
        }
    }
};
producers_t producersInfo={
    .size=2,//注：需要保证至少一个任务，否则client启动失败
    .producers={
        //todo 生产者 采集设备数据
        {
            .conn_index=0,
//            .channel_index=1,//第1个通道不使用,自动根据连接分配可用通道

            //参数设置
            .exchange_index=0,
            .confirmMode=0,
            .routingKey=ROUTING_KEY_PLC_DATA,//发布时的routingKey

            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            },
            .taskInfo={
                .task=producer_task_prepare_device_message,
            }
        },
        //todo 生产者 设备故障数据
        {
            .conn_index=0,
//            .channel_index=2,//自动分配
            
            //参数设置
            .exchange_index=0,
            .confirmMode=1,
            .routingKey=ROUTING_KEY_FAULT_REPORTS,

            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            },
            .taskInfo={
                .task=producer_task_prepare_fault_message,
            }
        }
    },

};

//todo synchronized
//任务 通知main所有任务已就绪
void task_notify_main_run(){
    //todo 通知 线程已就绪
    pthread_mutex_lock(&global_task_mutex);
    thread_counts++;
    if(thread_counts==producersInfo.size+consumersInfo.size){
        flag_running=1;
        warnLn("signal=cond_running");
        pthread_cond_signal(&cond_running);
    }
    pthread_mutex_unlock(&global_task_mutex);
}
//任务 通知main处理
void task_notify_main_deal(){
    pthread_mutex_lock(&global_task_mutex);
    pthread_cond_broadcast(&cond_deal);
    pthread_mutex_unlock(&global_task_mutex);
}
//任务 通知main停止
void task_notify_main_stop(taskInfo_t *taskInfo){
    //todo 处理出错或其它会导致客户端状态变为 2-stop
    {
        pthread_mutex_lock(&global_task_mutex);

        flag_stop=1;
        pthread_cond_broadcast(&cond_deal);

        pthread_mutex_unlock(&global_task_mutex);
    }
}
//线程 通知main退出
void task_notify_main_exit(){
    //todo 通知任务已退出
    pthread_mutex_lock(&global_task_mutex);
    thread_counts--;
    if(thread_counts==0){
        flag_exit=1;
        warnLn("signal=cond_exit");
        pthread_cond_broadcast(&cond_exit);//通知主线程
    }
    warnLn("thread_counts=%d", thread_counts);
    pthread_mutex_unlock(&global_task_mutex);
}
//通知 main处理 连接通道重置
void task_notify_main_reset_conn(int conn_index){
    {
        pthread_mutex_lock(&global_task_mutex);
        //todo 将 连接 标记为 重置
        rabbitmqConnsInfo.conns[conn_index].flag_reset=1;

        //todo 通知main处理 同个连接下有多个线程，需要等待所有线程通知main,main才能重置连接，否则造成线程访问空连接问题
        rabbitmqConnsInfo.conns[conn_index].task_nums--;
        if (rabbitmqConnsInfo.conns[conn_index].task_nums == 0) {
            flag_reset_conn = 1;
            pthread_cond_broadcast(&cond_deal);
        }

        pthread_mutex_unlock(&global_task_mutex);
    }
}
void task_notify_main_reset_channel(int conn_index, int channel_index){
    {
        pthread_mutex_lock(&global_task_mutex);
        //todo 将 连接 标记为 重置
        rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].flag_reset=1;

        //todo 通知main处理
        flag_reset_channel = 1;
        pthread_cond_broadcast(&cond_deal);

        pthread_mutex_unlock(&global_task_mutex);
    }
}
//等待 main处理 连接通道重置
int task_wait_main_reset_conn(int conn_index){

    int res=1;//0-重置失败，main正在等待所有任务退出  1-main重置成功，任务继续运行
    struct timespec t={.tv_sec=1};

    pthread_mutex_lock(&global_task_mutex);
    //todo 等待 main处理
    while(
        rabbitmqConnsInfo.conns[conn_index].flag_reset != 0
        || rabbitmqConnsInfo.conns[conn_index].status != 5
    ){
        pthread_cond_timedwait(
            &rabbitmqConnsInfo.cond_reset_conn,
            &global_task_mutex,
            &t
        );
        //每隔1s检查 main是否由于重置失败导致需要停止所有任务线程
        if(work_status == CLIENT_STATUS_STOPPING_TASKS){
            warnLn("thread: wait main handle reset conn,but fail");
            res=0;
            break;
        }
    }
    pthread_mutex_unlock(&global_task_mutex);

    return res;
}
int task_wait_main_reset_channel(int conn_index, int channel_index){

    int res=1;//0-重置失败，main正在等待所有任务退出  1-main重置成功，任务继续运行
    struct timespec t={.tv_sec=1};

    pthread_mutex_lock(&global_task_mutex);
    //todo 等待 main处理
    while(
        rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].flag_reset!=0
        || rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status!=2
    ){
        pthread_cond_timedwait(
            &rabbitmqConnsInfo.conns[conn_index].channelsInfo.cond_reset_channel,
            &global_task_mutex,
            &t
        );
        //每隔1s检查 main是否由于重置失败导致需要停止所有任务线程
        if(work_status == CLIENT_STATUS_STOPPING_TASKS){
            warnLn("thread: wait main handle reset channel,but fail");
            res=0;
            break;
        }
    }
    pthread_mutex_unlock(&global_task_mutex);

    return res;
}
//同步工具的初始化和销毁
void init_synchronize_tools(){
    //todo 可重入锁 注：不建议使用可重入锁，目前发现POSIX提供的wait操作在可重入锁的内层同步块中，无法释放锁资源
//    pthread_mutexattr_t attr;
//    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
//    pthread_mutex_init(&mutex,&attr);//可重入锁


    //todo 普通互斥锁 注：不要多次获取锁造成死锁
    pthread_mutex_init(&log_mutex,NULL);
    pthread_mutex_init(&global_task_mutex, NULL);


    //todo 条件变量
    pthread_cond_init(&cond_running, NULL);
    pthread_cond_init(&cond_deal, NULL);
    pthread_cond_init(&cond_exit,NULL);

    //连接
    pthread_cond_init(&rabbitmqConnsInfo.cond_reset_conn,NULL);

    //通道
    for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
        pthread_cond_init(&rabbitmqConnsInfo.conns[i].channelsInfo.cond_reset_channel,NULL);
    }

}
void destroy_synchronize_tools(){
    //todo 条件变量
    pthread_cond_destroy(&cond_running);
    pthread_cond_destroy(&cond_deal);
    pthread_cond_destroy(&cond_exit);

    //todo 锁
    pthread_mutex_destroy(&global_task_mutex);
    pthread_mutex_destroy(&log_mutex);
}
void clean_synchronize_resources(void *arg){
    infoLn("thread exit");
    pthread_mutex_t *p_lock = (pthread_mutex_t *) arg;
    if( p_lock->__data.__owner==syscall(SYS_gettid)){
        warnLn("clean: unlock");
        pthread_mutex_unlock(p_lock);
    }
}
//void main_cancel_all_task(){
////    for (int i = 0; i < ; ++i) {
////
////    }
//}


//todo check函数
int rabbitmq_check_conn_index(int conn_index){
    if(
            (conn_index >= CONNECTION_MAX_SIZE)
            || (conn_index < 0 || conn_index >= rabbitmqConnsInfo.size)
            ){
        warnLn("conns: index=%d,size=%d,max=%d", conn_index, rabbitmqConnsInfo.size, CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        return 1;
    }
}
int rabbitmq_check_channel_index(int conn_index,int channel_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    if(
            (channel_index >= CHANNEL_MAX_SIZE)
            || (channel_index < 0 || channel_index >= rabbitmqConnsInfo.conns[conn_index].channelsInfo.size)
            ){
        warnLn("channel: conn_index=%d,channel_index=%d,size=%d,max=%d",
               conn_index, channel_index,
               rabbitmqConnsInfo.conns[conn_index].channelsInfo.size,
               CHANNEL_MAX_SIZE
        );
        return 0;
    }
    else{
        return 1;
    }

}
int rabbitmq_check_queue_index(int queue_index){
    if(
            (queue_index >= QUEUE_MAX_SIZE)
            || (queue_index<0 || queue_index>=queuesInfo.size)
            ){
        warnLn("queue: index=%d,size=%d,max=%d", queue_index, queuesInfo.size, QUEUE_MAX_SIZE);
        return 0;
    }
    else{
        return 1;
    }


}
int rabbitmq_check_exchange_index(int exchange_index){
    if(
            (exchange_index >= EXCHANGE_MAX_SIZE)
            ||(exchange_index<0 || exchange_index>=exchangesInfo.size)
            ){
        warnLn("exchange: index=%d,size=%d,max=%d", exchange_index, exchangesInfo.size, EXCHANGE_MAX_SIZE);
        return 0;
    }
    else{
        return 1;
    }
}
int rabbitmq_check_bind_index(int bind_index){
    if(
            (bind_index>=BIND_MAX_SIZE)
            || (bind_index<0 || bind_index>=bindsInfo.size)
            ){
        warnLn("bind check: fail,bind_index=%d", bind_index);
        return 0;
    }

    return 1;
}
//int check_rabbitmq_server_conn_status(int conn_index){
//    return 0;
//}


//todo reset函数 重置状态和NULL
//重置单个连接下的单个通道
int rabbitmq_reset_channel(int conn_index,int channel_index){
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        return 0;
    }
    else{
        //todo 通道号（1-65535）
        //分配默认通道号(下标索引+1)
        if(rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num==0){
            rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num = channel_index+1;
        }
        //通道状态
        rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status = 0;

        //任务信息引用

        return 1;
    }
}
//重置单个连接下的所有通道
int rabbitmq_reset_channels(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    int size = rabbitmqConnsInfo.conns[conn_index].channelsInfo.size;
    if(size > CHANNEL_MAX_SIZE){
        warnLn("conns: size=%d,max=%d", size, CHANNEL_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < rabbitmqConnsInfo.conns[conn_index].channelsInfo.size; ++i) {
            flag&=rabbitmq_reset_channel(conn_index,i);
            if(flag==0){
                warnLn("conn[%d] reset: channel[%d] fail", conn_index, i);
                break;
            }
        }
        return flag;
    }
}
//reset连接初始化前调用，连接关闭资源回收后调用
int rabbitmq_reset_conn(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    else{
        //todo 设置连接状态 0-未打开
        rabbitmqConnsInfo.conns[conn_index].status = 0;
        //todo 设置连接 当前处于活动的任务
        rabbitmqConnsInfo.conns[conn_index].task_nums=0;
        //todo 设置连接相关指针 NULL
        rabbitmqConnsInfo.conns[conn_index].connState =NULL;
        //todo 设置连接套接字 NULL
        rabbitmqConnsInfo.conns[conn_index].socket=NULL;

        //todo 通道状态 0-未启用
        return rabbitmq_reset_channels(conn_index);
    }
}
//重置所有连接
int rabbitmq_reset_conns(){
    int size = rabbitmqConnsInfo.size;
    if(size > CONNECTION_MAX_SIZE){
        warnLn("conns: size=%d,max=%d", size, CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
            flag&=rabbitmq_reset_conn(i);
            if(flag==0){
                warnLn("conn[%d] reset: fail", i);
                break;
            }
        }
        return flag;
    }

}


//todo close函数
int rabbitmq_close_channel(int conn_index,int channel_index){
    //todo 释放资源
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        return 0;
    }

    int res=1;
    if(
        rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].flag_reset==0
        && rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status > 0
    ){
        amqp_rpc_reply_t reply = amqp_channel_close(
            rabbitmqConnsInfo.conns[conn_index].connState,
            rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num,
            AMQP_REPLY_SUCCESS
        );
        res&=die_on_amqp_error(reply,"closing channel")==0?1:0;
//        if(die_on_amqp_error(reply,"closing channel")==0){
//            res&=1;
//        }else{
//            res&=0;
//        }
    }

    //todo reset
    res&=rabbitmq_reset_channel(conn_index,channel_index);

    return res;
}
int rabbitmq_close_channels(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    int size = rabbitmqConnsInfo.conns[conn_index].channelsInfo.size;
    if(size > CHANNEL_MAX_SIZE){
        warnLn("conns: size=%d,max=%d", size, CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < size; ++i) {
            flag&=rabbitmq_close_channel(conn_index,i);
            if(flag==0){
                warnLn("conn[%d]:channel[%d] close fail", conn_index, i);
                break;
            }
        }
        return flag;
    }
}
int rabbitmq_close_conn(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    else{
        int res=1;

        //todo 根据连接的状态释放资源
        int conn_status = rabbitmqConnsInfo.conns[conn_index].status;
        if(conn_status == 5){
            //todo 关闭通道
            //不需要单独再关闭通道，只要保证连接被关闭即可
            //注：当连接断开时，无需处理通道和连接的关闭，经测试，连接已关闭情况下再关闭通道或连接会导致错误（卡死）
//            if(
//                rabbitmqConnsInfo.conns[conn_index].flag_reset==0
//                &&rabbitmq_close_channels(conn_index)==0
//            ){
//                return 0;
//            }
        }
        if(conn_status >= 2){
            //todo 关闭连接
            //注：当连接断开时，无需处理通道和连接的关闭，经测试，某些连接已关闭情况下再关闭通道或连接会导致错误（卡死）
            if(rabbitmqConnsInfo.conns[conn_index].flag_reset==0){
                amqp_rpc_reply_t reply= amqp_connection_close(
                    rabbitmqConnsInfo.conns[conn_index].connState,
                    AMQP_REPLY_SUCCESS
                );
                if(die_on_amqp_error(reply,"closing connection")==1){
                    res&=0;
                }
            }
        }
        if(conn_status >= 1){
            //todo 销毁连接对象
            amqp_destroy_connection(
                    rabbitmqConnsInfo.conns[conn_index].connState
            );
        }
        //todo reset连接资源(包括reset通道)
        res&=rabbitmq_reset_conn(conn_index);
        return res;
    }
}
int rabbitmq_close_conns(){
    int size = rabbitmqConnsInfo.size;
    if(size <= 0 ||size > CONNECTION_MAX_SIZE){
        warnLn("conns: size=%d,max=%d", size, CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < size; ++i) {
            flag&=rabbitmq_close_conn(i);
            if(flag==0){
                warnLn("conn[%d]: close fail", i);
                break;
            }
        }
        return flag;
    }
}

//todo init函数 资源申请
int rabbitmq_init_channel(int conn_index,int channel_index){
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        return 0;
    }
    else{
        int res=0;
        amqp_connection_state_t conn = rabbitmqConnsInfo.conns[conn_index].connState;
        //todo 打开通道
        if(conn!=NULL){
            int retry=0;
            while(1){
                amqp_channel_open(
                        conn,
                        rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num
                );
                if(die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel")==0){
                    //todo 修改通道状态
                    rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status = 1;

                    res = 1;
                    break;
                }
                else{
                    if(retry>5){
                        warnLn("channel init: has tried 5 times,but fail");
                        break;
                    }
                    warnLn("channel init: open fail,num=%d",
                           rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num);

                    //todo 通道打开失败 修改当前的通道号 new_channel=old_channel+size
                    rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num += CHANNEL_MAX_SIZE;
                    retry++;
                }
            }
        }
        return res;
    }
}
int rabbitmq_init_channels(int conn_index){

    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }

    int size = rabbitmqConnsInfo.conns[conn_index].channelsInfo.size;
    if(size <= 0 ||size > CHANNEL_MAX_SIZE){
        warnLn("channel: size=%d,max=%d", size, CHANNEL_MAX_SIZE);
        return 0;
    }

    else{
        int flag=1;
        for (int i = 0; i < rabbitmqConnsInfo.conns[conn_index].channelsInfo.size; ++i) {
            flag&=rabbitmq_init_channel(conn_index,i);
            if(flag==0){
                warnLn("channel[%d] int: fail", i);
                break;
            }
        }
        return flag;
    }
}
int rabbitmq_login_conn(amqp_connection_state_t conn){
    int res=0;
    amqp_rpc_reply_t reply = amqp_login(
        conn,
        "/",
        0,//channel_max
        131072,//frame_max
        0,//heartbeat
        AMQP_SASL_METHOD_PLAIN,//内部用户密码登录 | 外部系统登录
        rabbitmqConfigInfo.user,
        rabbitmqConfigInfo.passwd
    );
    if(die_on_amqp_error(reply, "login")==0){
        res = 1;
    }
    return res;
}
int rabbitmq_init_conn(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    else{
        rabbitmqConnsInfo.conns[conn_index].status=0;

        //todo 创建连接结构体数据
        amqp_connection_state_t pConn = amqp_new_connection();
        rabbitmqConnsInfo.conns[conn_index].status=1;
        if(pConn == NULL){
            warnLn("conn[%d] init: create fail", conn_index);
            rabbitmq_close_conn(conn_index);
            return 0;
        }else{
            warnLn("conn[%d] init: created", conn_index);
        }
        rabbitmqConnsInfo.conns[conn_index].connState = pConn;
        
        //todo 创建tcp套接字
        amqp_socket_t *socket = amqp_tcp_socket_new(pConn);
        rabbitmqConnsInfo.conns[conn_index].status=2;
        if(socket == NULL){
            warnLn("conn[%d] init: socket create fail", conn_index);
            rabbitmq_close_conn(conn_index);
            return 0;
        }else{
            warnLn("conn[%d] init: socket created", conn_index);
        }
        
        
        //todo 打开TCP连接
        struct timeval timeout={.tv_sec=5,.tv_usec=0};
        int res=amqp_socket_open_noblock(
            socket,
            rabbitmqConfigInfo.hostname,
            rabbitmqConfigInfo.port,
            &timeout
        );
        if(res != AMQP_STATUS_OK){
            warnLn("conn[%d] init: socket open fail,host=%s,port=%d",
               conn_index,
               rabbitmqConfigInfo.hostname,
               rabbitmqConfigInfo.port
            );
            rabbitmq_close_conn(conn_index);
            return 0;
        }else{
            warnLn("conn[%d] init: socket open success", conn_index);
        }

        rabbitmqConnsInfo.conns[conn_index].status=3;
        rabbitmqConnsInfo.conns[conn_index].socket = socket;

        //todo 连接的登录
        if(rabbitmq_login_conn(pConn)==0){
            warnLn("conn[%d] init: login fail", conn_index);
            rabbitmq_close_conn(conn_index);
            return 0;
        }else{
            warnLn("conn[%d] init: login success", conn_index);
        }
        rabbitmqConnsInfo.conns[conn_index].status=4;

        //todo 初始化该连接下的通道
        if(rabbitmq_init_channels(conn_index)==0){
            rabbitmq_close_conn(conn_index);
            return 0;
        }
        rabbitmqConnsInfo.conns[conn_index].status=5;

        return 1;
    }
}
int rabbitmq_init_conns(){
    int size = rabbitmqConnsInfo.size;
    if(size <= 0 ||size > CONNECTION_MAX_SIZE){
        warnLn("conns: size=%d,max=%d", size, CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        //todo 重置连接数据
        if(rabbitmq_reset_conns()==0){
            return 0;
        }

        //todo 初始化连接数据
        int flag=1;
        for (int i = 0; i < size; ++i) {
            flag &= rabbitmq_init_conn(i);//todo 需要保证初始化失败时释放已申请的连接资源，并完成重置
            if(flag==0){
                warnLn("conn[%d] init: fail", i);
                break;
            }
        }
        return flag;
    }

};

int rabbitmq_init_queue(int queue_index){
    if(rabbitmq_check_queue_index(queue_index) == 0){
        return 0;
    }
    else{
        //todo 连接和通道
        amqp_connection_state_t connState = rabbitmqConnsInfo.conns[0].connState;
        int channel_num = rabbitmqConnsInfo.conns[0].channelsInfo.channels[0].num;

        //todo 队列基本参数
        RabbitmqQueueEntity_t queue = queuesInfo.queues[queue_index];
        amqp_bytes_t queue_name = amqp_cstring_bytes(queue.name);
//    queue.passive;
//    queue.durability;
//    queue.exclusive;
//    queue.auto_delete;

        //todo 解析拓展参数
        amqp_table_entry_t kvs[9];
        amqp_table_t xargs;
        xargs.entries=kvs;
        int i=0;
        while(queue.args[i].key != NULL){
            //todo key解析
            xargs.entries[i].key = amqp_cstring_bytes(queue.args[i].key);

            //todo value整数解析
            if(queue.args[i].type==0){
                xargs.entries[i].value.kind=AMQP_FIELD_KIND_I32;
                xargs.entries[i].value.value.i32 = queue.args[i].value.integer;
            }
                //todo value字符串解析
            else if(queue.args[i].type==1){
                xargs.entries[i].value.kind=AMQP_FIELD_KIND_UTF8;
                xargs.entries[i].value.value.bytes = amqp_cstring_bytes(queue.args[i].value.str);
            }
            i++;
        }
        if(i!=0){
            xargs.num_entries=i;
        }
        else{
            xargs=amqp_empty_table;
        }

        //todo 检测
        amqp_queue_declare_ok_t *r = amqp_queue_declare(
                connState,//连接对象
                channel_num,//channel
                queue_name,//队列名称
                queue.passive,//passive 0-队列不存在会自动创建，对了存在检查参数是否匹配，不匹配返回错误
                queue.durability,//durable     队列元数据持久化，不保证数据不丢失
                queue.exclusive,//exclusive
                queue.auto_delete,//auto_delete 无消费者在自动删除
                xargs//额外参数 (通常用amqp_empty_table)
        );
        return die_on_amqp_error(amqp_get_rpc_reply(connState),"check queue fail")==1?0:1;
    }
}
int rabbitmq_init_exchange(int exchange_index){
    if(rabbitmq_check_exchange_index(exchange_index) == 0){
        return 0;
    }
    else{
        //todo 连接和通道
        amqp_connection_state_t connState = rabbitmqConnsInfo.conns[0].connState;
        int channel_num = rabbitmqConnsInfo.conns[0].channelsInfo.channels[0].num;
        //todo 交换机基本参数
        RabbitmqExchangeEntity exchange = exchangesInfo.exchanges[exchange_index];
        amqp_bytes_t exchange_name = amqp_cstring_bytes(exchange.name);

        amqp_bytes_t exchangeType = amqp_cstring_bytes(exchange.type);

        //todo 拓展参数
        amqp_table_t xargs;
        if(exchange.args[0].key!=NULL){
            amqp_table_entry_t kvs;
            xargs.num_entries=1;
            xargs.entries=&kvs;
            kvs.key = amqp_cstring_bytes(exchange.args[0].key);
            kvs.value.kind = AMQP_FIELD_KIND_UTF8;
            kvs.value.value.bytes = amqp_cstring_bytes(exchange.args[0].value.str);
        }
        else{
            xargs=amqp_empty_table;
        }

        //todo 检测
        amqp_exchange_declare(
                connState,
                channel_num,                       //channel
                exchange_name,//exchange
                exchangeType,//type
                exchange.passive,//passive 检查模式
                exchange.durability,//durable
                exchange.autoDelete,//autoDelete
                exchange.internal,//internal 内部交换机
                xargs
        );
        return die_on_amqp_error(amqp_get_rpc_reply(connState),"check exchange fail")==1?0:1;
    }
}
int rabbitmq_init_bind(int bind_index){
    if(rabbitmq_check_bind_index(bind_index) == 0){
        return 0;
    }
    else{
        RabbitmqBindEntity_t bind = bindsInfo.binds[bind_index];

        if(rabbitmq_check_exchange_index(bind.exchange_index) == 0){
            warnLn("bind check: fail ,exchange_index=%d", bind.exchange_index);
            return 0;
        }
        if(rabbitmq_check_queue_index(bind.queue_index) == 0){
            warnLn("bind check: fail ,queue_index=%d", bind.queue_index);
            return 0;
        }

        //todo 连接和通道
        amqp_connection_state_t connState = rabbitmqConnsInfo.conns[0].connState;
        int channel_num = rabbitmqConnsInfo.conns[0].channelsInfo.channels[0].num;
        amqp_bytes_t queue_name = amqp_cstring_bytes(queuesInfo.queues[bind.queue_index].name);
        amqp_bytes_t exchange_name = amqp_cstring_bytes(exchangesInfo.exchanges[bind.exchange_index].name);
        amqp_bytes_t routingKey = amqp_cstring_bytes(bind.routingKey);

        //todo 绑定
        amqp_queue_bind_ok_t *pT = amqp_queue_bind(
                connState,
                channel_num,              //channel
                queue_name,      //queue
                exchange_name, //exchange
                routingKey,  //bindingkey
                amqp_empty_table
        );
        return die_on_amqp_error(amqp_get_rpc_reply(connState), "Binding queue" )==1?0:1;
    }
}
//消费端和生产端任务初始化
int rabbitmq_init_consumer(int consumer_index){

    //todo 检查索引下标(消费者、连接、通道、队列)
    if(consumer_index<0 || consumer_index>=CONSUMER_MAX_SIZE){
        warnLn("consumer: index=%d,max=%d", consumer_index, CONSUMER_MAX_SIZE);
        return 0;
    }

    int conn_index = consumersInfo.consumers[consumer_index].conn_index;
    int channel_index = consumersInfo.consumers[consumer_index].channel_index;
    int queue_index = consumersInfo.consumers[consumer_index].queue_index;

    //todo 分配可用通道
    //第一个通道默认不用来处理生产者或消费者任务
    if(channel_index==0){
        channel_index = get_available_channel(conn_index);
        if(channel_index==0){
            warnLn("consumer[%d]: no available channel", consumer_index);
            return 0;
        }
        consumersInfo.consumers[consumer_index].channel_index=channel_index;
        warnLn("consumer[%d]: can't use the first channel,change conn[%d].channel_index=%d", consumer_index, conn_index,
               channel_index);
    }
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        warnLn("consumer[%d]: channel[%d] is not exists", consumer_index, channel_index);
        return 0;
    }
    //队列检查
    if(rabbitmq_check_queue_index(queue_index) == 0){
        warnLn("consumer[%d]: queue[%d] index check fail", consumer_index, queue_index);
        return 0;
    }

    //检查任务
    if(consumersInfo.consumers[consumer_index].taskInfo.task==NULL){
        warnLn("consumer[%d]: task is null", consumer_index);
        return 0;
    }

    //消费者参数
    amqp_connection_state_t connState = rabbitmqConnsInfo.conns[conn_index].connState;
    int channel = rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num;
    amqp_bytes_t queue_name = amqp_cstring_bytes(queuesInfo.queues[queue_index].name);
    amqp_bytes_t consumer_tag = amqp_cstring_bytes(consumersInfo.consumers[consumer_index].consumer_tag);
    int no_local = consumersInfo.consumers[consumer_index].no_local;
    int no_ack = consumersInfo.consumers[consumer_index].no_ack;
    int exclusive = consumersInfo.consumers[consumer_index].exclusive;

    //启动消息消费
    //告诉 RabbitMQ 服务器开始向客户端推送指定队列中的消息
    amqp_basic_consume(
            connState,
            channel,
            queue_name,
            consumer_tag,// 消费者标签(空字节则服务器自动生成)
            no_local,//no_local 是否接收自己发布的消息
            no_ack,//no_ack 0-手动ACK 1-自动ACK
            exclusive,//exclusive 排他消费
            amqp_empty_table//额外参数 (通常用amqp_empty_table)
    );
    if(die_on_amqp_error(amqp_get_rpc_reply(connState), "consumer") == 1){
        return 0;
    }
    
    //todo 任务信息
    consumersInfo.consumers[consumer_index].taskInfo.status=CONSUMER_TASK_WAIT_MESSAGE;
    consumersInfo.consumers[consumer_index].taskInfo.type=1;//1-消费者
    consumersInfo.consumers[consumer_index].taskInfo.index=consumer_index;
    consumersInfo.consumers[consumer_index].taskInfo.execInfo.code=0;
    consumersInfo.consumers[consumer_index].taskInfo.execInfo.info=NULL;
    
    //todo 统计 连接 的任务数量
    consumerEntity_t consumer = consumersInfo.consumers[consumer_index];
    rabbitmqConnsInfo.conns[consumer.conn_index].task_nums++;

    //todo 向通道 注册 任务信息的引用
    rabbitmqConnsInfo.conns[consumer.conn_index].channelsInfo.channels[channel_index].taskInfo = &(consumersInfo.consumers[consumer_index].taskInfo);

    //todo 修改通道状态 2-已使用
    rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status=2;
    return 1;

}
int rabbitmq_init_producer(int producer_index){

    //todo 检查索引下标(生产者、连接、通道、交换机)
    if(
        (producer_index>=PRODUCER_MAX_SIZE)
        ||(producer_index<0 || producer_index>=producersInfo.size)
    ){
        warnLn("producer: index=%d,max=%d", producer_index, PRODUCER_MAX_SIZE);
        return 0;
    }

    int conn_index = producersInfo.producers[producer_index].conn_index;
    int channel_index = producersInfo.producers[producer_index].channel_index;
    int exchange_index = producersInfo.producers[producer_index].exchange_index;

    //todo 分配可用通道
    //第一个通道默认不用来处理生产者或消费者任务
    if(channel_index==0){
        channel_index = get_available_channel(conn_index);
        if(channel_index==0){
            warnLn("producer[%d]: no available channel", producer_index);
            return 0;
        }
        producersInfo.producers[producer_index].channel_index=channel_index;
        warnLn("producer[%d]: can't use the first channel,change conn[%d].channel_index=%d", producer_index, conn_index,
               channel_index);
    }
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        warnLn("producer[%d]: channel[%d] is not exists", producer_index, channel_index);
        return 0;
    }
    //交换机
    if(rabbitmq_check_exchange_index(exchange_index) == 0){
        warnLn("producer[%d]: exchange[%d] index check fail", producer_index, exchange_index);
        return 0;
    }
    //检查任务
    if(producersInfo.producers[producer_index].taskInfo.task==NULL){
        warnLn("producer[%d]: task is null", producer_index);
        return 0;
    }

    //生产者参数
    amqp_connection_state_t connState = rabbitmqConnsInfo.conns[conn_index].connState;
    int channel = rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num;
    int confirmMode = producersInfo.producers[producer_index].confirmMode;


    //启动发布确认
    if(confirmMode==1){
        amqp_confirm_select(
            connState,
            channel
        );
        if(die_on_amqp_error(amqp_get_rpc_reply(connState), "Enable confirm-select")==1){
            return 0;
        }
    }

    //todo 任务信息
    producersInfo.producers[producer_index].taskInfo.status=PRODUCER_TASK_WAIT_DATA;//1-等待中
    producersInfo.producers[producer_index].taskInfo.type=0;//0-生产者
    producersInfo.producers[producer_index].taskInfo.index=producer_index;
    producersInfo.producers[producer_index].taskInfo.execInfo.code=0;
    producersInfo.producers[producer_index].taskInfo.execInfo.info=NULL;

    //todo 统计 连接 的任务数量
    producerEntity_t_t producer = producersInfo.producers[producer_index];
    rabbitmqConnsInfo.conns[producer.conn_index].task_nums++;

    //todo 向通道 注册 任务信息的引用
    rabbitmqConnsInfo.conns[producer.conn_index].channelsInfo.channels[producer.channel_index].taskInfo=&(producersInfo.producers[producer_index].taskInfo);

    //todo 修改通道状态 2-已使用
    rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status=2;

    return 1;
}
int init_role_by_taskInfo(taskInfo_t *taskInfo){
    if(taskInfo->type==0){//0-生产者
        if(rabbitmq_init_producer(taskInfo->index)==0){
            return 0;
        }
    }
    else if(taskInfo->type==1){//1-消费者
        if(rabbitmq_init_consumer(taskInfo->index)==0){
            return 0;
        }
    }
    return 1;
}
int init_roles_of_conn(int conn_index){
    connectionEntity conn = rabbitmqConnsInfo.conns[conn_index];
    for (int i = 0; i < conn.channelsInfo.size; ++i) {
        taskInfo_t *taskInfo = conn.channelsInfo.channels[i].taskInfo;
        if(taskInfo==NULL){
            continue;
        }
        else{
            if(init_role_by_taskInfo(taskInfo) == 0){
                return 0;
            }
        }

    }
    return 1;
}

//todo start函数
//生产端
int rabbitmq_start_producer(int index){
    //todo 初始化生产者结构体
    if(rabbitmq_init_producer(index)==0){
        warnLn("producer[%d]: init fail", index);
        return 0;
    }

    //todo 启动线程
    return pthread_create(
            &producersInfo.producers[index].taskInfo.thread_handle,
            NULL,
            rabbitmq_task,
            &producersInfo.producers[index].taskInfo
    )==0?1:0;
}
int rabbitmq_start_producers(){
    if(producersInfo.size<0 || producersInfo.size>PRODUCER_MAX_SIZE){
        warnLn("producers start: size=%d,max=%d", producersInfo.size, PRODUCER_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < producersInfo.size; ++i) {
            flag&=rabbitmq_start_producer(i);
            if(flag==0){
                warnLn("producer[%d] start: fail", i);
                break;
            }
        }
        return flag;
    }

}
//消费端
int rabbitmq_start_consumer(int index){
    //todo 初始化消费者结构体
    if(rabbitmq_init_consumer(index)==0){
        warnLn("consumer[%d]: init fail", index);
        return 0;
    }

    //todo 启动线程
    return pthread_create(
            &consumersInfo.consumers[index].taskInfo.thread_handle,
            NULL,
            rabbitmq_task,
            &consumersInfo.consumers[index].taskInfo
    )==0?1:0;
}
int rabbitmq_start_consumers(){
    if(consumersInfo.size<0 || consumersInfo.size>CONSUMER_MAX_SIZE){
        warnLn("consumers start: size=%d,max=%d", consumersInfo.size, CONSUMER_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < consumersInfo.size; ++i) {
            flag&=rabbitmq_start_consumer(i);
            if(flag==0){
                warnLn("consumer[%d] start: fail");
                break;
            }
        }
        return flag;
    }
}


//todo tools函数
//分配指定连接下的可用通道
int get_available_channel(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    else{
        int res=0;
        channelInfo_t channelsInfo = rabbitmqConnsInfo.conns[conn_index].channelsInfo;
        for (int i = 1; i < channelsInfo.size; ++i) {
            //1-可用
            if(channelsInfo.channels[i].status==1){
                res=i;
                break;
            }
        }
        return res;
    }
}
//根据code设置执行信息
char *get_code_info(int exit_code){
    switch (exit_code) {
        case EXEC_ERROR:
            return "exec with error";
        case EXEC_NORMAL:
            return "exec normally";
        case EXEC_CORRECT:
            return "exec correct";

        case EXEC_CONN_CLOSED:
            return "conn closed";
        case EXEC_CHANNEL_CLOSED:
            return "channel closed";

        case EXEC_PRODUCE_DATA_PREPARE_FAIL:
            return "data prepare fail";
        case EXEC_PRODUCER_PUBLISH_FAIL:
            return "publish fail";
        case EXEC_PRODUCER_CONFIRM_FAIL:
            return "confirm fail";

        case EXEC_CONSUMER_MESSAGE_GET_FAIL:
            return "message get fail";

        case EXEC_UNKNOWN_FRAME:
            return "unknown frame";
        case EXEC_NOTIFIED_STOP:
            return "notified stop";

        default:
            return "unknown";
    }
}
//获取单个连接下的任务数量
//int get_task_num_of_conn(int conn_index){
//    channelInfo_t channelsInfo = rabbitmqConnsInfo.conns[conn_index].channelsInfo;
//    int res=0;
//    for (int i = 0; i < channelsInfo.size; ++i) {
//        if(channelsInfo.channels[i].status==2){
//            res++;
//        }
//    }
//    return res;
//}




//todo task函数

//todo task函数
//任务模板
void *rabbitmq_task(void * arg){

    //todo 任务信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    //todo 注册退出回调函数
    pthread_cleanup_push(clean_synchronize_resources,&global_task_mutex);

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
//生产端状态机处理
int get_an_message(void *arg){
    //todo 自定义返回信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;

    consumerEntity_t *consumerInfo = &(consumersInfo.consumers[taskInfo->index]);
    connectionEntity *connInfo = &(rabbitmqConnsInfo.conns[consumerInfo->conn_index]);
    //参数
    amqp_connection_state_t connState = connInfo->connState;
    int channel = connInfo->channelsInfo.channels[consumerInfo->channel_index].num;

    {
        amqp_rpc_reply_t res={};//
        amqp_frame_t frame={};
        amqp_envelope_t *envelope=(amqp_envelope_t *)(taskInfo->execInfo.data);//用于存储消息内容
//        amqp_envelope_t envelope;
//        int success=0;

        //用于 内存优化 的关键函数
        //  注：在还未处理完消息内容(如访问envelope.message.body)前调用会导致数据丢失
//        amqp_maybe_release_buffers(connState);//，控制着库内部缓冲区的内存释放行为
        amqp_maybe_release_buffers_on_channel(connState, channel);

        //todo 非阻塞获取队列最新消息
        struct timeval timeout;
        timeout.tv_sec=0;
        timeout.tv_usec=0;
        res = amqp_consume_message(connState, envelope, &timeout, 0);

        //todo 处理消息
        if (AMQP_RESPONSE_NORMAL == res.reply_type) {
            //todo 将消息内容填充到taskInfo

            return EXEC_CORRECT;//1-结果正常
        }
        //todo 异常和非预期帧处理
        else{
            if(AMQP_RESPONSE_LIBRARY_EXCEPTION == res.reply_type){
                //todo 超时处理
                if(AMQP_STATUS_TIMEOUT == res.library_error){
                    return EXEC_NORMAL;//0-正常执行(保持状态)
                }
                else if(AMQP_STATUS_UNEXPECTED_STATE == res.library_error){
                    //根据amqp_consume_message注释信息：此时的异常处理表示收到了 AMQP_BASIC_DELIVER_METHOD 以外的帧，
                    // 则调用方应调用 amqp_simple_wait_frame（） 来读取此帧并采取适当的操作。
                    // 主要处理跟连接关闭、通道关闭帧
                    if (AMQP_STATUS_OK != amqp_simple_wait_frame(connState, &frame)) {
                        return EXEC_CONSUMER_MESSAGE_GET_FAIL;
                    }
                    //1.METHOD帧 方法帧
                    if (AMQP_FRAME_METHOD == frame.frame_type) {
//                    frame.payload.method.id;//方法帧（类和方法信息）
//                    frame.payload.method.decoded;//方法帧（参数信息）
                        switch (frame.payload.method.id) {
                            //todo 需要处理的帧(连接、通道的关闭)
                            case AMQP_CHANNEL_CLOSE_METHOD:
                            {
                                warnLn("consumer[%d]: AMQP_CHANNEL_CLOSE_METHOD", taskInfo->index);
                                //todo 解析关闭原因
                                amqp_channel_close_t *r = (amqp_channel_close_t *) frame.payload.method.decoded;
                                warnLn(r->reply_text.bytes);

                                return EXEC_CHANNEL_CLOSED;
                            }
                            //connect已关闭
                            case AMQP_CONNECTION_CLOSE_METHOD:
                            {
                                //对同一个deliveryTag进行多次ack也会触发
                                warnLn("consumer[%d]: AMQP_CONNECTION_CLOSE_METHOD", taskInfo->index);
                                //todo 需要重新打开连接
                                amqp_connection_close_t *r = (amqp_connection_close_t *) frame.payload.method.decoded;
                                warnLn(r->reply_text.bytes);

                                return EXEC_CONN_CLOSED;//2-连接已关闭
                            }
                            //其它非预期帧(除AMQP_BASIC_DELIVER_METHOD和连接相关以外的帧)
                            default:
                            {
                                //todo 非预期帧(以下帧无需消费端特别处理)
//                            AMQP_BASIC_ACK_METHOD
//                            AMQP_BASIC_RETURN_METHOD
//                            AMQP_BASIC_DELIVER_METHOD
                                warnLn("consumer[%d]: received other frame", taskInfo->index);
                                return EXEC_UNKNOWN_FRAME;
                            }
                        }
                    }
                }
            }

            return 0;
        }
    }
}
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
        else if (work_status == CLIENT_STATUS_RESETTING_CONN) {
            if(task_wait_main_reset_conn(conn_index)==1){
                taskInfo->status = CONSUMER_TASK_WAIT_MESSAGE;
            }
            else{
                //此处无处理，
                // 但能保证当main重置连接失败时，后续可见：
                // work_status==CLIENT_STATUS_STOPPING_TASKS
            }
        }
        else if (work_status == CLIENT_STATUS_RESETTING_CHANNEL) {
            if(task_wait_main_reset_channel(conn_index, channel_index)==1){
                taskInfo->status = CONSUMER_TASK_WAIT_MESSAGE;
            }else{
                //此处无处理，
                // 但能保证当main重置通道失败时，后续可见：
                // work_status==CLIENT_STATUS_STOPPING_TASKS
            }
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
//生产端状态机处理
void notify_message_publish_result(taskInfo_t *taskInfo,int success){
    char *msg=NULL;
    if(taskInfo->status==PRODUCER_TASK_PUBLISH){
        msg="publish";
    }
    else if(taskInfo->status==PRODUCER_TASK_CONFIRM){
        msg="confirm";
    }
//    warnLn("-------------------------------------------");
    warnLn("producer[%d]:%s %s",taskInfo->index,msg,success==1?"success":"fail");

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
//    int confirmMode = producerInfo->confirmMode;
    int mandatory = producerInfo->mandatory;
    int immediate = producerInfo->immediate;

    amqp_basic_properties_t *props = &(producerInfo->props);

    amqp_maybe_release_buffers(connState);

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
        return EXEC_CONN_CLOSED;
//        //todo 收到非ack帧
//        if (AMQP_STATUS_UNEXPECTED_STATE == ret.library_error) {
//            taskInfo->execInfo.info="AMQP_STATUS_UNEXPECTED_STATE is not ack frame";
//        }
//        //todo 等待确认已超时
//        else if (AMQP_STATUS_TIMEOUT == ret.library_error) {
//            // Timeout means you're done; no publisher confirms were waiting!
//            taskInfo->execInfo.info="AMQP_STATUS_TIMEOUT";
//        }
//        return EXEC_PRODUCER_CONFIRM_FAIL;//6-发布ACK失败
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
            sleep(1);
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
            if(task_wait_main_reset_conn(conn_index)==1){
                taskInfo->status = PRODUCER_TASK_WAIT_DATA;//1-等待中
            }
            else{
                //此处无处理，
                // 但能保证当main重置连接失败时，后续可见：
                // work_status==CLIENT_STATUS_STOPPING_TASKS
            }
        }
        else if (work_status == CLIENT_STATUS_RESETTING_CHANNEL) {
            if(task_wait_main_reset_channel(conn_index, channel_index)==1){
                taskInfo->status = PRODUCER_TASK_WAIT_DATA;//1-等待中
            }
            else{
                //此处无处理，
                // 但能保证当main重置通道失败时，后续可见：
                // work_status==CLIENT_STATUS_STOPPING_TASKS
            }
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
                else if(work_status==CLIENT_STATUS_STOPPING_TASKS){
                    taskInfo->execInfo.code = EXEC_NOTIFIED_STOP;//10-被通知停止运行
                    taskInfo->status=PRODUCER_TASK_HANDLE_EXCEPTION;//4-异常处理
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
//                connectionEntity *connInfo = &(rabbitmqConnsInfo.conns[producerInfo->conn_index]);
                //todo 发布一条消息
                int res=publish_an_message(taskInfo);
                taskInfo->execInfo.code=res;

                if(res==EXEC_CORRECT){
                    notify_message_publish_result(taskInfo,1);
                    //todo 检查发布确认是否开启
                    if(producerInfo->confirmMode == 1) {
                        //已发送且需要等待确认
                        taskInfo->status=PRODUCER_TASK_CONFIRM;//3-发布确认中
                        break;
                    }
                    else{
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
//todo 业务函数 非阻塞 后续扩展阻塞支持
//下拉数据
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
//        taskInfo->execInfo.data;
//        warnLn("===========================================");
//        info(stdout, "producer[%d]>",taskInfo->index);
//        fgets(taskInfo->execInfo.data,255,stdin);

        sleep(5);
        static int data=0;
        sprintf(taskInfo->execInfo.data,"%d",data++);
        warnLn("===========================================");
        infoLn("producer[%d]: prepared data\nlen=%d\n>%s",
               taskInfo->index,
               strlen(taskInfo->execInfo.data),
               taskInfo->execInfo.data
        );

        return EXEC_CORRECT;
    }

    //暂无数据
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
//        sleep(10);
//        return EXEC_PRODUCE_DATA_PREPARE_FAIL;
//    }

}


//todo handle函数
int main_handle_reset_channels(){

    for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
        channelInfo_t *channelsInfo = &rabbitmqConnsInfo.conns[i].channelsInfo;
        for (int j = 0; j < channelsInfo->size; ++j) {
            channelEntity_t *channel = &(channelsInfo->channels[j]);
            if(channel->flag_reset == 1){
                //todo 关闭通道
                if(rabbitmq_close_channel(i,j)==0){
                    warnLn("main: channel close fail");
                    return 0;
                }

                //todo 初始化通道
                if(rabbitmq_init_channel(i,j)==0){
                    warnLn("main: channel reopen fail");
                    return 0;
                }

                //todo 初始化角色(生产者和消费者)
                if(init_role_by_taskInfo(channel->taskInfo)==0){
                    warnLn("main: init roles fail");
                    return 0;
                }

                warnLn("main: conn[%d].channel[%d] reopened", i, j);


                //todo 清除重置标志
                channel->flag_reset=0;
                //todo 通知等待该通道重置道德任务线程
                pthread_cond_broadcast(&(channelsInfo->cond_reset_channel));
            }
        }
    }

    return 1;
}
int main_handle_reset_conns(){
    //todo 初始化被关闭的连接
    for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
        if (rabbitmqConnsInfo.conns[i].flag_reset == 1){
            //todo 关闭 连接释放资源
            if(rabbitmq_close_conn(i)==0){
                warnLn("main: conn[%d] close fail", i);
                return 0;
            }
            //todo 初始化 连接
            if(rabbitmq_init_conn(i)==0){
                warnLn("main: conn reopen fail");
                return 0;
            }
            //todo 初始化角色(生产者和消费者)
            if(init_roles_of_conn(i) == 0){
                warnLn("main: init roles fail");
                return 0;
            }

            warnLn("main: conn[%d] reopened", i);


            //todo 清除重置标志
            rabbitmqConnsInfo.conns[i].flag_reset=0;
            //todo 通知等待该连接重置的任务线程
            pthread_cond_broadcast(&rabbitmqConnsInfo.cond_reset_conn);
        }
    }
    return 1;
}


//todo client客户端函数
int rabbitmq_init_client(){

    //todo 全局变量
    flag_running=0;
    flag_stop=0;
    flag_exit=0;
    flag_reset_conn=0;
    flag_reset_channel=0;

    //todo 初始化连接和通道(失败能自动释放连接资源)
    if(rabbitmq_init_conns()==0){
        return 0;
    }

    else{
        //todo 交换机
        for (int i = 0; i < exchangesInfo.size; ++i) {
            if(rabbitmq_init_exchange(i)==0){
                warnLn("exchange[%d]: init error", i);
                return 0;
            }
        }
        //todo 队列
        for (int i = 0; i < queuesInfo.size; ++i) {
            if(rabbitmq_init_queue(i)==0){
                warnLn("queue[%d]: init error", i);
                return 0;
            }
        }
        //todo 绑定关系
        for (int i = 0; i < bindsInfo.size; ++i) {
            if(rabbitmq_init_bind(i)==0){
                warnLn("bind[%d]: init error");
                return 0;
            }
        }
        return 1;
    }

}
int rabbitmq_start_client(){

    //todo 初始化同步工具（锁、条件变量）
    init_synchronize_tools();

    if(rabbitmq_init_client()==0){
        warnLn("rabbitmq client: init fail");
        return 0;
    }
    else{
//        //todo 设置流的缓冲类型 无缓冲
//        setbuf(stdout, NULL);
//        setvbuf()

        //todo main线程任务：负责连接和通道的申请和关闭，控制子线程的运行
        if(producersInfo.size+consumersInfo.size==0)
        {
            warnLn("main: no ask");
        }
        else{

            //todo 启动任务
            if(
                rabbitmq_start_consumers()==0
                ||rabbitmq_start_producers()==0
            ){
                warnLn("rabbitmq client: start task fail");
                return 0;
            };

            //todo main处理
            warnLn("main: get lock");
            pthread_mutex_lock(&global_task_mutex);
            warnLn("main: locked");

            work_status=CLIENT_STATUS_READY;
            warnLn("main: status=%d", work_status);

            //todo 等待 运行
            while(flag_running!=1){
                warnLn("main: wait cond_running,work_status=%d", work_status);
                pthread_cond_wait(&cond_running, &global_task_mutex);
            }
//            sleep(1);
            work_status=CLIENT_STATUS_RUNNING;
            warnLn("main: status=%d", work_status);

            //todo 等待 stop
            while(flag_stop!=1){
                warnLn("main: wait cond_stop,work_status=%d", work_status);
                //todo 等待 子线程的通知处理
                pthread_cond_wait(&cond_deal, &global_task_mutex);

                //todo 检查是否有连接重置处理
                if(flag_reset_conn == 1){
                    work_status=CLIENT_STATUS_RESETTING_CONN;
                    warnLn("main: status=%d", work_status);

                    //todo 检查和处理连接重置
                    if(main_handle_reset_conns()==0){
                        goto exit;
                    }
                    flag_reset_conn=0;
                }
                //todo 检查是否有通道重置处理
                if(flag_reset_channel == 1){
                    work_status=CLIENT_STATUS_RESETTING_CHANNEL;
                    warnLn("main: status=%d", work_status);

                    //todo 检查和处理通道重置
                    if(main_handle_reset_channels()==0){
                        goto exit;
                    }
                    flag_reset_conn=0;

                }
                work_status=CLIENT_STATUS_RUNNING;
                warnLn("main: status=%d", work_status);
            }
exit:
            work_status=CLIENT_STATUS_STOPPING_TASKS;
            warnLn("main: status=%d", work_status);

            warnLn("main: close all threads,work_status=%d", work_status);

            //todo 等待 所有子线程结束任务 exit
            while(flag_exit!=1){
                //todo 唤醒所有阻塞中的线程
                warnLn("main: wait cond_exit,work_status=%d", work_status);
                pthread_cond_wait(&cond_exit,&global_task_mutex);

            }
            work_status=CLIENT_STATUS_EXIT;
            warnLn("main: status=%d", work_status);


            warnLn("main: release lock,work_status=%d", work_status);
            pthread_mutex_unlock(&global_task_mutex);
            warnLn("main: unlocked,work_status=%d", work_status);


            //todo 释放连接
            rabbitmq_close_conns();

            //todo 打印退出信息
            print_threads_exitInfo();
        }
    }
    //todo 释放锁和条件变量资源
    destroy_synchronize_tools();

    return 1;
}
//////////////////////////////////////
//todo log函数
//日志打印
void vlog(FILE *fd,char *str,va_list args){
    vfprintf(fd,str,args);
}
void infoLn(char *str, ...){
    va_list list;

    pthread_mutex_lock(&log_mutex);

    va_start(list,str);
    vlog(stdout,str,list);
    fprintf(stdout,"\n");
    fflush(stdout);
    va_end(list);

    pthread_mutex_unlock(&log_mutex);
}
void warnLn(char *str, ...){
    va_list list;

    pthread_mutex_lock(&log_mutex);

    va_start(list,str);
    vlog(stderr,str,list);
    fprintf(stderr,"\n");
    fflush(stderr);
    va_end(list);

    pthread_mutex_unlock(&log_mutex);
}
//打印同步信息
/*void print_lock_info(const pthread_mutex_t *mutex){
    warnLn(
            "mutex: {data = {lock = %d, count = %d, owner = %d, nusers = %d, kind = %d, spins = %d, elision = %d}}\n",
            mutex->__data.__lock,
            mutex->__data.__count,
            mutex->__data.__owner,
            mutex->__data.__nusers,
            mutex->__data.__kind,
            mutex->__data.__spins,
            mutex->__data.__elision
    );
}
void print_cond_info(const pthread_cond_t *cond){
    warnLn("cond: {data = {"
           "wseq = {value64 =%d, value32 = {low =%d, high =%d}}, "
           "g1_start = {value64 =%d, value32 = {low =%d, high =%d}}, "
           "g_refs = {%d,%d}, "
           "g_size = {%d,%d}, "
           "g1_orig_size =%d, "
           "wrefs =%d, "
           "g_signals = {%d,%d}}}\n",
           cond->__data.__wseq.__value64,
           cond->__data.__wseq.__value32.__low,
           cond->__data.__wseq.__value32.__high,
           cond->__data.__g1_start.__value64,
           cond->__data.__g1_start.__value32.__low,
           cond->__data.__g1_start.__value32.__high,
           cond->__data.__g_refs[0],
           cond->__data.__g_refs[1],
           cond->__data.__g_size[0],
           cond->__data.__g_size[1],
           cond->__data.__g1_orig_size,
           cond->__data.__wrefs,
           cond->__data.__g_signals[0],
           cond->__data.__g_signals[1]
    );
}
void print_synchronized_info(const pthread_mutex_t *mutex,const pthread_cond_t *cond){
    warnLn("---------------------");
    print_lock_info(mutex);
    print_cond_info(cond);
    warnLn("---------------------");
}*/
//打印消息
void print_rcv_message(amqp_envelope_t *envelope){
    //todo 提取消息内容

    char tmp[128]={0};
    char res[256]={0};
    char *buffer=NULL;

    sprintf(tmp,"from channel[%d]:",envelope->channel);
    strcat(res,tmp);

    strcat(res," exchange=");
    strncat(res,envelope->exchange.bytes,envelope->exchange.len);

    strcat(res,",routing_key=");
    strncat(res,envelope->routing_key.bytes,envelope->routing_key.len);

    sprintf(tmp,",redelivered=%d",envelope->redelivered);
    strcat(res,tmp);

    sprintf(tmp,",delivery_tag=%llu",envelope->delivery_tag);
    strcat(res,tmp);

    infoLn("============================================================");
    //消息基本信息
    infoLn("%s", res);

    //消息内容
    memset(res,0,256);
    if(envelope->message.body.len>256){
        warnLn("message: truncated,len=%d,max=%d", envelope->message.body.len, 256);
        buffer= malloc(envelope->message.body.len+1);
        strncat(buffer,envelope->message.body.bytes,envelope->message.body.len);

        infoLn("content: len=%d\n>%s",
               envelope->message.body.len,//不包括‘\0’
               buffer
        );

        free(buffer);
    }
    else{
        strncat(res,envelope->message.body.bytes,envelope->message.body.len);
        infoLn("content: len=%d\n>%s",
               envelope->message.body.len,
               res
        );
    }
}
void print_send_message(taskInfo_t *taskInfo){

}
//打印线程退出时记录的信息
void print_threads_exitInfo(){
    infoLn("========================(exitInfo)==========================");
    //todo 生产者
    for (int i = 0; i < producersInfo.size; ++i) {
        infoLn("producer[%d] exitInfo:", i);
        infoLn("code: %d", producersInfo.producers[i].taskInfo.execInfo.code);
        infoLn("info: %s", producersInfo.producers[i].taskInfo.execInfo.info);
        infoLn("");
    }

    //todo 消费者
    for (int i = 0; i < consumersInfo.size; ++i) {
        infoLn("consumer[%d] exitInfo:", i);
        infoLn("code: %d", consumersInfo.consumers[i].taskInfo.execInfo.code);
        infoLn("info: %s", consumersInfo.consumers[i].taskInfo.execInfo.info);
        infoLn("");
    }
}










