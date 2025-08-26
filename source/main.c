#include "rabbitmq-c.h"
void main(){

//    {
//        if(rabbitmq_init_client()==0){
//            warn("rabbitmq client: init fail",stderr);
//        }
//        else{
//            warn("rabbitmq client:init success");
//
////            if(rabbitmq_close_conns()==0){
////                warn("rabbitmq client: close conns fail");
////            }
////            else{
////                warn("rabbitmq client: close conns success");
////            }
//        }
//    }

//    {
//        int flag=1;
//        for (int i = 0; i < bindsInfo.size; ++i) {
//            if(rabbitmq_check_bind(i)==0){
//                flag=0;
//                warn("binds[%d]:check error",i);
//                break;
//            }
//        }
////        return flag;
//    }

    {
        rabbitmq_start_client();
    }

}
