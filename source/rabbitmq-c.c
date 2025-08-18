
#include "rabbitmq-c/amqp.h"
#include <rabbitmq-c/tcp_socket.h>
#include "utils.h"


#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include "pthread.h"
#include <sys/time.h>
#include <inttypes.h>
//结构体
typedef struct {
    char *hostname[20];
    int port;

}RabbitmqConfig;

//typedef struct {
//    char *arg_name;
//    char *value;
//}RabbitmqQueueArgument;//队列可选参数
typedef struct {
    char *name;
    int type;//classic quorum stream   Lazy dead-letter
    int passive;//
    int durability;//队列持久化
    int exclusive;//排他队列
    int auto_delete;
    amqp_table_t args;
}RabbitmqQueue;//队列

//typedef struct {
//    char *arg_name;
//    char *value;
//}RabbitmqExchangeArgument;//交换机可选参数
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
#define ARGUMENT_QUEUE_03 "x-single-active-consumer_task"    //单消费者活动模式,当其中一个消费者消费时，其余消费者将阻塞直至该消费者停止消费或断开连接
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
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
void logln(const char *str,FILE *fd);
void log(const char *str,FILE *fd);
void log_die_on_amqp_error(amqp_rpc_reply_t x, char const *context);
void log_die_on_error(int x, char const *context);

int consumer_message_handle(const amqp_envelope_t *envelope);
void *consumer_task(void *);

int producer_prepare_message();
void *producer_task(void *);

char const *hostname="192.168.200.132";
int port=5672;
int status;
//amqp_bytes_t queue_name;
char *queue = "myQueue";
char *exchange = "myExchange";
char *exchangeMode = "topic";

char *bindingKey = "weld.*";//绑定键 用于队列和交换机的绑定
char *routingKey = "weld.upload";//路由键 用于生产者发布消息


void main() {

//    amqp_exchange_declare();
//    amqp_queue_declare()

    //分配连接结构体 和 创建套接字
    connState = amqp_new_connection();
    if(connState != NULL){
        socket = amqp_tcp_socket_new(connState);
    }
    if(!socket){
        logln("creating TCP socket",stderr);
    }
    logln("ok: socket created",stdout);


    //建立连接
    status = amqp_socket_open(
            socket,
            hostname,   //ip
            port        //端口
    );
    if (status!=AMQP_STATUS_OK) {
        die("opening TCP socket");
    }
    logln("ok: tcp socket opened",stdout);

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
        log_die_on_amqp_error(
                reply,
                "Logging in"
        );
    }
    logln("ok: login",stdout);



    //打开通道
    amqp_channel_open(connState,1);


    //队列声明
    {
        //todo zc-指定队列相关参数,eg:ARGUMENT_QUEUE_00
//        amqp_table_t  args;
//        args.entries->key
//        args.entries->value
        amqp_queue_declare_ok_t *r = amqp_queue_declare(
            connState,
            1,//channel
            amqp_cstring_bytes("myQueue"),
            0,//passive 0-队列不存在会自动创建，对了存在检查参数是否匹配，不匹配返回错误
            0,//durable
            0,//exclusive
            0,//auto_delete
            amqp_empty_table//额外参数 (通常用amqp_empty_table)
        );

        log_die_on_amqp_error(amqp_get_rpc_reply(connState), "Declaring queue");

        //分配空间-完全复制原始字节串的内容到新分配的内存中
//        queue_name = amqp_bytes_malloc_dup(r->queue);//注意调用amqp_bytes_free()释放内存空间
//        if(queue_name.bytes==NULL){
//
//        }
    }

    //交换机声明
    amqp_exchange_declare(
        connState,
        1,                       //channel
        amqp_cstring_bytes(exchange),//exchange
        amqp_cstring_bytes(exchangeMode),//type
        0,//passive 检查模式
        0,//durable
        0,//autoDelete
        0,//internal 内部交换机
        amqp_empty_table
    );
    log_die_on_amqp_error(amqp_get_rpc_reply(connState), "declare exchange");


    //绑定交换机
    amqp_queue_bind_ok_t *pT = amqp_queue_bind(
            connState,
            1,                       //channel
            amqp_cstring_bytes(queue),
            amqp_cstring_bytes(exchange),//exchange
            amqp_cstring_bytes(bindingKey),  //bindingkey
            amqp_empty_table
    );
    log_die_on_amqp_error(amqp_get_rpc_reply(connState), "Binding queue");


    //创建任务线程
    {
        pthread_t  producer;
        pthread_t  consumer;

        pthread_create(&producer,NULL,producer_task,NULL);
        pthread_create(&consumer,NULL,consumer_task,NULL);

        pthread_join(producer,NULL);
        pthread_join(consumer,NULL);
    }


    //关闭连接
    amqp_channel_close(connState,1,AMQP_REPLY_SUCCESS);
    amqp_connection_close(connState,AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(connState);
}

