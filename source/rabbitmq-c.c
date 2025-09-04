#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <bits/types/struct_timeval.h>
#include <fcntl.h>

#include "rabbitmq-c.h"

pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_consumer_tasks=PTHREAD_MUTEX_INITIALIZER;//消费者任务线程互斥锁
pthread_mutex_t mutex_producer_tasks=PTHREAD_MUTEX_INITIALIZER;//生产者任务线程互斥锁

pthread_mutex_t mutex;
volatile int thread_counts=0;
volatile int work_status=0;//0-ready就绪 1-running运行 2-stop停止 3-exit 4-terminated
volatile int flag_running=0;
pthread_cond_t cond_running;
volatile int flag_stop=0;
pthread_cond_t cond_deal;
volatile int flag_exit=0;
pthread_cond_t cond_exit;

//todo 全局重置标识
volatile int flag_reset_conn;//
volatile int flag_reset_channel;//-1

RabbitmqConfig_t rabbitmqConfigInfo={
    .hostname="192.168.200.132",
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
    .size=3,
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
        }
    }
};//队列

RabbitmqBinds_t bindsInfo= {
    .size=3,
    .binds={
        //连接状态 通道 路由键 队列 交换机
        {
            .routingKey=ROUTING_KEY_PLC_DATA,
            .exchange_index=0,
            .queue_index=0//设备数据消息队列
        },
        {
            .routingKey=ROUTING_KEY_FAULT_REPORTS,
            .exchange_index=0,
            .queue_index=1//设备故障消息队列
        },
        {
            .routingKey=ROUTING_KEY_DEAD_LETTER,
            .exchange_index=1,//死信交换机
            .queue_index=2//死信队列
        },
    }
};
consumers_t consumersInfo={
    .size=1,
    .consumers={
        //todo 消费者
        {
            .conn_index=1,
//            .channel_index=1,//第1个通道不使用,自动分配可用通道

            //参数设置
            .queue_index=0,
            .consumer_tag="consumer00",
//            .no_local=0,
//            .no_ack=1,
//            .exclusive=0,
//            .requeue=0,

            .taskInfo={
                .task=consumer_task_00,
            }
        }
    }
};
producers_t producersInfo={
    .size=2,
    .producers={
        //todo 生产者 采集设备数据
        {
            .conn_index=0,
//            .channel_index=1,//第1个通道不使用,自动分配可用通道

            //参数设置
            .exchange_index=0,
            .confirmMode=0,
            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            },
            .taskInfo={
                .task=producer_task_upload_device_data,
            }
        },
        //todo 生产者 设备故障数据
        {
            .conn_index=0,
//            .channel_index=2,//自动分配
            
            //参数设置
            .exchange_index=0,
            .confirmMode=0,
            
            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            },
            .taskInfo={
                .task=producer_task_upload_fault_data,
            }
        }
    },

};
void log_threads_exitInfo(){
    info("========================(exitInfo)==========================");
    //todo 生产者
    for (int i = 0; i < producersInfo.size; ++i) {
        info("producer[%d] exitInfo:",i);
        info("code: %d",producersInfo.producers[i].taskInfo.execInfo.code);
        info("info: %s",producersInfo.producers[i].taskInfo.execInfo.info);
        info("");
    }

    //todo 消费者
    for (int i = 0; i < consumersInfo.size; ++i) {
        info("consumer[%d] exitInfo:",i);
        info("code: %d",consumersInfo.consumers[i].taskInfo.execInfo.code);
        info("info: %s",consumersInfo.consumers[i].taskInfo.execInfo.info);
        info("");
    }
}

void vlog(FILE *fd,char *str,va_list args){
    vfprintf(fd,str,args);
}
void log(FILE *fd ,char *str,...){
    va_list list;

    pthread_mutex_lock(&log_mutex);

    va_start(list,str);
    vlog(stdout,str,list);
    fflush(stdout);
    va_end(list);

    pthread_mutex_unlock(&log_mutex);
}
void info(char *str,...){
    va_list list;

    pthread_mutex_lock(&log_mutex);

    va_start(list,str);
    vlog(stdout,str,list);
    fprintf(stdout,"\n");
    fflush(stdout);
    va_end(list);

    pthread_mutex_unlock(&log_mutex);
}
void warn(char *str,...){
    va_list list;

    pthread_mutex_lock(&log_mutex);

    va_start(list,str);
    vlog(stderr,str,list);
    fprintf(stderr,"\n");
    fflush(stderr);
    va_end(list);

    pthread_mutex_unlock(&log_mutex);
}

