#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>

#include "rabbitmq-c.h"

pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex;
pthread_cond_t cond_start;
pthread_cond_t cond_exit;
//pthread_barrier_t barrier;
exitInfo_t exitInfo={
    .type=0,
    .info=NULL,
    .index=0
};


RabbitmqConfig_t rabbitmqConfigInfo={
    .hostname="168.192.200.132",
    .port=5672
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
                    //todo 专用通道-非消费相关操作
                    {
                        .num=1,
                        .status=0
                    },
                    //todo 消费通道-线程专用
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
                    //todo 专用通道-非消费相关操作
                    {
//                        .num=1,
                        .status=0
                    },
                    //todo 消费通道-线程专用
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
            .type=0,
            .durability=1,
            .autoDelete=0,
            .internal=0,
            .args={0}
        },
        //todo 死信交换机
        {
            .name="deadLetterExchange",
            .type=0,
            .durability=1,
            .autoDelete=0,
            .internal=0,
            .args={0}
        }
    }
};//交换机

RabbitmqQueues_t queuesInfo={
    .size=2,
    .queues={
        //todo 设备数据消息队列
        {
            .name="plc_data",
            .type=0,
            .durability=1,
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
            .args={
                    {.type=0,.key=ARGUMENT_QUEUE_04,.value.str=""},
                    {.type=0,.key=ARGUMENT_QUEUE_05,.value.str=""}
            }
        },
        //todo 设备故障消息队列
        {
            .name="Fault_Reports",
            .type=0,
            .durability=1,
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
            .args={
                    {.type=0,.key=ARGUMENT_QUEUE_04,.value.str=""},
                    {.type=0,.key=ARGUMENT_QUEUE_05,.value.str=""}
            }
        }
    }
};//队列

RabbitmqBinds_t bindsInfo= {
    .size=2,
    .binds={
        //连接状态 通道 路由键 队列 交换机
        {
            .routingKey=ROUTING_KEY_PLC_DATA,
            .exchange_index=0,
            .queue_index=0
        },
        {
            .routingKey=ROUTING_KEY_FAULT_REPORTS,
            .exchange_index=0,
            .queue_index=1
        },
    }
};
consumers_t consumersInfo={
    .size=1,
    .consumers={
        //todo 消费者
        {
            .conn_index=1,
            .channel_index=2,
            .consumer_tag="consumer00",
//            .no_local=0,
//            .no_ack=0,
//            .exclusive=0,
//            .thread_handle={}
            .task=consumer_task_00
        }
    }
};
producers_t producersInfo={
    .size=2,
    .producers={
        //todo 生产者 采集设备数据
        {
            .conn_index=0,
            .channel_index=2,
            .confirmMode=0,
            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            },
            .task=producer_task_upload_device_data
        },
        //todo 生产者 设备故障数据
        {
            .conn_index=0,
            .channel_index=2,
            .confirmMode=0,
            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            },
            .task=producer_task_upload_fault_data
        }
    },

};
void vlog(FILE *fd,char *str,va_list args){
    pthread_mutex_lock(&log_mutex);
    vfprintf(fd,str,args);
    pthread_mutex_unlock(&log_mutex);
}
void info(char *str,...){
    va_list list;
    va_start(list,str);
    vlog(stdout,str,list);
    fprintf(stdout,"\n");
    fflush(stdout);
    va_end(list);
}
void warn(char *str,...){
    va_list list;
    va_start(list,str);
    vlog(stderr,str,list);
    fprintf(stderr,"\n");
    fflush(stderr);
    va_end(list);
}


void main(){
    //todo 初始化锁、条件变量、barrier
    pthread_mutex_init(&mutex,NULL);
    pthread_cond_init(&cond_exit,NULL);
//    pthread_barrier_init(&barrier,NULL,producersInfo.size+consumersInfo.size);

    //todo 启动任务
    rabbitmq_start_consumers();
    rabbitmq_start_producers();

    //todo 等待同步

    //todo 如果某个线程出现异常并结束
    {
//        sleep(1);
        pthread_mutex_lock(&mutex);
        pthread_cond_broadcast(&cond_start);
        warn("main:broadcast started");

        pthread_cond_wait(&cond_exit,&mutex);
        warn("main:notified");
        warn("exitInfo:type=%d,index=%d,info=%s",exitInfo.type,exitInfo.index,exitInfo.info);
        pthread_mutex_unlock(&mutex);
    }


}