void logln(const char *str,FILE *fd){
    char buffer[100]={0};
    strcat(buffer,str);
    buffer[strlen(buffer)]='\n';
    log(buffer,fd);
}
void log(const char *str,FILE *fd){
    pthread_mutex_lock(&log_mutex);
    fputs(str,fd);
    fflush(fd);
    pthread_mutex_unlock(&log_mutex);
}
void log_die_on_amqp_error(amqp_rpc_reply_t r, char const *info){
    pthread_mutex_lock(&log_mutex);
    die_on_amqp_error(r,info);
    pthread_mutex_unlock(&log_mutex);
}
void log_die_on_error(int x, const char *context) {
    pthread_mutex_lock(&log_mutex);
    die_on_error(x,context);
    pthread_mutex_unlock(&log_mutex);
}

int consumer_message_handle(const amqp_envelope_t *envelope){

    //工具函数-显示接收到的数据
    pthread_mutex_lock(&log_mutex);
    amqp_dump(envelope->message.body.bytes, envelope->message.body.len);
    pthread_mutex_unlock(&log_mutex);


    //todo zc-业务处理逻辑


    return 1;
}
void *consumer_task(void *args){
    //启动消息消费
    //告诉 RabbitMQ 服务器开始向客户端推送指定队列中的消息
    amqp_basic_consume(
            connState,
            1,
            amqp_cstring_bytes(queue),
            amqp_cstring_bytes("consumer00"),// 消费者标签(空字节则服务器自动生成)
            1,//no_local 是否接收自己发布的消息
            0,//no_ack 0-手动ACK 1-自动ACK
            0,//exclusive 排他消费
            amqp_empty_table//额外参数 (通常用amqp_empty_table)
    );
    log_die_on_amqp_error(amqp_get_rpc_reply(connState), "Consuming");
    //todo 可以在单个通道上启动多个队列

    //处理消息
    while(1){
        amqp_rpc_reply_t res;//
        amqp_envelope_t envelope;//用于存储消息内容
        amqp_frame_t frame;
        int success=0;


        //用于 内存优化 的关键函数
        //  注：在还未处理完消息内容(如访问envelope.message.body)前调用会导致数据丢失
        amqp_maybe_release_buffers(connState);//，控制着库内部缓冲区的内存释放行为
        //阻塞获取消息
        res = amqp_consume_message(connState, &envelope, NULL, 0);//每次从队列取最新的消息，即使没有被ack
        //todo 处理消息
        if (AMQP_RESPONSE_NORMAL == res.reply_type) {
            success = consumer_message_handle(&envelope);
            //手动ACK处理
            if(success){
                amqp_basic_ack(
                    connState,
                    1,//channel
                    envelope.delivery_tag,//需要被确认消息的标识符
                    0//批量确认
                );

                logln("consumer: manual ack",stdout);
            }
            else{
                amqp_basic_reject(
                    connState,
                    1,//channel
                    envelope.delivery_tag,//需要被拒绝消息的标识符
                    1 //requeue
                );
//                amqp_basic_nack(
//                    connState,
//                    1,//channel
//                    envelope.delivery_tag,//需要被拒绝消息的标识符
//                    1,//multiple 批量拒绝比当前标识小的未确认的消息
//                    1//requeue
//                );
                logln("consumer: reject requeue",stdout);

            }
        }


        //todo 异常处理
        else{
            if (
                AMQP_RESPONSE_LIBRARY_EXCEPTION == res.reply_type       //客户端Rabbitmq-c库内部错误
                && AMQP_STATUS_UNEXPECTED_STATE == res.library_error    //协议状态机异常
            ) {
                //获取帧数据，以判断错误类型
                if (AMQP_STATUS_OK != amqp_simple_wait_frame(connState, &frame)) {
                    return NULL;
                }

                //判断帧类型（method,head,body）
                //1.METHOD帧
                if (AMQP_FRAME_METHOD == frame.frame_type) {
                    frame.payload.method.id;
                    frame.payload.method.decoded;
                    switch (frame.payload.method.id) {
                        //消息未确认
                        case AMQP_BASIC_ACK_METHOD:
                        {
                            /* if we've turned publisher confirms on, and we've published a
                             * message here is a message being confirmed.
                             */
                            //todo 如果生产者开启了消息确认，会收到重复消息进行ack处理
                            logln("consumer: AMQP_BASIC_ACK_METHOD\n",stderr);

//                            amqp_basic_ack(
//                                    connState,
//                                    1,//channel
//                                    envelope.delivery_tag,//需要被确认消息的标识符
//                                    0//批量确认
//                            );


                            break;
                        }

                        //消息退回处理
                        case AMQP_BASIC_RETURN_METHOD:
                        {
                            /* if a published message couldn't be routed and the mandatory
                             * flag was set this is what would be returned. The message then
                             * needs to be read.
                             */
                            amqp_message_t message;
                            res = amqp_read_message(connState, frame.channel, &message, 0);
                            if (AMQP_RESPONSE_NORMAL != res.reply_type) {
                                return NULL;
                            }
                            logln("consumer: AMQP_BASIC_RETURN_METHOD\n",stderr);
                            amqp_destroy_message(&message);
                            break;
                        }
                        //channel已关闭
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
                            logln("consumer: AMQP_CHANNEL_CLOSE_METHOD\n",stderr);
                            //todo 需要重新打开通道
                            amqp_channel_open(
                                connState,
                                1
                            );


                            break;
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
                            logln("consumer: AMQP_CONNECTION_CLOSE_METHOD\n",stderr);
                            //todo 需要重新打开连接

                            break;
                        }

                        default:
                        {
                            fprintf(stderr, "consumer: An unexpected method was received %u\n",
                                    frame.payload.method.id);
                            return NULL;
                        }
                    }
                }
                //2.HEAD帧 内容头
                if(AMQP_FRAME_HEADER == frame.frame_type){

                }
                //3.BODY帧 内容体
                if(AMQP_FRAME_BODY == frame.frame_type){

                }

            }
//            break;
        }
        //释放空间
//        amqp_destroy_message(&envelope.message);
        amqp_destroy_envelope(&envelope);//底层会调用 amqp_destroy_messag()
    }
}


