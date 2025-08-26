#include "rabbitmq-c.h"
void main(){

//    {
//        rabbitmq_init_conn(0);
//
//        //todo 连接和通道
//        amqp_connection_state_t connState = rabbitmqConnsInfo.conns[0].connState;
//        int channel_num = rabbitmqConnsInfo.conns[0].channelsInfo.channels[0].num;
//
//        //todo 队列基本参数
//        RabbitmqQueueEntity_t queue = queuesInfo.queues[0];
//        amqp_bytes_t queue_name = amqp_cstring_bytes(queue.name);
//
//        //todo 解析拓展参数
//        amqp_table_entry_t kvs[9];
//        amqp_table_t xargs;
//        xargs.num_entries=3;
//        xargs.entries=kvs;
//
////        xargs.entries[0].key = amqp_cstring_bytes(queue.args[0].key);
//        xargs.entries[0].key = amqp_cstring_bytes(ARGUMENT_QUEUE_01);
//        xargs.entries[0].value.kind = AMQP_FIELD_KIND_I32;
//        xargs.entries[0].value.value.i32 = queue.args[0].value.integer;
//
////        xargs.entries[1].key = amqp_cstring_bytes(queue.args[1].key);
//        xargs.entries[1].key = amqp_cstring_bytes(ARGUMENT_QUEUE_04);
//        xargs.entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
////        xargs.entries[1].value.value.bytes = amqp_cstring_bytes(queue.args[1].value.str);
//        xargs.entries[1].value.value.bytes = amqp_cstring_bytes("deadLetterExchange");
//
////        xargs.entries[2].key = amqp_cstring_bytes(queue.args[2].key);
//        xargs.entries[2].key = amqp_cstring_bytes(ARGUMENT_QUEUE_05);
//        xargs.entries[2].value.kind = AMQP_FIELD_KIND_UTF8;
////        xargs.entries[2].value.value.bytes = amqp_cstring_bytes(queue.args[1].value.str);
//        xargs.entries[2].value.value.bytes = amqp_cstring_bytes("deadRoutingKeyExample");
//
//        //todo 检测
//        amqp_queue_declare_ok_t *r = amqp_queue_declare(
//                connState,//连接对象
//                channel_num,//channel
//                queue_name,//队列名称
//                queue.passive,//passive 0-队列不存在会自动创建，对了存在检查参数是否匹配，不匹配返回错误
//                queue.durability,//durable     队列元数据持久化，不保证数据不丢失
//                queue.exclusive,//exclusive
//                queue.auto_delete,//auto_delete 无消费者在自动删除
//                xargs//额外参数 (通常用amqp_empty_table)
//        );
//        if(die_on_amqp_error(amqp_get_rpc_reply(connState),"check")){
//            info("fail");
//        }else{
//            info("ok");
//        }
//    }

    {
        if(rabbitmq_init_client()==0){
            warn("rabbitmq client: init fail",stderr);
        }
        else{
            warn("rabbitmq client:init success");

            if(rabbitmq_close_conns()==0){
                warn("rabbitmq client: close conns fail");
            }
            else{
                warn("rabbitmq client: close conns success");
            }
        }
    }

}