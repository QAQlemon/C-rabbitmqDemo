//
// Created by QAQlemon on 2025/8/8.
//

#ifndef C_RABBITMQDEMO_RABBITMQ_C_H
#define C_RABBITMQDEMO_RABBITMQ_C_H

#include "rabbitmq-c/amqp.h"
#include <rabbitmq-c/tcp_socket.h>
#include "utils.h"
#include "log.h"



//todo 宏定义
#define ROUTING_KEY_PLC_DATA "work.upload"
#define ROUTING_KEY_FAULT_REPORTS "work.fault"
//交换机参数
#define ARGUMENT_EXCHANGE_ALTER "alternate-exchange"
//队列参数
#define ARGUMENT_QUEUE_00 "x-expires"                   //队列生存期
#define ARGUMENT_QUEUE_01 "x-message-ttl"               //消息生存期
#define ARGUMENT_QUEUE_02 "x-overflow"                  //拒绝策略
#define ARGUMENT_QUEUE_03 "x-single-active-consumer_task"    //单消费者活动模式,当其中一个消费者消费时，其余消费者将阻塞直至该消费者停止消费或断开连接
#define ARGUMENT_QUEUE_04 "x-dead-letter-exchange"      //死信队列-交换机
#define ARGUMENT_QUEUE_05 "x-dead-letter-routing-key"   //死信队列-路由键
#define ARGUMENT_QUEUE_06 "x-max-length"                //队列可容纳最大消息数量
#define ARGUMENT_QUEUE_07 "x-max-length-bytes"          //队列可容纳最大正文字节数
#define ARGUMENT_QUEUE_08 "x-queue-leader-locator"      //配置队列的领导者（Leader）定位器，和集群相关
#define ARGUMENT_QUEUE_09 ""

//别名
typedef void *(*task_t) (void *) ;

//结构体
typedef struct {
    char hostname[20];
    int port;
}RabbitmqConfig_t;//连接登录信息


#define CHANNEL_MAX_SIZE 3
typedef struct{
//    int conn_index;//连接索引
    int num;//通道号（1-65535）: a.队列、交换机、绑定声明使用 b.生产者专用通道 c.消费者专用通道
    int status;//标志位: 0-未启用 1-已启用
}channelEntity_t;
typedef struct{
    int size;
    channelEntity_t channels[CHANNEL_MAX_SIZE];
}channelInfo_t;

#define CONNECTION_MAX_SIZE 2
typedef struct{
    int status;//0-未打开 1-已连接 2-已登录 3-通道已打开
    amqp_connection_state_t connState;//指针 amqp_new_connection()返回
    amqp_socket_t *socket;//amqp_tcp_socket_new()返回
    channelInfo_t channelsInfo;//不同线程使用不同通道
}connectionEntity;
typedef struct{
    int size;
    connectionEntity conns[CONNECTION_MAX_SIZE];
}connectionsInfo_t;//包含连接、通道信息

#define QUEUE_MAX_SIZE 3
typedef struct {
    int type;//0-整数 1-字符串
    char *key;
    union {
        int integer;
        char *str;
    }value;
}xargs_t;
typedef struct{
    char *name;

    int type;//0-classic 1-quorum 2-stream | 3-Lazy 4-dead-letter
    int passive;//0-队列不存在会自动创建，若当前参数设置和已有队列检查参数不匹配返回错误 1-检查队列是否存在，不会尝试创建队列
    int durability;//队列持久化 1-durable 2-transient
    int exclusive;//排他队列（队列为连接私有，同连接下队列可见） 0-不开启 1-开启
    int auto_delete;//0-不开启 1-开启

    xargs_t args[9];//用于构建amqp_table_t
}RabbitmqQueueEntity_t;
typedef struct {
    int size;
    RabbitmqQueueEntity_t queues[QUEUE_MAX_SIZE];
}RabbitmqQueues_t;//队列

#define EXCHANGE_MAX_SIZE 3
typedef struct{
    char *name;
    int type;       //0-direct 1-fanout 2-topic 3-headers
    int durability;
    int autoDelete; //无消费者自动删除队列
    int internal;   //标识为内部交换机
//    amqp_table_t args;
    xargs_t args[1];
}RabbitmqExchangeEntity ;
typedef struct {
    int size;
    RabbitmqExchangeEntity exchanges[EXCHANGE_MAX_SIZE];
}RabbitmqExchanges_t;//交换机

