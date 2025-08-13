
#include "amqp.h"
#include <amqp_tcp_socket.h>


#include <stdio.h>
#include <unistd.h>
#include "utils.h"


//结构体
typedef struct {
    char *hostname[20];
    int port;

}RabbitmqConfig;

typedef struct {

}RabbitmqQueueArgument;
typedef struct {
    char *name;
    int type;//classic quorum stream   Lazy dead-letter
    int passive;
    int durability;
    int exclusive;
    int auto_delete;
    RabbitmqQueueArgument args[];
}RabbitmqQueue;//队列

typedef struct {

}RabbitmqExchangeArgument;
typedef struct {
    char *name;
    int type;//direct fanout topic headers
    int durability;
    int autoDelete;
    int Internal;   //内部交换机
    RabbitmqExchangeArgument args[];
}RabbitmqExchange;//交换机

typedef struct {
    amqp_connection_state_t *connState;
    int channel;//通道

    char *routingKey;//路由键

    RabbitmqQueue queue;
    RabbitmqExchange exchange;
}RabbitmqBind;//绑定

typedef struct {
    amqp_connection_state_t connState;
    amqp_socket_t *socket;

    RabbitmqConfig config;//配置信息

    RabbitmqBind binds[20];//绑定信息
}RabbitmqClient;

//宏定义
#define ROUTING_KEY_UPLOAD "work.upload"
#define ROUTING_KEY_DEAD "work.dead"

//交换机参数
#define ARGUMENT_EXCHANGE_ALTER "alternate-exchange"
//队列参数
#define ARGUMENT_QUEUE_00 "x-expires"                   //队列生存期
#define ARGUMENT_QUEUE_01 "x-message-ttl"               //消息生存期
#define ARGUMENT_QUEUE_02 "x-overflow"                  //拒绝策略
#define ARGUMENT_QUEUE_03 "x-single-active-consumer"    //单消费者活动模式,当其中一个消费者消费时，其余消费者将阻塞直至该消费者停止消费或断开连接
#define ARGUMENT_QUEUE_04 "x-dead-letter-exchange"      //死信队列-交换机
#define ARGUMENT_QUEUE_05 "x-dead-letter-routing-key"   //死信队列-路由键
#define ARGUMENT_QUEUE_06 "x-max-length"                //队列可容纳最大消息数量
#define ARGUMENT_QUEUE_07 "x-max-length-bytes"          //队列可容纳最大正文字节数
#define ARGUMENT_QUEUE_08 "x-queue-leader-locator"      //配置队列的领导者（Leader）定位器，和集群相关
#define ARGUMENT_QUEUE_09 ""


//全局变量
amqp_connection_state_t connState;
amqp_socket_t *socket = NULL;

RabbitmqBind binds[2]={
    //连接状态 通道 路由键 队列 交换机
    {NULL,1,ROUTING_KEY_UPLOAD,{"workQueue"},{"myExchange"}},
    {NULL,1,ROUTING_KEY_DEAD,{"deadQueue"},{"myExchange"}}
};


//函数声明
int message_handle(const amqp_envelope_t *envelope);



