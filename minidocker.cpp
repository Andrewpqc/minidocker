#include <iostream>
#include <sys/wait.h>  // waitpid
#include <sys/mount.h> // mount
#include <fcntl.h>     // open
#include <unistd.h>    // execv, sethostname, chroot, fchdir
#include <sched.h>     // clone
#include <signal.h>
#include <cstring>
#include <fstream> 
#include <string> //std::string
#include <net/if.h>     // if_nametoindex
#include <arpa/inet.h>  // inet_pton

#include "minidocker.h"
#include "network.h"


namespace minidocker{

    minidocker::container::container(minidocker::container_conf &_conf){
        this->conf=_conf;
    }

    minidocker::container::~container(){
        lxc_netdev_delete_by_name(this->veth1);
        lxc_netdev_delete_by_name(this->veth2);
        std::cout<<"container destoryed"<<std::endl;
    }

    void minidocker::container::set_hostname(){
        sethostname(this->conf.host.c_str(), this->conf.host.length());
    }

    void minidocker::container::set_procsys(){
        mount("none", "/proc", "proc", 0, NULL);
        mount("none", "/sys", "sys", 0, NULL);
    }

    void minidocker::container::start_bash(){
        std::string bash = "/bin/bash";
        char *c_bash = new char[bash.length() + 1]; // +1 用于存放 '\0'
        strcpy(c_bash, bash.c_str());
        char *child_args[] = {c_bash, NULL};
        execv(child_args[0], child_args); // 在子进程中执行 /bin/bash
        std::cout<<"free"<<std::endl;
        delete[] c_bash;  //这里有内存泄露                      
    }

    void minidocker::container::set_rootdir(){
        chdir(this->conf.root_dir.c_str());
        chroot(".");
    }

    void minidocker::container::set_network() {
        int ifindex = if_nametoindex("eth0");
        struct in_addr ipv4;
        struct in_addr bcast;
        struct in_addr gateway;

        // IP 地址转换函数，将 IP 地址在点分十进制和二进制之间进行转换
        inet_pton(AF_INET, this->conf.ip.c_str(), &ipv4);
        inet_pton(AF_INET, "255.255.255.0", &bcast);
        inet_pton(AF_INET, this->conf.bridge_ip.c_str(), &gateway);

        // 配置 eth0 IP 地址
        lxc_ipv4_addr_add(ifindex, &ipv4, &bcast, 16);

        // 激活 lo
        lxc_netdev_up("lo");

        // 激活 eth0
        lxc_netdev_up("eth0");

        // 设置网关
        lxc_ipv4_gateway_add(ifindex, &gateway);

        // 设置 eth0 的 MAC 地址
        char mac[18];
        new_hwaddr(mac);
        setup_hw_addr(mac, "eth0");


        // add  nameserver 114.114.114.114 --> /etc/resolv.conf
        std::ofstream ofresult( "/etc/resolv.conf",std::ios::app); 
        ofresult<<std::endl<<"nameserver 114.114.114.114"<<std::endl;
        ofresult.close();
    }

    void::minidocker::container::set_usermap(){
        char uid_path[256];
        char gid_path[256];

        sprintf(uid_path,"/proc/%d/uid_map",getpid());
        sprintf(gid_path,"/proc/%d/gid_map",getpid());
        std::ofstream uid_map(uid_path);
        std::ofstream gid_map(gid_path);

        uid_map<<"0 1000 2";
        gid_map<<"0 1000 2";
       

        uid_map.close();
        gid_map.close();
        
    }


    void minidocker::container::run(){
        char veth1buf[IFNAMSIZ] = "andrewpqc0X";
        char veth2buf[IFNAMSIZ] = "andrewpqc0X";
        // 创建一对网络设备, 一个用于加载到宿主机, 另一个用于转移到子进程容器中
        veth1 = lxc_mkifname(veth1buf); // lxc_mkifname 这个 API 在网络设备名字后面至少需要添加一个 "X" 来支持随机创建虚拟网络设备
        veth2 = lxc_mkifname(veth2buf); // 用于保证网络设备的正确创建, 详见 network.c 中对 lxc_mkifname 的实现
        lxc_veth_create(veth1, veth2);

        // 设置 veth1 的 MAC 地址
        setup_private_host_hw_addr(veth1);

        // 将 veth1 添加到网桥中
        lxc_bridge_attach(this->conf.bridge_name.c_str(), veth1);

        // 激活 veth1
        lxc_netdev_up(veth1);


        auto setup= [](void * args)->int{
            auto _this = reinterpret_cast<container*>(args);
            _this->set_hostname();
            _this->set_rootdir();
            _this->set_procsys();
            _this->set_network();
            _this->set_usermap();                        
            _this->start_bash(); //start_bash放最后    
                   
            return 1;
        };
        int flag=CLONE_NEWUTS|CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWNET|CLONE_NEWUSER|SIGCHLD;
        int child_pid = clone(setup, child_stack + STACK_SIZE,flag,this);
        std::cout<<child_pid<<std::endl;
        if (child_pid == -1){
            std::cout << "clone failed,maybe you should run it with root." << std::endl;
        }

         // 将 veth2 转移到容器内部, 并命名为 eth0
        lxc_netdev_move_by_name(veth2, child_pid, "eth0");

        waitpid(child_pid, NULL, 0); // 等待子进程的退出
    }

    void minidocker::container::limit_cpu(){

    }

    void minidocker::container::limit_memory(){

    }

    
    


}
