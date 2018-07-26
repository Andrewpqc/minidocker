#include <iostream>
#include <string>
#include <cstdlib>
#include "minidocker.h"


int main(void){
    std::cout<<"container start..."<<std::endl;
    
    //config
    minidocker::container_conf conf {
        host:"minidocker",// 容器主机名
        root_dir:"./rootfs",// 容器内文件系统挂载点
        ip:"192.168.0.100",// 容器IP
        bridge_name:"minidockerbr",// 网桥名,由setup.sh脚本创建，一个系统只运行一次
        bridge_ip:"192.168.0.1"// 网桥IP
    };

    minidocker::container myfirstcontainer(conf);
    myfirstcontainer.run();
    std::cout<<"container exit..."<<std::endl;

}
