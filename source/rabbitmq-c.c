
#include "amqp.h"
#include <amqp_tcp_socket.h>


#include <stdio.h>
#include <unistd.h>
#include "utils.h"

amqp_connection_state_t connState ;
amqp_socket_t *socket = NULL;





int main() {
    char const *hostname="192.168.200.131";
    int port=5672;
    int status;
    int rate_limit;
    int message_count;

    //分配连接结构体 和 创建套接字
    connState = amqp_new_connection();
    if(connState != NULL){
        socket = amqp_tcp_socket_new(connState);
    }
    if(!socket){
        die("creating TCP socket");
    }

    //建立连接
    status = amqp_socket_open(
            socket,
            hostname,   //ip
            port        //端口
    );
    if (status!=AMQP_STATUS_OK) {
        die("opening TCP socket");
    }

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

    //打开通道
    amqp_channel_open(connState,1);

}