#define BIND_MAX_SIZE 3
typedef struct{
//    int conn_index;
//    int channel_index;//通道

    char *routingKey;//路由键

    int exchange_index;
    int queue_index;
}RabbitmqBindEntity_t;
typedef struct {
    int size;
    RabbitmqBindEntity_t binds[BIND_MAX_SIZE];
}RabbitmqBinds_t;//绑定



#define CONSUMER_MAX_SIZE 2
typedef struct{
    int index;          //用于线程快速查找消费者信息
    int conn_index;     //连接：rabbitmqConnsInfo.conns[conn_index]
    int channel_index;  //通道：rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index]
    char *consumer_tag;

    //消息接收的全局设置
    int no_local;//是否接收自己发布的消息:0-关闭 1-开启
    int no_ack;//消费确认模式：0-手动ACK 1-自动ACK
    int exclusive;//排他消费(队列为消费者私有) 0-关闭 1-开启

    //线程句柄
    pthread_t thread_handle;
    task_t task;
}consumerEntity_t;
typedef struct{
    int size;
    consumerEntity_t consumers[CONSUMER_MAX_SIZE];
}consumers_t;

#define PRODUCER_MAX_SIZE 3
typedef struct{
    int index;          //用于线程快速查找生产者信息
    int conn_index;     //连接：rabbitmqConnsInfo.conns[conn_index]
    int channel_index;  //通道：rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index]

    int confirmMode;//0-无发布确认 1-启用发布确认

    //消息发送的全局设置
    amqp_basic_properties_t props;// 内容头帧的消息属性 投递模式

    //线程句柄
    pthread_t thread_handle;
    task_t task;
}producerEntity_t_t;
typedef struct{
    int size;
    producerEntity_t_t producers[PRODUCER_MAX_SIZE];
}producers_t;


typedef struct{
    char *info;
    int type;//1-生产者 2-消费者
    int index;//下标
}exitInfo_t;

//全局变量
extern RabbitmqConfig_t rabbitmqConfigInfo;//配置信息

extern connectionsInfo_t rabbitmqConnsInfo;//连接和通道信息 0-消费 1-生产

extern RabbitmqExchanges_t exchangesInfo;//交换机

extern RabbitmqQueues_t queuesInfo;//队列

extern RabbitmqBinds_t bindsInfo;//绑定信息

extern pthread_mutex_t log_mutex;
extern pthread_mutex_t mutex;
extern pthread_cond_t cond_start;
extern pthread_cond_t cond_exit;
//extern pthread_barrier_t barrier;
extern exitInfo_t exitInfo;


extern consumers_t consumersInfo;//消息消费者
extern producers_t producersInfo;//消息生产者

//todo check函数
int rabbitmq_check_conn_index(int conn_index);
int rabbitmq_check_channel_index(int conn_index,int channel_index);

//todo reset函数 重置状态和NULL
int rabbitmq_reset_channel(int conn_index,int channel_index);
int rabbitmq_reset_channels(int conn_index);
int rabbitmq_reset_conn(int conn_index);
int rabbitmq_reset_conns();

//todo close函数
int rabbitmq_close_channel(int conn_index,int channel_index);
int rabbitmq_close_channels(int conn_index);
int rabbitmq_close_conn(int conn_index);
int rabbitmq_close_conns();

//todo init函数
int rabbitmq_init_channel(int conn_index,int channel_index);
int rabbitmq_init_channels(int conn_index);
int rabbitmq_login_conn(amqp_connection_state_t conn);
int rabbitmq_init_conn(int conn_index);
int rabbitmq_init_conns();



//todo start函数
int rabbitmq_start_producer(int index);
int rabbitmq_start_producers();

int rabbitmq_start_consumer(int index);
int rabbitmq_start_consumers();


//todo task函数
//下拉
void *consumer_task_00(void *arg);//定时数据
//上传
void *producer_task_upload_device_data(void *arg);//采集设备数据
void *producer_task_upload_fault_data(void *arg);//设备故障数据



//todo parse解析消息
//int message_parse(char *buffer,int size);

//todo pack打包消息
//int message_pack(char *buffer,int size);

//todo handle函数
int consumer_handle_message(const amqp_envelope_t *envelope);
int producer_prepare_message(char *buffer,int size);




//todo 客户端启动函数
int rabbitmq_init_client();
int rabbitmq_start_client();




//日志打印
void vlog(FILE *fd,char *str,va_list args);
void info(char *str,...);
void warn(char *str,...);



#endif //C_RABBITMQDEMO_RABBITMQ_C_H
