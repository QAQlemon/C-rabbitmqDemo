#include "rabbitmq-c.h"
void main(){

    {
        rabbitmq_start_client();
    }

}
int rabbitmq_init_consumer(int consumer_index){

    //todo 检查索引下标(消费者、连接、通道、队列)
    if(consumer_index<0 || consumer_index>=CONSUMER_MAX_SIZE){
        warn("consumer: index=%d,max=%d",consumer_index,CONSUMER_MAX_SIZE);
        return 0;
    }
    consumersInfo.consumers[consumer_index].index=consumer_index;

    int conn_index = consumersInfo.consumers[consumer_index].conn_index;
    int channel_index = consumersInfo.consumers[consumer_index].channel_index;
    int queue_index = consumersInfo.consumers[consumer_index].queue_index;
    //第一个通道默认不用来处理生产者或消费者任务
    if(channel_index==0){
        warn("consumer[%d]: can't use the first channel,channel_index=%d",consumer_index,channel_index);
        return 0;
    }
    if(rabbitmq_check_channel_index(conn_index,channel_index)==0){
        return 0;
    }
    if(rabbitmq_check_queue(queue_index)==0){
        return 0;
    }
    //检查任务
    if(consumersInfo.consumers[consumer_index].task==NULL){
        warn("consumer[%d]: task is null",consumer_index);
        return 0;
    }
    //消费者参数
    amqp_connection_state_t connState = rabbitmqConnsInfo.conns[conn_index].connState;
    int channel = rabbitmqConnsInfo.conns[conn_index].channelsInfo.channels[channel_index].num;
    amqp_bytes_t queue_name = amqp_cstring_bytes(queuesInfo.queues[queue_index].name);
    amqp_bytes_t consumer_tag = amqp_cstring_bytes(consumersInfo.consumers[consumer_index].consumer_tag);
    int no_local = consumersInfo.consumers[consumer_index].no_local;
    int no_ack = consumersInfo.consumers[consumer_index].no_ack;
    int exclusive = consumersInfo.consumers[consumer_index].exclusive;
    
    
    //todo 退出信息
    consumersInfo.consumers[consumer_index].exitInfo.type=1;//1-消费者
    consumersInfo.consumers[consumer_index].exitInfo.index=consumer_index;
    consumersInfo.consumers[consumer_index].exitInfo.code=0;
    consumersInfo.consumers[consumer_index].exitInfo.info=NULL;
    
    //启动消息消费
    //告诉 RabbitMQ 服务器开始向客户端推送指定队列中的消息
    amqp_basic_consume(
            connState,
            channel,
            queue_name,
            consumer_tag,// 消费者标签(空字节则服务器自动生成)
            no_local,//no_local 是否接收自己发布的消息
            no_ack,//no_ack 0-手动ACK 1-自动ACK
            exclusive,//exclusive 排他消费
            amqp_empty_table//额外参数 (通常用amqp_empty_table)
    );
    return die_on_amqp_error(amqp_get_rpc_reply(connState), "consumer")==1?0:1;
    
}
//int rabbitmq_init_consumers(){
//    for (int i = 0; i < consumersInfo.size; ++i) {
//        if(rabbitmq_init_consumer(i)==0){
//            return 0;
//        }
//    }
//    return 1;
//}
//int rabbitmq_init_producer(int producer_index){
//
//
//}
//int rabbitmq_init_producers(){
//
//}