int producer_prepare_message(char *buffer,int size){
    //todo zc-业务处理逻辑
    memset(buffer,0,100);
    log("publish>",stdout);
    fgets(buffer,100,stdin);//会将‘\n’读入

    //处理末尾换行符
    *strstr(buffer,"\n")='\0';
    if(!strcmp(buffer,"exit"))
    {
        return 0;
    }
    return 1;
}
void wait_ack() {
    amqp_publisher_confirm_t result = {};
    struct timeval timeout = {3, 0};

    for (;;) {
        amqp_maybe_release_buffers(connState);

        amqp_rpc_reply_t ret = amqp_publisher_confirm_wait(
                connState,
                &timeout,
//                NULL,
                &result
        );

        //todo 异常处理
        if (AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type) {
            if(AMQP_RESPONSE_SERVER_EXCEPTION == ret.library_error){
                logln("producer: AMQP_RESPONSE_SERVER_EXCEPTION",stderr);
            }
            else if (AMQP_STATUS_UNEXPECTED_STATE == ret.library_error) {
                logln("producer: AMQP_STATUS_UNEXPECTED_STATE",stderr);
                return;
            } else if (AMQP_STATUS_TIMEOUT == ret.library_error) {
                // Timeout means you're done; no publisher confirms were waiting!
                logln("producer: AMQP_STATUS_TIMEOUT",stderr);
                return;
            } else {
                log_die_on_amqp_error(ret, "Waiting for publisher confirmation");
            }
        }

        //结果
        {
            pthread_mutex_lock(&log_mutex);
            switch (result.method) {

                case AMQP_BASIC_ACK_METHOD:
                    //2次ACK,1次rabbitmq服务，1次消费端手动ack
                    fprintf(stderr, "Got an ACK!\n");
                    fprintf(stderr, "Here's the ACK:\n");
                    fprintf(stderr, "\tdelivery_tag: «%" PRIu64 "»\n",
                            result.payload.ack.delivery_tag);
                    fprintf(stderr, "\tmultiple: «%d»\n", result.payload.ack.multiple);
                    break;
                case AMQP_BASIC_NACK_METHOD:
                    fprintf(stderr, "NACK\n");
                    break;
                case AMQP_BASIC_REJECT_METHOD:
                    fprintf(stderr, "REJECT\n");
                    break;
                default:
                    fprintf(stderr, "Unexpected method «%s» is.\n",
                            amqp_method_name(result.method));
                    break;
            };
            pthread_mutex_unlock(&log_mutex);
            return;
        }
    }
}
void *producer_task(void *args){
    int confirmFlag = 1;
    char buffer[100]={0};
    int success = 0;
    amqp_frame_t frame={};


    //启动发布确认
    if(confirmFlag == 1){
        amqp_confirm_select(connState, 1);
        log_die_on_amqp_error(amqp_get_rpc_reply(connState), "Enable confirm-select");
    }


    while(1){
        success = 0;
        
        //准备消息内容
        success = producer_prepare_message(buffer,100);
//        data = amqp_bytes_malloc(strlen(buffer)+1);

        if(success){
            // 设置消息属性
            amqp_basic_properties_t props;
            //注：props._flags 必须包含 AMQP_BASIC_DELIVERY_MODE_FLAG，否则 delivery_mode 会被忽略
//            props._flags = AMQP_BASIC_DELIVERY_MODE_FLAG; // 启用 delivery_mode 属性
//            props.delivery_mode = 1; // 2 = 持久化，1 = 非持久化
            //消息发布
            amqp_basic_publish(
                connState,
                2,//channel
                amqp_cstring_bytes(exchange),   //exchange
                amqp_cstring_bytes(routingKey), //routingKey
                0,//mandatory
                0,//immediate
                &props,//properties
                amqp_cstring_bytes(buffer)//body
            );
            log("producer send:",stdout);
            logln(buffer,stdout);


            if(confirmFlag == 1) {
                //等待ack
                wait_ack();
            }

            

            //异常处理

            //
//            amqp_bytes_free(data);
        }
        else{
            logln("producer: message prepare fail\n",stdout);
        }
    }
}