void print_lock_info(const pthread_mutex_t *mutex){
    log(stdout,"mutex: {data = {lock = %d, count = %d, owner = %d, nusers = %d, kind = %d, spins = %d, elision = %d}}\n",
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
    log(stdout,"cond: {data = {"
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
    warn("---------------------");
    print_lock_info(mutex);
    print_cond_info(cond);
    warn("---------------------");
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
int rabbitmq_check_queue_index(int queue_index){
    if(
        (queue_index >= QUEUE_MAX_SIZE)
        || (queue_index<0 || queue_index>=queuesInfo.size)
    ){
        warn("queue: index=%d,size=%d,max=%d",queue_index,queuesInfo.size,QUEUE_MAX_SIZE);
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
        warn("exchange: index=%d,size=%d,max=%d",exchange_index,exchangesInfo.size,EXCHANGE_MAX_SIZE);
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
        warn("bind check: fail,bind_index=%d",bind_index);
        return 0;
    }

    return 1;
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
        warn("conns: size=%d,max=%d",size,CHANNEL_MAX_SIZE);
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
    if(size > CHANNEL_MAX_SIZE){
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
        //todo 根据连接的状态释放资源
        int conn_status = rabbitmqConnsInfo.conns[conn_index].status;
        if(conn_status == 5){
            //todo 关闭通道
            if(rabbitmq_close_channels(conn_index)==0){
                return 0;
            }
        }
        if(conn_status >= 2){
            //todo 关闭连接
            amqp_rpc_reply_t reply= amqp_connection_close(
                    rabbitmqConnsInfo.conns[conn_index].connState,
                    AMQP_REPLY_SUCCESS
            );
            if(die_on_amqp_error(reply,"closing connection")==1){
                return 0;
            }
        }
        if(conn_status >= 1){
            //todo 销毁连接对象
            amqp_destroy_connection(
                    rabbitmqConnsInfo.conns[conn_index].connState
            );
        }
        //todo reset连接资源
        return rabbitmq_reset_conn(conn_index);
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
                warn("channel[%d] int: fail",i);
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
            warn("conn[%d] init: create fail",conn_index);
            rabbitmq_close_conn(conn_index);
            return 0;
        }
        rabbitmqConnsInfo.conns[conn_index].connState = pConn;
        
        //todo 创建tcp套接字
        amqp_socket_t *socket = amqp_tcp_socket_new(pConn);
        rabbitmqConnsInfo.conns[conn_index].status=2;
        if(socket == NULL){
            warn("conn[%d] init: socket create fail",conn_index);
            rabbitmq_close_conn(conn_index);
            return 0;
        }
        
        
        //todo 打开TCP连接
        int res = amqp_socket_open(
                socket,
                rabbitmqConfigInfo.hostname,
                rabbitmqConfigInfo.port
        );
        if(res != AMQP_STATUS_OK){
            warn("conn[%d] init: socket open fail,host=%s,port=%d",
                 conn_index,
                 rabbitmqConfigInfo.hostname,
                 rabbitmqConfigInfo.port
             );
            rabbitmq_close_conn(conn_index);
            return 0;
        }
        rabbitmqConnsInfo.conns[conn_index].status=3;
        rabbitmqConnsInfo.conns[conn_index].socket = socket;

        //todo 连接的登录
        if(rabbitmq_login_conn(pConn)==0){
            rabbitmq_close_conn(conn_index);
            return 0;
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
            flag &= rabbitmq_init_conn(i);//todo 需要保证初始化失败时释放已申请的连接资源，并完成重置
            if(flag==0){
                warn("conn[%d] init: fail",i);
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
            warn("bind check: fail ,exchange_index=%d",bind.exchange_index);
            return 0;
        }
        if(rabbitmq_check_queue_index(bind.queue_index) == 0){
            warn("bind check: fail ,queue_index=%d",bind.queue_index);
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

int rabbitmq_init_consumer(int consumer_index){

    //todo 检查索引下标(消费者、连接、通道、队列)
    if(consumer_index<0 || consumer_index>=CONSUMER_MAX_SIZE){
        warn("consumer: index=%d,max=%d",consumer_index,CONSUMER_MAX_SIZE);
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
            warn("consumer[%d]: no available channel",consumer_index);
            return 0;
        }
        warn("consumer[%d]: can't use the first channel,change channel_index=%d",consumer_index,channel_index);
    }
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        warn("consumer[%d]: channel[%d] is not exists",consumer_index,channel_index);
        return 0;
    }
    //队列检查
    if(rabbitmq_check_queue_index(queue_index) == 0){
        warn("consumer[%d]: queue[%d] index check fail",consumer_index,queue_index);
        return 0;
    }

    //检查任务
    if(consumersInfo.consumers[consumer_index].taskInfo.task==NULL){
        warn("consumer[%d]: task is null",consumer_index);
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
        warn("producer: index=%d,max=%d",producer_index,PRODUCER_MAX_SIZE);
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
            warn("producer[%d]: no available channel",producer_index);
            return 0;
        }
        warn("producer[%d]: can't use the first channel,change channel_index=%d",producer_index,channel_index);
    }
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        warn("producer[%d]: channel[%d] is not exists",producer_index,channel_index);
        return 0;
    }
    //交换机
    if(rabbitmq_check_exchange_index(exchange_index) == 0){
        warn("producer[%d]: exchange[%d] index check fail",producer_index,exchange_index);
        return 0;
    }
    //检查任务
    if(producersInfo.producers[producer_index].taskInfo.task==NULL){
        warn("producer[%d]: task is null",producer_index);
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
    if(taskInfo->type==0){
        if(rabbitmq_init_producer(taskInfo->index)==0){
            return 0;
        }
    }
    else{
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
        
        if(init_role_by_taskInfo(taskInfo) == 0){
            return 0;
        }
    }
    return 1;
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
        case -1:
            return "exit with error";
        case 0:
            return "exit normally";
        case 1:
            return "stop to exit";
    }
}

int get_task_num_of_conn(int conn_index){
    channelInfo_t channelsInfo = rabbitmqConnsInfo.conns[conn_index].channelsInfo;
    int res=0;
    for (int i = 0; i < channelsInfo.size; ++i) {
        if(channelsInfo.channels[i].status==2){
            res++;
        }
    }
    return res;
}


//todo start函数
int rabbitmq_start_producer(int index){
    //todo 初始化生产者结构体
    if(rabbitmq_init_producer(index)==0){
        warn("producer[%d]: init fail",index);
        return 0;
    }
//    producersInfo.producers[index].taskInfo.index=index;
//
//    //todo 向通道 注册 任务信息的引用
//    producerEntity_t_t producer = producersInfo.producers[index];
//    rabbitmqConnsInfo.conns[producer.conn_index].channelsInfo.channels[producer.channel_index].taskInfo=&(consumersInfo.consumers[index].taskInfo);
//
//    //todo 统计 连接 的任务数量
//    rabbitmqConnsInfo.conns[producer.conn_index].task_nums++;

    //todo 启动线程
    return pthread_create(
        &producersInfo.producers[index].taskInfo.thread_handle,
        NULL,
        rabbitmq_task,
        &producersInfo.producers[index].taskInfo
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
    if(rabbitmq_init_consumer(index)==0){
        warn("consumer[%d]: init fail",index);
        return 0;
    }
    //todo 向通道 注册 任务信息的引用
    consumerEntity_t consumer = consumersInfo.consumers[index];
    rabbitmqConnsInfo.conns[consumer.conn_index].channelsInfo.channels[consumer.channel_index].taskInfo=&(consumersInfo.consumers[index].taskInfo);

    //todo 统计 连接 的任务数量
    rabbitmqConnsInfo.conns[consumer.conn_index].task_nums++;

    //todo 启动线程
    return pthread_create(
        &consumersInfo.consumers[index].taskInfo.thread_handle,
        NULL,
        rabbitmq_task,
        &consumersInfo.consumers[index].taskInfo
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
int consumer_task_00(void *arg){
    //todo 自定义返回信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
//    taskInfo->exitInfo.info="consumer[%d]: exited!";
    if(taskInfo->type!=1){
        return -1;
    }
    
    consumerEntity_t consumerInfo = consumersInfo.consumers[taskInfo->index];
    connectionEntity connInfo = rabbitmqConnsInfo.conns[consumerInfo.conn_index];
    //参数
    amqp_connection_state_t connState = connInfo.connState;
    int channel_index = connInfo.channelsInfo.channels[consumerInfo.channel_index].num;

    {
        amqp_rpc_reply_t res;//
        amqp_envelope_t envelope;//用于存储消息内容
        amqp_frame_t frame;
        int success=0;

        //用于 内存优化 的关键函数
        //  注：在还未处理完消息内容(如访问envelope.message.body)前调用会导致数据丢失
        amqp_maybe_release_buffers(connState);//，控制着库内部缓冲区的内存释放行为

        //todo 非阻塞获取队列最新消息
        struct timeval timeout;
        timeout.tv_sec=0;
        timeout.tv_usec=0;
        res = amqp_consume_message(connState, &envelope, NULL, 0);

        //todo 处理消息
        if (AMQP_RESPONSE_NORMAL == res.reply_type) {
            //todo 消费消息
            success = consumer_message_handle(&envelope);

            //todo 手动ack处理
            if(consumerInfo.no_ack==0){
                //todo 确认消息
                if(success){
                    amqp_basic_ack(
                            connState,
                            channel_index,//channel
                            envelope.delivery_tag,//需要被确认消息的标识符
                            0//批量确认
                    );

                    info("consumer: manual ack");
                }
                //todo 拒绝消息
                else{
//                    amqp_basic_reject(
//                            connState,
//                            channel_index,//channel
//                            envelope.delivery_tag,//需要被拒绝消息的标识符
//                            0 //requeue
//                    );
////                amqp_basic_nack(
////                    connState,
////                    1,//channel
////                    envelope.delivery_tag,//需要被拒绝消息的标识符
////                    1,//multiple 批量拒绝比当前标识小的未确认的消息
////                    1//requeue
////                );
//                    info("consumer: reject requeue");
                }
            }

            return 0;
        }
        //todo 异常和非预期帧处理
        else{
            //todo 释放空间
            amqp_destroy_envelope(&envelope);//底层会调用 amqp_destroy_message()
            if (
                AMQP_RESPONSE_LIBRARY_EXCEPTION == res.reply_type       //客户端Rabbitmq-c库内部错误
                && AMQP_STATUS_UNEXPECTED_STATE == res.library_error    //协议状态机异常
            ) {
                //根据amqp_consume_message注释信息：此时的异常处理表示收到了 AMQP_BASIC_DELIVER_METHOD 以外的帧，
                // 则调用方应调用 amqp_simple_wait_frame（） 来读取此帧并采取适当的操作。
                // 主要处理跟连接关闭、通道关闭帧
                if (AMQP_STATUS_OK != amqp_simple_wait_frame(connState, &frame)) {
                    return -1;
                }
                //1.METHOD帧 方法帧
                if (AMQP_FRAME_METHOD == frame.frame_type) {
//                    frame.payload.method.id;//方法帧（类和方法信息）
//                    frame.payload.method.decoded;//方法帧（参数信息）
                    switch (frame.payload.method.id) {
                        //todo 需要处理的帧(连接、通道的关闭)
                        case AMQP_CHANNEL_CLOSE_METHOD:
                        {
                            /* a channel.close method happens when a channel exception occurs,
                             * this can happen by publishing to an exchange that doesn't exist
                             * for example.
                             *
                             * In this case you would need to open another channel redeclare
                             * any queues that were declared auto-delete, and restart any
                             * consumers that were attached to the previous channel.
                             */
                            warn("consumer: AMQP_CHANNEL_CLOSE_METHOD");
                            //todo 解析关闭原因
                            amqp_channel_close_t *r = (amqp_channel_close_t *) frame.payload.method.decoded;
                            warn(r->reply_text.bytes);

                            //todo 修改重置标志
//                            rabbitmqConnsInfo.conns[consumerInfo.conn_index].channelsInfo.channels[consumerInfo.channel_index].status=0;

                            return 3;
                        }
                        //connect已关闭
                        case AMQP_CONNECTION_CLOSE_METHOD:
                        {
                            /* a connection.close method happens when a connection exception
                             * occurs, this can happen by trying to use a channel that isn't
                             * open for example.
                             *
                             * In this case the whole connection must be restarted.
                             */
                            //对同一个deliveryTag进行多次ack也会触发
                            warn("consumer: AMQP_CONNECTION_CLOSE_METHOD");
                            //todo 需要重新打开连接
                            amqp_connection_close_t *r = (amqp_connection_close_t *) frame.payload.method.decoded;
                            warn(r->reply_text.bytes);

                            //todo 修改重置标志
                            rabbitmqConnsInfo.conns[consumerInfo.conn_index].reset_flag=1;
                            return 2;
                        }
//                        //服务端投递帧(表示接下来服务端有消息发来)
//                        case AMQP_BASIC_DELIVER_METHOD:
//                            warn("consumer: AMQP_BASIC_DELIVER_METHOD");
//                            break;
                        //其它
                        default:
                        {
                            //todo 非预期帧(以下帧无需消费端特别处理)
//                            AMQP_BASIC_ACK_METHOD
//                            AMQP_BASIC_RETURN_METHOD
//                            AMQP_BASIC_DELIVER_METHOD
                            warn("consumer: received other frame");
                            break;
                        }
                    }
                }
            }

            return 0;
        }
        //释放空间
//        amqp_destroy_message(&envelope.message);
    }
}
//上传
//采集设备数据
int producer_task_upload_device_data(void *arg){
    //todo 自定义返回信息
    taskInfo_t *taskInfo = (taskInfo_t *) arg;
//    taskInfo->exitInfo.info="consumer[%d]: exited!";
    if(taskInfo->type!=0){
        return -1;
    }

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

    {

    }
    
    {
        char buffer[100]={0};
//        amqp_frame_t frame={};

        //todo 准备消息内容
        int code = producer_prepare_message(buffer, 100);//-1-异常 0-无数据发送 1-有数据待发送
//        data = amqp_bytes_malloc(strlen(buffer)+1);
        int res=0;
        if(code==1){
            //消息发布
            int res=amqp_basic_publish(
                    connState,
                    channel,//channel
                    exchange,   //exchange
                    routingKey, //routingKey
                    mandatory,//mandatory
                    immediate,//immediate
                    props,//properties 消息头帧属性
                    amqp_cstring_bytes(buffer)//body
            );

//            amqp_status_enum;//枚举值
            if(res==AMQP_STATUS_OK){
                info("producer send:");
                info(buffer);
                //todo 检查发布确认是否开启
                if(confirmMode == 1) {
                    //todo 等待ack
                    wait_ack(taskInfo);
                }
//                return 0;
                res=0;
            }
            else{
                warn("producer[%d]: publish fail",taskInfo->index);
                res=-1;
//                return -1;
            }

        }
        else if(code==0){
            return  0;
        }
        else{
            warn("producer[%d]: message prepare fail",taskInfo->index);
            return -1;
        }

        return res;
    }
}
//设备故障数据
int producer_task_upload_fault_data(void *arg){

    return 0;
}

void task_notify_main_run(){
    //todo 通知 线程已就绪
    pthread_mutex_lock(&mutex);
    thread_counts++;
    if(thread_counts==producersInfo.size+consumersInfo.size){
        flag_running=1;
        warn("signal=cond_running");
        pthread_cond_signal(&cond_running);
    }
    pthread_mutex_unlock(&mutex);
}
void task_notify_main_deal(){
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond_deal);
    pthread_mutex_unlock(&mutex);
}
void task_notify_main_stop(taskInfo_t *taskInfo){
    //todo 处理出错或其它会导致客户端状态变为 2-stop
    {
        pthread_mutex_lock(&mutex);

        flag_stop=1;
        pthread_cond_broadcast(&cond_deal);

        pthread_mutex_unlock(&mutex);
    }
}
void task_notify_main_reset_conn(int conn_index){
    pthread_mutex_lock(&mutex);

    //todo 通知main处理 同个连接下有多个线程，需要等待所有线程通知main,main才能重置连接，否则造成线程访问空连接问题
    rabbitmqConnsInfo.conns[conn_index].task_nums--;
    if(rabbitmqConnsInfo.conns[conn_index].task_nums==0){
        flag_reset_conn=1;
        pthread_cond_broadcast(&cond_deal);
    }

    //todo 等待处理完成
    task_wait_main_reset_conn(conn_index);

    pthread_mutex_unlock(&mutex);
}
void task_notify_main_reset_channel(int conn_index, int channel_index){
    pthread_mutex_lock(&mutex);

    //todo 通知main处理
    flag_reset_conn=1;
    pthread_cond_broadcast(&cond_deal);

    //todo 等待处理完成
    task_wait_main_reset_channel(conn_index,channel_index);

    pthread_mutex_unlock(&mutex);

}
void task_notify_main_exit(){
    //todo 通知任务已退出
    pthread_mutex_lock(&mutex);
    thread_counts--;
    if(thread_counts==0){
        flag_exit=1;
        warn("signal=cond_exit");
        pthread_cond_broadcast(&cond_exit);//通知主线程
    }
    warn("thread_counts=%d",thread_counts);
    pthread_mutex_unlock(&mutex);
}
void task_wait_main_reset_conn(int conn_index){
    pthread_mutex_lock(&mutex);
    //todo 等待 main处理
    while(
        rabbitmqConnsInfo.conns[conn_index].reset_flag != 1
        || rabbitmqConnsInfo.conns[conn_index].status != 5
    ){
        pthread_cond_wait(&rabbitmqConnsInfo.cond_reset_conn,&mutex);
    }
    pthread_mutex_unlock(&mutex);
}

void task_wait_main_reset_channel(int conn_index, int channel_index){
    pthread_mutex_lock(&mutex);
    //todo 等待 main处理
    while(
        rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].flag_reset!=0
        || rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].status!=2

    ){
        pthread_cond_wait(&rabbitmqConnsInfo.conns[conn_index].channelsInfo.cond_reset_channel,&mutex);
    }
    pthread_mutex_unlock(&mutex);
}



//todo parse解析消息
int message_parse(char *buffer,int size){

}

//todo pack打包消息
int message_pack(char *buffer,int size){

}

//todo handle函数
int producer_prepare_message(char *buffer,int size){
    //todo zc-业务处理逻辑
    memset(buffer,0,size);
//    log(stdout,"publish>");

//    int flags = fcntl(0, F_GETFL, 0);
//    fcntl(0,F_SETFL,flags | O_NONBLOCK);

    fgets(buffer,size,stdin);//会将‘\n’读入

    //处理末尾换行符
    *strstr(buffer,"\n")='\0';
    if(strcmp(buffer,"exit")==0)
    {
        return 0;
    }
    else if(strcmp(buffer,"error")==0){

        return -1;
    }
    return 1;
}

int main_handle_reset_channels(){


    for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
        channelInfo_t *channelsInfo = &rabbitmqConnsInfo.conns[i].channelsInfo;
        for (int j = 0; j < channelsInfo->size; ++j) {
            channelEntity_t *channel = &(channelsInfo->channels[j]);
            if(channel->flag_reset == 1){
                //todo 关闭通道
                if(rabbitmq_close_channel(i,j)==0){
                    return 0;
                }

                //todo 初始化通道
                if(rabbitmq_init_channel(i,j)==0){
                    pthread_cond_broadcast(&(channelsInfo->cond_reset_channel));
                    warn("main: channel reopen fail");
                    return 0;
                }

                //todo 初始化角色(生产者和消费者)
                if(init_role_by_taskInfo(channel->taskInfo)==0){
                    return 0;
                }

//                pthread_mutex_lock(&mutex);
                //todo 清除重置标志
                channel->flag_reset=0;
                //todo 通知等待该通道重置道德任务线程
                pthread_cond_broadcast(&channelsInfo->cond_reset_channel);
//                pthread_mutex_unlock(&mutex);
            }
        }
    }

    return 1;
}
int main_handle_reset_conns(){
    //todo 初始化被关闭的连接
    for (int i = 0; i < rabbitmqConnsInfo.size; ++i) {
        if (rabbitmqConnsInfo.conns[i].reset_flag==1){
            //todo 关闭 连接释放资源
            if(rabbitmq_close_conn(i)==0){
                warn("main: conn close fail");
                return 0;
            }
            //todo 初始化 连接
            if(rabbitmq_init_conn(i)==0){
                warn("main: conn reopen fail");
                return 0;
            }
            //todo 初始化角色(生产者和消费者)
            if(init_roles_of_conn(i) == 0){
                warn("main: init roles fail");
                return 0;
            }

//            pthread_mutex_lock(&mutex);
            //todo 清除重置标志
            rabbitmqConnsInfo.conns[i].reset_flag=0;
            //todo 通知等待该连接重置的任务线程
            pthread_cond_broadcast(&rabbitmqConnsInfo.cond_reset_conn);
//            pthread_mutex_unlock(&mutex);
        }
    }
    return 1;
}

int consumer_message_handle(const amqp_envelope_t *envelope){

    //工具函数-显示接收到的数据
    pthread_mutex_lock(&log_mutex);
    amqp_dump(envelope->message.body.bytes, envelope->message.body.len);
    pthread_mutex_unlock(&log_mutex);


    //todo zc-业务处理逻辑

    return 1;
}




//todo 客户端启动函数
int rabbitmq_init_client(){
    //todo 初始化连接和通道(失败能自动释放连接资源)
    if(rabbitmq_init_conns()==0){
        return 0;
    }

    else{
//        int flag=1;
        //todo 交换机
        for (int i = 0; i < exchangesInfo.size; ++i) {
            if(rabbitmq_init_exchange(i)==0){
                warn("exchange[%d]: init error",i);
                return 0;
            }
        }
        //todo 队列
        for (int i = 0; i < queuesInfo.size; ++i) {
            if(rabbitmq_init_queue(i)==0){
                warn("queue[%d]: init error",i);
                return 0;
            }
        }
        //todo 绑定关系
        for (int i = 0; i < bindsInfo.size; ++i) {
            if(rabbitmq_init_bind(i)==0){
                warn("bind[%d]: init error");
                return 0;
            }
        }
        return 1;
    }

}

int rabbitmq_start_client(){
    if(rabbitmq_init_client()==0){
        warn("rabbitmq client: init fail");
        return 0;
    }
    else{
//        //todo 设置流的缓冲类型 无缓冲
//        setbuf(stdout, NULL);
//        setvbuf()

        //todo 初始化锁、条件变量、barrier
        pthread_mutexattr_t attr;
        pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex,&attr);//可重入锁

        pthread_cond_init(&cond_running, NULL);
        pthread_cond_init(&cond_deal, NULL);
        pthread_cond_init(&cond_exit,NULL);

        //todo 启动任务
        if(
            rabbitmq_start_consumers()==0
            ||rabbitmq_start_producers()==0
        ){
            warn("rabbitmq client: start task fail");
            return 0;
        };

        //todo main线程任务：负责连接和通道的申请和关闭，控制子线程的运行
        {
            warn("main: get lock");
            pthread_mutex_lock(&mutex);
            warn("main: locked");

            work_status=0;
            warn("main: status=%d",work_status);

            //todo 等待 运行
            while(flag_running!=1){
                warn("main: wait cond_running,work_status=%d",work_status);
                pthread_cond_wait(&cond_running, &mutex);
            }
            work_status=1;
            warn("main: status=%d",work_status);

            //todo 等待 stop
            while(flag_stop!=1){
                warn("main: wait cond_stop,work_status=%d",work_status);
                //todo 等待 子线程的通知处理
                pthread_cond_wait(&cond_deal, &mutex);

                //todo 检查是否有连接重置处理
                if(flag_reset_conn == 1){
                    work_status=2;
                    warn("main: status=%d",work_status);

                    //todo 检查和处理连接重置
                    if(main_handle_reset_conns()==0){
                        goto exit;
                    }


                }
                //todo 检查是否有通道重置处理
                if(flag_reset_channel == 1){
                    work_status=3;
                    warn("main: status=%d",work_status);

                    //todo 检查和处理通道重置
                    if(main_handle_reset_channels()==0){
                        goto exit;
                    }


                }
                work_status=1;
                warn("main: status=%d",work_status);
            }
exit:
            work_status=4;
            warn("main: status=%d",work_status);

            warn("main: close all threads,work_status=%d",work_status);

            //todo 等待 所有子线程结束任务 exit
            while(flag_exit!=1){
                warn("main: wait cond_exit,work_status=%d",work_status);
                pthread_cond_wait(&cond_exit,&mutex);
            }
            work_status=5;
            warn("main: status=%d",work_status);

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
        pthread_cond_destroy(&cond_deal);
        pthread_mutex_destroy(&mutex);
    }
}
