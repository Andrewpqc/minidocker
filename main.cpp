#include <iostream>
#include <string>
#include <cstdlib>

#include "minidocker.h"
#include "cmdline.h"

int main(int argc, char** argv){
    std::cout << "container start..." << std::endl;

    cmdline::parser cmd;
    cmd.add<std::string>("host", 'h', "host name", true, "");
    cmd.add<int>("port", 'p', "port number", false, 80, cmdline::range(1, 65535));
    cmd.parse_check(argc, argv);
    
    minidocker::container_conf conf{
        host : "minidocker",          // 容器主机名
        root_dir : "./rootfs",        // 容器内文件系统挂载点
        ip : "192.168.0.100",         // 容器IP
        bridge_name : "minidockerbr", // 网桥名,由setup.sh脚本创建，一个系统只运行一次
        bridge_ip : "192.168.0.1"     // 网桥IP
    };

    minidocker::container myfirstcontainer(conf);
    myfirstcontainer.run();

    std::cout  << cmd.get<std::string>("host") << ":"
       << cmd.get<int>("port") << std::endl;
    

    std::cout << "container exit..." << std::endl;
}

