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

RabbitmqExchange_t exchangeInfo[3]={
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
};//交换机

RabbitmqQueue_t queueInfo[2]={
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
};//队列

RabbitmqBind_t bindsInfo[20]= {
    //连接状态 通道 路由键 队列 交换机
    {
//        .conn_index=0,
//        .channel_index=1,
        .routingKey=ROUTING_KEY_PLC_DATA,
        .exchange_index=0,
        .queue_index=0
    },
    {
//        .conn_index=0,
//        .channel_index=1,
        .routingKey=ROUTING_KEY_FAULT_REPORTS,
        .exchange_index=0,
        .queue_index=1
    },
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
void main(){
    exchangeInfo;

    fputs("ok",stdout);
}