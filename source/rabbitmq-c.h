//
// Created by QAQlemon on 2025/8/8.
//

#ifndef C_RABBITMQDEMO_RABBITMQ_C_H
#define C_RABBITMQDEMO_RABBITMQ_C_H

#include "rabbitmq-c/amqp.h"
#include <rabbitmq-c/tcp_socket.h>
#include "utils.h"

#include "pthread.h"
#include <stdio.h>

//todo 宏定义
#define ROUTING_KEY_PLC_DATA "work.upload"
#define ROUTING_KEY_FAULT_REPORTS "work.fault"
#define ROUTING_KEY_DEAD_LETTER "deadRoutingKeyExample"
//交换机参数
#define ARGUMENT_EXCHANGE_ALTER "alternate-exchange"
//队列参数
// 注：字符串类型参数 对应 AMQP_FIELD_KIND_UTF8
#define ARGUMENT_QUEUE_00 "x-expires"                   //队列生存期
#define ARGUMENT_QUEUE_01 "x-message-ttl"               //消息生存期ms
#define ARGUMENT_QUEUE_02 "x-overflow"                  //拒绝策略
#define ARGUMENT_QUEUE_03 "x-single-active-consumer_task"    //单消费者活动模式,当其中一个消费者消费时，其余消费者将阻塞直至该消费者停止消费或断开连接
#define ARGUMENT_QUEUE_04 "x-dead-letter-exchange"      //死信队列-交换机
#define ARGUMENT_QUEUE_05 "x-dead-letter-routing-key"   //死信队列-路由键
#define ARGUMENT_QUEUE_06 "x-max-length"                //队列可容纳最大消息数量
#define ARGUMENT_QUEUE_07 "x-max-length-bytes"          //队列可容纳最大正文字节数
#define ARGUMENT_QUEUE_08 "x-queue-leader-locator"      //配置队列的领导者（Leader）定位器，和集群相关
#define ARGUMENT_QUEUE_09 ""



//结构体
typedef struct {
    char *hostname;
    int port;
    char *user;
    char *passwd;
}RabbitmqConfig_t;//连接登录信息

//任务信息
enum enum_exec_code_t{
    EXEC_ERROR=-1,                  //-1-执行异常
    EXEC_NORMAL,                    //0-正常执行(保持状态)
    EXEC_CORRECT,                   //1-结果正常

    EXEC_CONN_CLOSED,               //2-连接已关闭
    EXEC_CHANNEL_CLOSED,            //3-通道已关闭

    EXEC_PRODUCE_DATA_PREPARE_FAIL, //4-数据准备失败
    EXEC_PRODUCER_PUBLISH_FAIL,     //5-数据发布失败
    EXEC_PRODUCER_CONFIRM_FAIL,     //6-发布ACK失败

    EXEC_CONSUMER_MESSAGE_GET_FAIL, //7-消息获取失败
    EXEC_CONSUMER_MESSAGE_REJECT,   //8-拒绝消息

    EXEC_UNKNOWN_FRAME,             //9-非预期帧
    EXEC_NOTIFIED_STOP              //10-被通知停止运行
};
enum producer_task_status_t{
    PRODUCER_TASK_IDLE=0,
    PRODUCER_TASK_WAIT_DATA,
    PRODUCER_TASK_PUBLISH,
    PRODUCER_TASK_CONFIRM,
    PRODUCER_TASK_HANDLE_EXCEPTION,
    PRODUCER_TASK_EXIT
};
enum consumer_task_status_t{
    CONSUMER_TASK_IDLE=0,
    CONSUMER_TASK_WAIT_MESSAGE,
    CONSUMER_TASK_HANDLE_MESSAGE,
    CONSUMER_TASK_HANDLE_EXCEPTION,
    CONSUMER_TASK_EXIT
};
//typedef struct{
//    amqp_envelope_t envelope;//由amqp_consume_message填充
//    char *message;
//    char *exchange;
//    char *routing_key;;
//}message_t;
typedef int (*task_t) (void *);
typedef struct{
    int code;//-1-执行异常 0-正常执行(保持状态) 1-结果正常 2-连接已关闭 3-通道已关闭 4-数据准备失败 5-数据发布失败 6-发布ACK失败 | 7-消息获取失败 8-拒绝消息 | 9-非预期帧 | 10-被通知停止运行
    char *info;//执行信息
    void *data;//生产者-从控制板接收到的数据，用于发布  消费者-从rabbit服务获取到的数据，用于解析
}execInfo_t;
typedef struct taskInfoStruct{
    int type;//0-生产者 1-消费者
    int index;//下标
    int status;//状态 生产者（0-闲置（等待main重置连接或通道） 1-等待中 2-发布中 3-确认中 4-异常处理 5-结束）
               //    消费者（0-闲置（等待main重置连接或通道） 1-等待中 2-处理消息中 3-异常处理 4-结束）

    pthread_t thread_handle;
    task_t task;//todo 考虑针对
    execInfo_t execInfo;//执行信息
}taskInfo_t;