//todo check函数
int rabbitmq_check_conn_index(int conn_index){
    if(
        (conn_index >= CONNECTION_MAX_SIZE)
        || (conn_index < 0 || conn_index >= rabbitmqConnsInfo.size)
    ){
        warn("conns: index=%d,size=%d,max=%d",conn_index,rabbitmqConnsInfo.size,CONNECTION_MAX_SIZE);
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
        warn("channel: conn_index=%d,channel_index=%d,size=%d,max=%d",
             conn_index,channel_index,
             rabbitmqConnsInfo.conns[conn_index].channelsInfo.size,
             CHANNEL_MAX_SIZE
        );
        return 0;
    }
    else{
        return 1;
    }

}


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
        return 1;
    }
}
//重置单个连接下的所有通道
int rabbitmq_reset_channels(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    int size = rabbitmqConnsInfo.conns[conn_index].channelsInfo.size;
    if(size > CONNECTION_MAX_SIZE){
        warn("conns: size=%d,max=%d",size,CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < rabbitmqConnsInfo.conns[conn_index].channelsInfo.size; ++i) {
            flag&=rabbitmq_reset_channel(conn_index,i);
            if(flag==0){
                warn("conn[%d] reset: channel[%d] fail",conn_index,i);
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
        //todo 设置连接相关指针 NULL
        rabbitmqConnsInfo.conns[conn_index].connState =NULL;
        rabbitmqConnsInfo.conns[conn_index].connState =NULL;
        //todo 设置连接套接字 NULL
        rabbitmqConnsInfo.conns[conn_index].socket=NULL;

        //todo 通道状态 0-未启用
        rabbitmq_reset_channels(conn_index);

    }
}

int rabbitmq_reset_conns(){
    int size = rabbitmqConnsInfo.size;
    if(size > CONNECTION_MAX_SIZE){
        warn("conns: size=%d,max=%d",size,CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
            flag&=rabbitmq_reset_conn(i);
            if(flag==0){
                warn("conn[%d] reset: fail",i);
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

    int res=0;
    if(rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status==1){
        amqp_rpc_reply_t reply = amqp_channel_close(
            rabbitmqConnsInfo.conns[conn_index].connState,
            rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num,
            AMQP_REPLY_SUCCESS
        );
        if(die_on_amqp_error(reply,"closing channel")==0){
            //todo reset
            res=rabbitmq_reset_channel(conn_index,channel_index);
        }
    }
    return res;
}
int rabbitmq_close_channels(int conn_index){
    if(rabbitmq_check_conn_index(conn_index)==0){
        return 0;
    }
    int size = rabbitmqConnsInfo.conns[conn_index].channelsInfo.size;
    if(size > CONNECTION_MAX_SIZE){
        warn("conns: size=%d,max=%d",size,CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < size; ++i) {
            flag&=rabbitmq_close_channel(conn_index,i);
            if(flag==0){
                warn("conn[%d]:channel[%d] close fail",conn_index,i);
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
        //todo 关闭通道
        if(rabbitmq_close_channels(conn_index)==0){
            return 0;
        }

        //todo 关闭连接
        amqp_rpc_reply_t reply= amqp_connection_close(
            rabbitmqConnsInfo.conns[conn_index].connState,
            AMQP_REPLY_SUCCESS
        );
        if(die_on_amqp_error(reply,"closing connection")==1){
            return 0;
        }
        
        //todo 销毁连接对象
        amqp_destroy_connection(
            rabbitmqConnsInfo.conns[conn_index].connState
        );
        
        //todo reset
        rabbitmq_reset_conn(conn_index);
        return 1;
    }
}
int rabbitmq_close_conns(){
    int size = rabbitmqConnsInfo.size;
    if(size <= 0 ||size > CONNECTION_MAX_SIZE){
        warn("conns: size=%d,max=%d",size,CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < size; ++i) {
            flag&=rabbitmq_close_conn(i);
            if(flag==0){
                warn("conn[%d]: close fail",i);
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
                        break;
                    }
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
        warn("channel: size=%d,max=%d",size,CHANNEL_MAX_SIZE);
        return 0;
    }

    else{
        int flag=1;
        for (int i = 0; i < rabbitmqConnsInfo.conns[conn_index].channelsInfo.size; ++i) {
            flag&=rabbitmq_init_channel(conn_index,i);
            if(flag==0){
                warn("channel[%d] reset: fail",i);
                break;
            }
        }
        return flag;
    }
}
int rabbitmq_login_conn(amqp_connection_state_t conn){
    int res=0;
    amqp_login(
        conn,
        "/",
        0,//channel_max
        131072,//frame_max
        0,//heartbeat
        AMQP_SASL_METHOD_PLAIN,//内部用户密码登录 | 外部系统登录
        "root",
        "123123"
    );
    if(die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel")==0){
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
        if(pConn == NULL){
            warn("conn[%d] init: create fail",conn_index);
            return 0;
        }
        //todo 创建tcp套接字
        amqp_socket_t *socket = amqp_tcp_socket_new(pConn);
        if(socket == NULL){
            warn("conn[%d] init:socket create fail",conn_index);
            return 0;
        }
        //todo 打开TCP连接
        int res = amqp_socket_open(
                socket,
                rabbitmqConfigInfo.hostname,
                rabbitmqConfigInfo.port
        );
        if(res != AMQP_STATUS_OK){
            warn("conn[%d] init:socket open fail",conn_index);
            return 0;
        }

        rabbitmqConnsInfo.conns[conn_index].status=1;
        //todo 连接的登录
        if(rabbitmq_login_conn(pConn)==0){
            return 0;
        }

        rabbitmqConnsInfo.conns[conn_index].status=2;
        //todo 初始化该连接下的通道
        if(rabbitmq_init_channels(conn_index)==0){
            return 0;
        }

        rabbitmqConnsInfo.conns[conn_index].status=3;
        //todo 设置数据
        rabbitmqConnsInfo.conns[conn_index].connState = pConn;
        rabbitmqConnsInfo.conns[conn_index].socket = socket;
        return 1;
    }
}
int rabbitmq_init_conns(){
    int size = rabbitmqConnsInfo.size;
    if(size <= 0 ||size > CONNECTION_MAX_SIZE){
        warn("conns: size=%d,max=%d",size,CONNECTION_MAX_SIZE);
        return 0;
    }
    else{
        //todo 重置连接数据
        if(rabbitmq_reset_conns()==0){
            return 0;
        }

        //todo 初始化连接数据
        int flag=1;
        int conn_init_status;
        for (int i = 0; i < size; ++i) {
            flag &= rabbitmq_init_conn(i);
            if(flag==0){
                //todo 释放已申请资源
                switch (rabbitmqConnsInfo.conns[i].status) {
//                    //0-未打开
//                    case 0:
//                        break;
                    //1-已连接
                    case 1:
                        //todo 释放连接资源
                        rabbitmq_close_conns();
////                        die_on_amqp_error(amqp_channel_close(rabbitmqConnsInfo.conns[i].connState, 1, AMQP_REPLY_SUCCESS),"Closing channel");
//                        die_on_amqp_error(amqp_connection_close(rabbitmqConnsInfo.conns[i].connState, AMQP_REPLY_SUCCESS),"Closing connection");
//                        die_on_error(amqp_destroy_connection(rabbitmqConnsInfo.conns[i].connState), "Ending connection");
                        break;
//                    //2-已登录
//                    case 2:
//                        break;
                    default:
                        break;
                }
                warn("conn[%d] reset: fail",i);
                break;
            }
        }
        return flag;
    }

};


//todo start函数
int rabbitmq_start_producer(int index){
    //todo 初始化生产者结构体
    producersInfo.producers[index].index=index;

    //todo 启动线程
    return pthread_create(
        &producersInfo.producers[index].thread_handle,
        NULL,
        producersInfo.producers[index].task,
//        &index//todo 指针指向栈空间地址，主线程退栈后可能造成子线程接收的参数指向一个被回收的内存
        &producersInfo.producers[index].index
    )==0?1:0;
}
int rabbitmq_start_producers(){
    if(producersInfo.size<=0 || producersInfo.size>PRODUCER_MAX_SIZE){
        warn("producers start: size=%d,max=%d",producersInfo.size,PRODUCER_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < producersInfo.size; ++i) {
            flag&=rabbitmq_start_producer(i);
            if(flag==0){
                warn("producer[%d] start: fail",i);
                break;
            }
        }
        return flag;
    }

}

int rabbitmq_start_consumer(int index){
    //todo 初始化消费者结构体
    consumersInfo.consumers[index].index=index;

    //todo 启动线程
    return pthread_create(
        &consumersInfo.consumers[index].thread_handle,
        NULL,
        consumersInfo.consumers[index].task,
        &consumersInfo.consumers[index].index
    )==0?1:0;
}
int rabbitmq_start_consumers(){
    if(consumersInfo.size<=0 || consumersInfo.size>CONSUMER_MAX_SIZE){
        warn("consumers start: size=%d,max=%d",consumersInfo.size,CONSUMER_MAX_SIZE);
        return 0;
    }
    else{
        int flag=1;
        for (int i = 0; i < consumersInfo.size; ++i) {
            flag&=rabbitmq_start_consumer(i);
            if(flag==0){
                warn("consumer[%d] start: fail");
                break;
            }
        }
        return flag;
    }
}

//todo task函数
//下拉
//定时数据
void *consumer_task_00(void *arg){
    int consumer_index=*(int *)arg;

//    sleep(1);

    {
        pthread_mutex_lock(&mutex);

        info("thread consumer[%d]:wait start",consumer_index);
        pthread_cond_wait(&cond_start,&mutex);
        info("thread consumer[%d]:run",consumer_index);

        //todo 修改数据
        exitInfo.type=0;
        exitInfo.info="thread consumer exit:定时数据";
        exitInfo.index=consumer_index;

        info("thread consumer[%d]:signal",consumer_index);
        pthread_cond_signal(&cond_exit);

        pthread_mutex_unlock(&mutex);
    }


}
//上传
//采集设备数据
void *producer_task_upload_device_data(void *arg){

    int producer_index=*(int *)arg;

//    info("thread producer[%d]:sleep...",producer_index);
//    sleep(1000);
//    info("thread producer[%d]:wake up",producer_index);

    {
        pthread_mutex_lock(&mutex);

        info("thread producer[%d]: wait start",producer_index);
        pthread_cond_wait(&cond_start,&mutex);
        info("thread producer[%d]: run",producer_index);

        exitInfo.type=1;
        exitInfo.info="thread producer exit:采集设备数据";
        exitInfo.index=producer_index;


        info("thread producer[%d]:signal",producer_index);
        pthread_cond_signal(&cond_exit);

        pthread_mutex_unlock(&mutex);
    }
}
//设备故障数据
void *producer_task_upload_fault_data(void *arg){
    int producer_index=*(int *)arg;


//    info("thread producer[%d]:sleep...",producer_index);
//    sleep(1000);
//    info("thread producer[%d]:wake up",producer_index);

    {
        pthread_mutex_lock(&mutex);

        info("thread producer[%d]: wait start",producer_index);
        pthread_cond_wait(&cond_start,&mutex);
        info("thread producer[%d]: run",producer_index);

        exitInfo.type=1;
        exitInfo.info="thread producer exit:设备故障数据";
        exitInfo.index=producer_index;

        info("thread producer[%d]:signal",producer_index);
        pthread_cond_signal(&cond_exit);

        pthread_mutex_unlock(&mutex);
    }
}








//todo 客户端启动函数
int rabbitmq_init_client(){
    //todo 初始化连接和通道
    if(rabbitmq_init_conns()==0){
        return 0;
    }
    //todo 交换机检查

    //todo 队列检查

    return 1;
}

int rabbitmq_start_client(){
    if(rabbitmq_init_client()==0){
        warn("rabbitmq client:init fail",stderr);
    }else{
        int flag=1;
        //todo 初始化条件变量
        pthread_mutex_init(&mutex,NULL);
        pthread_cond_init(&cond_exit,NULL);

        //todo 启动生产者
        flag&=rabbitmq_start_consumers();

        //todo 启动消费者
        flag&=rabbitmq_start_producers();

        if(flag==1){
            //todo 如果某个线程出现异常并结束
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&cond_exit,&mutex);

            //todo 关闭所有线程
            warn("close all");

            pthread_mutex_unlock(&mutex);
        }
        //todo 释放锁和条件变量资源
        pthread_cond_destroy(&cond_exit);
        pthread_mutex_destroy(&mutex);
    }
}


//todo 生产者
void *producer(){

    return NULL;
}

//todo 消费者
void *consumer(){
    return NULL;
}