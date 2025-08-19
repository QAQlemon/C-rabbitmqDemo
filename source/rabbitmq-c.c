#include "rabbitmq-c.h"

RabbitmqBind binds[2]={
        //连接状态 通道 路由键 队列 交换机
        {NULL,1,ROUTING_KEY_UPLOAD,{"workQueue"},{"myExchange"}},
        {NULL,1,ROUTING_KEY_DEAD,{"deadQueue"},{"myExchange"}}
};