//连接和通道信息
#define CHANNEL_MAX_SIZE 3
typedef struct{
//    int conn_index;//连接索引
    int num;//通道号（1-65535）: a.队列、交换机、绑定声明使用 b.生产者专用通道 c.消费者专用通道
    int status;//标志位: 0-关闭 1-可用 2-已被线程使用
    int flag_reset;//重置标记位 0-无重置 1-需要重置
    taskInfo_t *taskInfo;
}channelEntity_t;
typedef struct{
    int size;
    channelEntity_t channels[CHANNEL_MAX_SIZE];

    pthread_cond_t cond_reset_channel;//用于main通知子线程通道重置情况
}channelInfo_t;

#define CONNECTION_MAX_SIZE 2
typedef struct{
    int status;//0-未打开 1-已创建连接结构体 2-已经创建套接字 3-已建立TCP连接 4-已登录 5-已打开通道(可用连接)
    int flag_reset;//连接重置标识 0-无重置 1-需要重置
    int task_nums;//统计数 当前活动任务数
    amqp_connection_state_t connState;//指针 amqp_new_connection()返回
    amqp_socket_t *socket;//amqp_tcp_socket_new()返回
    channelInfo_t channelsInfo;//不同线程使用不同通道
}connectionEntity;
typedef struct{
    int size;
    connectionEntity conns[CONNECTION_MAX_SIZE];

    pthread_cond_t cond_reset_conn;//用于main通知子线程连接重置情况
}connectionsInfo_t;//包含连接、通道信息

#define QUEUE_MAX_SIZE 4
typedef struct {
    int type;//0-整数 1-字符串 3-布尔
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
    char *type;       //0-direct 1-fanout 2-topic 3-headers
    int passive;
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
    int conn_index;     //连接：rabbitmqConnsInfo.conns[conn_index]
    int channel_index;  //通道：rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index]
    int queue_index;    //队列：
    char *consumer_tag; //消费者名称: 为NULL时由rabbitmq服务自动分配

    //消息接收的全局参数设置
    int no_local;//是否接收自己发布的消息:0-关闭 1-开启
    int no_ack;//消费确认模式：0-手动ACK 1-自动ACK
    int exclusive;//排他消费(队列为消费者私有) 0-关闭 1-开启

    int requeue;//消息拒绝后是否重新入队 0-关闭 1-开启

    //任务信息
    taskInfo_t taskInfo;
}consumerEntity_t;
typedef struct{
    int size;
    consumerEntity_t consumers[CONSUMER_MAX_SIZE];
}consumers_t;

#define PRODUCER_MAX_SIZE 3
typedef struct{
    int conn_index;     //连接：rabbitmqConnsInfo.conns[conn_index]
    int channel_index;  //通道：rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index]
    int exchange_index; //交换机

    int confirmMode;//0-无发布确认 1-启用发布确认

    char *routingKey;//路由键
    int mandatory;//强制投送，投送失败返回:AMQP_BASIC_RETURN_METHOD
    int immediate;//立即投送到消费者，投送失败返回:AMQP_BASIC_RETURN_METHOD

    //消息发送的全局设置
    amqp_basic_properties_t props;// 内容头帧的消息属性 投递模式

    //任务信息
    taskInfo_t taskInfo;
}producerEntity_t_t;
typedef struct{
    int size;
    producerEntity_t_t producers[PRODUCER_MAX_SIZE];//0-生产者 1-消费者
}producers_t;


//todo 全局变量
extern RabbitmqConfig_t rabbitmqConfigInfo;//配置信息
extern connectionsInfo_t rabbitmqConnsInfo;//连接和通道信息 0-消费 1-生产
extern RabbitmqExchanges_t exchangesInfo;//交换机
extern RabbitmqQueues_t queuesInfo;//队列
extern RabbitmqBinds_t bindsInfo;//绑定信息

//todo 目前一个消费者对应一个连接一个通道一个线程一个队列 一个队列由可多个消费者线程处理
extern consumers_t consumersInfo;//消息消费者
extern producers_t producersInfo;//消息生产者

//todo main与子线程间通
typedef struct {
    pthread_mutex_t mutex;
    taskInfo_t *taskInfo;
}lock_t;

extern pthread_mutex_t log_mutex;//用于日志输出
extern pthread_mutex_t global_task_mutex;//用于主线程和任务线程通讯

