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
#define ROUTING_KEY_UPLOAD "work.upload"
#define ROUTING_KEY_DEAD "work.dead"
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

//todo函数声明

//结构体
typedef struct {
    char *hostname[20];
    int port;

}RabbitmqConfig;//连接登录信息

typedef struct {
    char *name;
    int type;//classic quorum stream   Lazy dead-letter
    int passive;//
    int durability;//队列持久化
    int exclusive;//排他队列
    int auto_delete;
    amqp_table_t args;
}RabbitmqQueue;//队列

typedef struct {
    char *name;
    int type;       //direct fanout topic headers
    int durability;
    int autoDelete; //无消费者自动删除队列
    int Internal;   //标识为内部交换机
    amqp_table_t args;
}RabbitmqExchange;//交换机

typedef struct {
    amqp_connection_state_t *connState;
    int channel;//通道

    char *routingKey;//路由键

    RabbitmqQueue queue;
    RabbitmqExchange exchange;
}RabbitmqBind;//绑定

typedef struct{

}consumer;

typedef struct{

}producer;

typedef struct {
    amqp_connection_state_t connState;
    amqp_socket_t *socket;

    RabbitmqConfig config;//配置信息

    RabbitmqBind binds[20];//绑定信息

    consumer consumers[20];//消息消费者
    producer producers[20];//消息生产者
}RabbitmqClient;





RabbitmqBind binds[2];



#endif //C_RABBITMQDEMO_RABBITMQ_C_H
