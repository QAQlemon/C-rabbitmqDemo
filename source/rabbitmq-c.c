#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>

#include "rabbitmq-c.h"

pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex;
volatile int thread_counts=0;
volatile int work_status=0;//0-ready就绪 1-running运行 2-stop停止 3-exit 4-terminated
volatile int flag_running=0;
pthread_cond_t cond_running;
volatile int flag_stop=0;
pthread_cond_t cond_stop;
volatile int flag_exit=0;
pthread_cond_t cond_exit;

//exitInfo_t exitInfo={
//    .info=NULL,
//    .type=0,
//    .index=0
//};


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
            .type="topic",
//            .passive=0,
            .durability=1,
            .autoDelete=0,
            .internal=0,
            .args={0}
        },
        //todo 死信交换机
        {
            .name="deadLetterExchange",
            .type="direct",
//            .passive=0,
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
//            .passive=0,
            .durability=1,
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
            .args={
//                {.type=0,.key=ARGUMENT_QUEUE_00,.value.integer=},
                {.type=0,.key=ARGUMENT_QUEUE_01,.value.integer=3600},
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
//            .passive=0,
            .exclusive=0,
            .auto_delete=0,
            //todo 指定死信交换机
            .args={
                {.type=1,.key=ARGUMENT_QUEUE_04,.value.str="deadLetterExchange"},
                {.type=1,.key=ARGUMENT_QUEUE_05,.value.str="deadRoutingKeyExample"}
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
void log_threads_exitInfo(){
    info("========================(exitInfo)==========================");
    //todo 生产者
    for (int i = 0; i < producersInfo.size; ++i) {
        info(producersInfo.producers[i].exitInfo.info,i);
    }

    //todo 消费者
    for (int i = 0; i < consumersInfo.size; ++i) {
        info(consumersInfo.consumers[i].exitInfo.info,i);
    }
}

void vlog(FILE *fd,char *str,va_list args){

    vfprintf(fd,str,args);

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
int rabbitmq_check_queue(int queue_index){
    if(queue_index<0 || queue_index>queuesInfo.size){
        warn("queue check:fail,queue_index=%d",queue_index);
        return 0;
    }

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
int rabbitmq_check_exchange(int exchange_index){
    if(exchange_index<0 || exchange_index>exchangesInfo.size){
        warn("exchange check:fail,exchange_index=%d",exchange_index);
        return 0;
    }

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
int rabbitmq_check_bind(int bind_index){
    if(bind_index<0 || bind_index>bindsInfo.size){
        warn("bind check:fail,bind_index=%d",bind_index);
        return 0;
    }
    RabbitmqBindEntity_t bind = bindsInfo.binds[bind_index];
    if(rabbitmq_check_exchange(bind.exchange_index)==0){
        warn("bind check: fail ,exchange=%d",bind.exchange_index);
        return 0;
    }
    if(rabbitmq_check_queue(bind.queue_index)==0){
        warn("bind check: fail ,queue=%d",bind.queue_index);
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
        //todo 设置连接相关指针 NULL
        rabbitmqConnsInfo.conns[conn_index].connState =NULL;
        //todo 设置连接套接字 NULL
        rabbitmqConnsInfo.conns[conn_index].socket=NULL;

        //todo 通道状态 0-未启用
        rabbitmq_reset_channels(conn_index);

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
        if(pConn == NULL){
            warn("conn[%d] init: create fail",conn_index);
            return 0;
        }
        rabbitmqConnsInfo.conns[conn_index].connState = pConn;
        //todo 创建tcp套接字
        amqp_socket_t *socket = amqp_tcp_socket_new(pConn);
        if(socket == NULL){
            warn("conn[%d] init: socket create fail",conn_index);
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
            return 0;
        }
        rabbitmqConnsInfo.conns[conn_index].socket = socket;
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
                warn("conn[%d] init: fail",i);
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

    //todo 通知 线程已就绪
    notify_task_run();
    {
        while(1){
            //todo 运行
            if(work_status==1){
                //todo 业务处理
                info("thread consumer[%d]: running",consumer_index);

                //todo 通知 线程已停止
                notify_task_stop();
                break;
            }
            else if(work_status==2){
                info("thread consumer[%d]:stopped",consumer_index);
                break;
            }
        }

        //todo 退出前处理
        {
            //todo 停止前进行资源释放

            //todo 记录退出信息
            info("thread consumer[%d]: exit,work_status=%d",consumer_index,work_status);
            consumersInfo.consumers[consumer_index].exitInfo.info="consumer[%d]: exited!";
        }

        //todo 通知任务已全部结束
        notify_task_exit();

    }
}
//上传
//采集设备数据
void *producer_task_upload_device_data(void *arg){

    int producer_index=*(int *)arg;

    //todo 通知 线程已就绪
    notify_task_run();
    {
        while(1){
            //todo 运行
            if(work_status==1){
                //todo 业务处理
//                info("thread producer[%d]: running",producer_index);

                //todo 处理出错或其它会导致客户端状态变为 2-stop
//                notify_task_stop();
//                break;
            }
            else if(work_status==2){
                info("thread producer[%d]: stopped",producer_index);

                break;
            }
        }
        //todo 推出前处理
        {
            //todo 停止前进行资源释放

            //todo 记录退出信息
            info("thread producer[%d]: exit,work_status=%d",producer_index,work_status);
            producersInfo.producers[producer_index].exitInfo.info="producer[%d]: exited!";

        }

        //todo 通知任务已全部结束
        notify_task_exit();

    }

}
//设备故障数据
void *producer_task_upload_fault_data(void *arg){
    int producer_index=*(int *)arg;

    //todo 通知 线程已就绪
    notify_task_run();
    {
        while(1){
            //todo 运行
            if(work_status==1){
//                info("thread producer[%d]: running",producer_index);
                //todo 业务处理

                //todo 处理出错或其它会导致客户端状态变为 2-stop
//                notify_task_stop();
//                break;
            }
            else if(work_status==2){
                info("thread producer[%d]: stopped",producer_index);
                break;
            }
        }
        //todo 退出前处理
        {
            //todo 停止前进行资源释放

            //todo 记录退出信息
            info("thread producer[%d]: exit,work_status=%d",producer_index,work_status);
            producersInfo.producers[producer_index].exitInfo.info="producer[%d]: exited!";
        }

        //todo 等待 退出通知 3-exit
        notify_task_exit();
    }
}

void notify_task_run(){
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
void notify_task_stop(){
    //todo 处理出错或其它会导致客户端状态变为 2-stop
    {
        pthread_mutex_lock(&mutex);
        flag_stop=1;
        pthread_cond_broadcast(&cond_stop);
        pthread_mutex_unlock(&mutex);
    }
}
void notify_task_exit(){
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






//todo 客户端启动函数
int rabbitmq_init_client(){
    //todo 初始化连接和通道(失败能自动释放连接资源)
    if(rabbitmq_init_conns()==0){
        return 0;
    }

    else{
        int flag=1;
        //todo 交换机检查
        for (int i = 0; i < exchangesInfo.size; ++i) {
            if(rabbitmq_check_exchange(i)==0){
                flag=0;
                warn("exchange[%d]:check error",i);
                break;
            }
        }
        //todo 队列检查
        for (int i = 0; i < queuesInfo.size; ++i) {
            if(rabbitmq_check_queue(i)==0){
                flag=0;
                warn("queue[%d]:check error",i);
                break;
            }
        }
        //todo 绑定关系检查


        return flag;
    }

}

int rabbitmq_start_client(){
    if(rabbitmq_init_client()==0){
        warn("rabbitmq client: init fail",stderr);
        return 0;
    }
    else{
//        //todo 设置缓冲类型 无缓冲
//        setbuf(stdout, NULL);

        //todo 初始化锁、条件变量、barrier
        pthread_mutex_init(&mutex,NULL);
        pthread_cond_init(&cond_running, NULL);
        pthread_cond_init(&cond_stop, NULL);
        pthread_cond_init(&cond_exit,NULL);

        //todo 启动任务
        rabbitmq_start_consumers();
        rabbitmq_start_producers();

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
}