void main() {
    char const *hostname="192.168.200.131";
    int port=5672;
    int status;

//    amqp_exchange_declare();
//    amqp_queue_declare()

    //分配连接结构体 和 创建套接字
    connState = amqp_new_connection();
    if(connState != NULL){
        socket = amqp_tcp_socket_new(connState);
    }
    if(!socket){
        die("creating TCP socket");
    }
    printf("ok: socket created\n");


    //建立连接
    status = amqp_socket_open(
            socket,
            hostname,   //ip
            port        //端口
    );
    if (status!=AMQP_STATUS_OK) {
        die("opening TCP socket");
    }
    printf("ok: tcp socket opened\n");

    //登陆验证
    amqp_rpc_reply_t reply = amqp_login(
            connState,
            "/",
            0,//channel_max
            131072,//frame_max
            0,//heartbeat
            AMQP_SASL_METHOD_PLAIN,//内部用户密码登录 | 外部系统登录
            "root",
            "123123"
    );
    if(reply.reply_type!=AMQP_RESPONSE_NORMAL){
        die_on_amqp_error(
                reply,
                "Logging in"
        );
    }
    printf("ok: login\n");



    //打开通道
    amqp_channel_open(connState,1);


    //队列声明
    amqp_bytes_t queue_name;
    {
        amqp_queue_declare_ok_t *r = amqp_queue_declare(
            connState,
            1,
//            amqp_empty_bytes,//队列标签(空字节则服务器自动生成)
            amqp_cstring_bytes("myQueue"),
            0,//passive
            1,//durable
            0,//exclusive
            0,//auto_delete
            amqp_empty_table//额外参数 (通常用amqp_empty_table)
        );

        die_on_amqp_error(amqp_get_rpc_reply(connState), "Declaring queue");

        //分配空间-完全复制原始字节串的内容到新分配的内存中
        queue_name = amqp_bytes_malloc_dup(r->queue);//注意调用amqp_bytes_free()释放内存空间
        if(queue_name.bytes==NULL){

        }
    }

    //交换机声明
    amqp_exchange_declare(
        connState,
        1,                       //channel
        amqp_cstring_bytes("test"),//exchange
        amqp_cstring_bytes("topic"),//type
        0,    //passive 被动模式标志
        0,
        0,
        0,//internal 内部交换机
        amqp_empty_table
    );
    die_on_amqp_error(amqp_get_rpc_reply(connState), "declare exchange");


    //绑定交换机
    amqp_queue_bind_ok_t *pT = amqp_queue_bind(
            connState,
            1,                       //channel
            queue_name,
            amqp_cstring_bytes("myExchange"),//exchange
            amqp_cstring_bytes(""),  //bindingkey
            amqp_empty_table
    );
    die_on_amqp_error(amqp_get_rpc_reply(connState), "Binding queue");

    //启动消息消费
    //告诉 RabbitMQ 服务器开始向客户端推送指定队列中的消息
    amqp_basic_consume(
            connState,
            1,
            queue_name,
//            amqp_empty_bytes,// 消费者标签(空字节则服务器自动生成)
            amqp_cstring_bytes("consumer00"),
            0,//no_local 是否接收自己发布的消息
            0,//no_ack 0-手动ACK 1-自动ACK
            0,//exclusive 排他消费
            amqp_empty_table//额外参数 (通常用amqp_empty_table)
    );
    die_on_amqp_error(amqp_get_rpc_reply(connState), "Consuming");
    //todo 可以在单个通道上启动多个队列



    //处理消息
    while(1){
        amqp_rpc_reply_t res;//RPC回调
        amqp_envelope_t envelope;//用于存储消息内容
        amqp_frame_t frame;
        int success=0;


        //用于 内存优化 的关键函数
        //  注：在还未处理完消息内容(如访问envelope.message.body)前调用会导致数据丢失
        amqp_maybe_release_buffers(connState);//，控制着库内部缓冲区的内存释放行为
        //阻塞获取消息
        res = amqp_consume_message(connState, &envelope, NULL, 0);


        if (AMQP_RESPONSE_NORMAL != res.reply_type) {
            if (AMQP_RESPONSE_LIBRARY_EXCEPTION == res.reply_type && AMQP_STATUS_UNEXPECTED_STATE == res.library_error) {
                if (AMQP_STATUS_OK != amqp_simple_wait_frame(connState, &frame)) {
                    return;
                }

            }


            //todo 处理消息
            success = message_handle(&envelope);


            //手动ACK处理
            if(success){
                amqp_basic_ack(
                    connState,
                    1,//channel
                    envelope.delivery_tag,//需要被确认消息的标识符
                    0//批量确认
                );
            }
            else{
                amqp_basic_reject(
                    connState,
                    1,//channel
                    envelope.delivery_tag,//需要被拒绝消息的标识符
                    1
                );
//                amqp_basic_nack(connState,);
            }


            fflush(stdout);
            break;
        }
        else{
            //释放空间
//        amqp_destroy_message(&envelope.message);
            amqp_destroy_envelope(&envelope);//底层会调用 amqp_destroy_messag()
        }




    }


}
int message_handle(const amqp_envelope_t *envelope){

    //工具函数-显示接收到的数据
    amqp_dump(envelope->message.body.bytes, envelope->message.body.len);

    //todo zc-业务处理逻辑


    return 0;
}