extern volatile int thread_counts;
enum client_work_status_t{
    CLIENT_STATUS_READY=0,          //0-ready就绪
    CLIENT_STATUS_RUNNING,          //1-running运行
    CLIENT_STATUS_RESETTING_CONN,   //2-conn重置
    CLIENT_STATUS_RESETTING_CHANNEL,//3-channel重置
    CLIENT_STATUS_STOPPING_TASKS,   //4-stop停止
    CLIENT_STATUS_EXIT              //5-exit
};
extern volatile int work_status;//0-ready就绪 1-running运行 2-conn重置 3-channel重置 4-stop停止 5-exit

extern volatile int flag_running;
extern pthread_cond_t cond_running;//子->主
extern volatile int flag_stop;
extern pthread_cond_t cond_deal;//子->主
extern volatile int flag_exit;
extern pthread_cond_t cond_exit;//主->子

//todo 全局重置标识
extern volatile int flag_reset_conn;//
extern volatile int flag_reset_channel;//-1

//todo synchronized
//任务 通知main所有任务以就绪
void task_notify_main_run();
//任务 通知main停止
void task_notify_main_stop(taskInfo_t *taskInfo);
//任务 通知main处理
void task_notify_main_deal();
//线程 通知main退出
void task_notify_main_exit();
//
void task_notify_main_reset_conn(int conn_index);
void task_notify_main_reset_channel(int conn_index, int channel_index);
//
void task_wait_main_reset_conn(int conn_index);
void task_wait_main_reset_channel(int conn_index, int channel_index);
//
void init_synchronize_tools();
void destroy_synchronize_tools();
void clean_synchronize_resources(void *arg);

void main_cancel_all_task();


//todo check函数
int rabbitmq_check_conn_index(int conn_index);
int rabbitmq_check_channel_index(int conn_index,int channel_index);
int rabbitmq_check_queue_index(int queue_index);
int rabbitmq_check_exchange_index(int exchange_index);
int rabbitmq_check_bind_index(int bind_index);

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

int rabbitmq_init_queue(int queue_index);
int rabbitmq_init_exchange(int exchange_index);
int rabbitmq_init_bind(int bind_index);

int rabbitmq_init_consumer(int consumer_index);
//int rabbitmq_init_consumers();
int rabbitmq_init_producer(int producer_index);
//int rabbitmq_init_producers();

int init_role_by_taskInfo(taskInfo_t *taskInfo);
int init_roles_of_conn(int conn_index);

//todo start函数
int rabbitmq_start_producer(int index);
int rabbitmq_start_producers();

int rabbitmq_start_consumer(int index);
int rabbitmq_start_consumers();
//todo 客户端启动函数
int rabbitmq_init_client();
int rabbitmq_start_client();

//todo tools函数
//分配指定连接下的可用通道
int get_available_channel(int conn_index);
//获取code对应的信息
char *get_code_info(int exit_code);
//获取单个连接下的任务数量
int get_task_num_of_conn(int conn_index);

//todo task函数
void *rabbitmq_task(void *arg);
//生产端和消费端的状态处理
int get_an_message(void *arg);//下拉 定时数据
void *rabbitmq_consumer_deal(void *arg);

int wait_ack(taskInfo_t *taskInfo);
void *rabbitmq_producer_deal(void *arg);

//业务函数 非阻塞 后续扩展阻塞支持
int consumer_task_handle_cron_message(void *arg);//解析转发 定时消息数据
int producer_task_prepare_device_message(void *arg);//准备 采集设备数据
int producer_task_prepare_fault_message(void *arg);//准备 设备故障数据

//业务函数 注：此类函数处于while循环中，仅负责单次操作，每次操作完必须返回操作结果
int producer_task_upload_device_data(void *arg);//上传 采集设备数据
int producer_task_upload_fault_data(void *arg);//上传 设备故障数据



//todo parse解析消息
int message_parse(char *buffer,int size);

//todo pack打包消息
int message_pack(char *buffer,int size);

//todo handle函数
int consumer_message_handle(const amqp_envelope_t *envelope);
int producer_prepare_message(char *buffer,int size);

int main_handle_reset_channels();
int main_handle_reset_conns();





//todo 打印线程退出时记录的信息
void log_threads_exitInfo();
//日志打印
void vlog(FILE *fd,char *str,va_list args);
void log(FILE *fd ,char *str,...);
void info(char *str,...);
void warn(char *str,...);

void print_lock_info(const pthread_mutex_t *mutex);
void print_cond_info(const pthread_cond_t *cond);
void print_synchronized_info(const pthread_mutex_t *mutex,const pthread_cond_t *cond);




#endif //C_RABBITMQDEMO_RABBITMQ_C_H
