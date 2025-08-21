#include <stdarg.h>
#include "rabbitmq-c.h"


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
        }
    }
};
producers_t producersInfo={
    .size=1,
    .producers={
        //todo 生产者
        {
            .conn_index=0,
            .channel_index=2,
            .confirmMode=0,
            //todo 消息持久化设置
            .props={
                ._flags=AMQP_BASIC_DELIVERY_MODE_FLAG,
                .delivery_mode=2
            }
        }
    }
};
void vlog(FILE *fd,char *str,va_list args){
    vfprintf(fd,str,args);
}
void info(char *str,...){
    va_list list;
    va_start(list,str);
    vlog(stdout,str,list);
    va_end(list);
}
void warn(char *str,...){
    va_list list;
    va_start(list,str);
    vlog(stderr,str,list);
    va_end(list);
}
void main(){
    
}





int rabbitmq_init_client(){
    //todo 初始化连接信息
    rabbitmq_init_conns();

}
//todo restart函数
//todo reset函数
//重置单个连接下的单个通道
int rabbitmq_reset_channel(int conn_index,int channel_index){
    if(conn_index < 0 || conn_index >= rabbitmqConnsInfo.size){
        return 0;
    }
    else if(channel_index < 0 || channel_index >= rabbitmqConnsInfo.conns[conn_index].channelsInfo.size){
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
    if(conn_index < 0 || conn_index >= rabbitmqConnsInfo.size){
        return 0;
    }
    else{
        int flag=0;
        for (int i = 0; i < rabbitmqConnsInfo.conns[conn_index].channelsInfo.size; ++i) {
            flag|=rabbitmq_reset_channel(conn_index,i);
            if(flag==0){
                warn("reset_channel: fail,%d",i);
                break;
            }
        }
        return flag;
    }
}
//reset连接初始化前调用，连接关闭资源回收后调用
int rabbitmq_reset_conn(int conn_index){
    if(conn_index < 0 || conn_index >= rabbitmqConnsInfo.size){
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
        int flag=0;
        for (int i = 0; i < size; ++i) {
            flag|=rabbitmq_reset_conn(i);
            if(flag==0){
                warn("reset_channel: fail,%d",i);
                break;
            }
        }
        return flag;
    }

}







int rabbitmq_init_conns(){
    int size;
    
    size = rabbitmqConnsInfo.size;
    if(size > CONNECTION_MAX_SIZE){
        warn("conns: size=%d,max=%d",size,CONNECTION_MAX_SIZE);
        return 0;
    }

    //todo 清空连接数据
    if(rabbitmq_reset_conns()==0){
        return 0;
    }

    //todo 初始化连接数据
    if(size > 0){
        for (int i = 0; i < size; ++i) {
            //todo 初始化连接状态
            if(rabbitmqConnsInfo.conns[i].status != 0){
                rabbitmqConnsInfo.conns[i].status = 0;
            }


            //todo 创建连接结构体数据
            amqp_connection_state_t pConn = amqp_new_connection();
            if(pConn == NULL){
                warn("conn[%d]: create fail",i);
                return 0;
            }
            rabbitmqConnsInfo.conns[i].connState == pConn;

            //
            
        }
    }
};
void rabbitmq_login(){

}
