#include <iostream>
#include <sys/wait.h>  // waitpid
#include <sys/mount.h> // mount
#include <fcntl.h>     // open
#include <unistd.h>    // execv, sethostname, chroot, fchdir
#include <sched.h>     // clone
#include <signal.h>
#include <dirent.h>   // mkdir
#include <stdlib.h>
#include <cstring>    // c_str
#include <fstream>
#include <stdarg.h>
#include <sys/stat.h>
#include <string>      //std::string
#include <net/if.h>    // if_nametoindex
#include <arpa/inet.h> // inet_pton

#include "minidocker.h"
#include "network.h"

namespace minidocker{

//functions outside of the container calss//

std::string &trim(std::string &s){
    if (s.empty()){
        return s;
    }

    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

//member functions of the container calss//

//构造函数，初始化私有的conf变量
minidocker::container::container(minidocker::container_conf &_conf){
    this->conf = _conf;
}

//析构函数，释放veth1,veth2两个网络接口
minidocker::container::~container(){
    lxc_netdev_delete_by_name(this->veth1);
    lxc_netdev_delete_by_name(this->veth2);
    std::cout << "container destoryed" << std::endl;
}

//设置主机名
void minidocker::container::set_hostname(){
    sethostname(this->conf.host.c_str(), this->conf.host.length());
}

//挂载主机的proc,sys目录
void minidocker::container::set_procsys(){
    mount("none", "/proc", "proc", 0, NULL);
    mount("none", "/sys", "sys", 0, NULL);
}

//启动bash
void minidocker::container::start_bash(){
    std::string bash = "/bin/bash";
    char *c_bash = new char[bash.length() + 1]; // +1 用于存放 '\0'
    strcpy(c_bash, bash.c_str());
    char *child_args[] = {c_bash, NULL};
    execv(child_args[0], child_args); // 在子进程中执行 /bin/bash
    std::cout << "free" << std::endl;
    delete[] c_bash; //这里有内存泄露
}

//设置容器的根目录
void minidocker::container::set_rootdir(){
    chdir(this->conf.root_dir.c_str());
    chroot(".");
}

//设置容器内网络
void minidocker::container::set_network(){
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

    // // add  nameserver 114.114.114.114 --> /etc/resolv.conf
    // std::ofstream ofresult("/etc/resolv.conf", std::ios::app);
    // ofresult << "nameserver 8.8.8.8" << std::endl;
    // ofresult.close();
}

//用户映射，将当前用户映射到容器中的root用户
void ::minidocker::container::set_usermap(){
    char uid_path[256];
    char gid_path[256];

    sprintf(uid_path, "/proc/%d/uid_map", getpid());
    sprintf(gid_path, "/proc/%d/gid_map", getpid());

    std::ofstream uid_map(uid_path);
    std::ofstream gid_map(gid_path);

    uid_map << "0 1000 2";
    gid_map << "0 1000 2";

    uid_map.close();
    gid_map.close();
}

// parse user input content for momeory limit
unsigned long long minidocker::container::all2KB(std::string upper_limit){
    std::string upperlimit = trim(upper_limit);
    char last_char = upperlimit[upperlimit.length() - 1];
    if (last_char == 'K' || last_char == 'k'){
        return atoi(upperlimit.c_str());
    }
    else if (last_char == 'M' || last_char == 'm'){
        return atoi(upperlimit.c_str()) * 1024;
    }
    else if (last_char == 'G' || last_char == 'g'){
        return atoi(upperlimit.c_str()) * 1024 * 1024;
    }
    else{
        return 0;
    }
}

//run
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

    auto setup = [](void *args) -> int {
        auto _this = reinterpret_cast<container *>(args);
        _this->set_hostname();
        _this->set_rootdir();
        _this->set_procsys();
        _this->set_network();
        // _this->set_usermap();
        _this->start_bash(); //start_bash放最后

        return 1;
    };
    // int flag = CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWUSER | SIGCHLD;
    int flag = CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWPID | SIGCHLD;
    int child_pid = clone(setup, child_stack + STACK_SIZE, flag, this);
    std::cout << child_pid << std::endl;
    this->child_pid = child_pid;
    if (child_pid == -1){
        std::cout << "clone failed,maybe you should run it with root." << std::endl;
        return;
    }

    // 将 veth2 转移到容器内部, 并命名为 eth0
    lxc_netdev_move_by_name(veth2, child_pid, "eth0");

    waitpid(child_pid, NULL, 0); // 等待子进程的退出
}

void minidocker::container::limit_cpu(float rate){
    char cupDir[128];
    sprintf(cupDir, "/sys/fs/cgroup/cpu/minidocker");
    std::fstream _file;
    _file.open(cupDir, std::ios::in);
    if (!_file && mkdir(cupDir, 0755) == -1){
        std::cout << "create '/sys/fs/cgroup/cpu/minidocker' fail" << std::endl;
        return;
    }else{
        unsigned long weight = static_cast<int32_t>(100000 * rate);

        char cmd1[128];
        char cmd2[128];
        sprintf(cmd1, "echo %u >> /sys/fs/cgroup/cpu/minidocker/tasks", this->child_pid);
        sprintf(cmd2, "echo %lu > /sys/fs/cgroup/cpu/minidocker/cpu.cfs_quota_us", weight);

        system(cmd1); //将本container的子进程放到cgroup
        system(cmd2); //设置cpu占用率
    }
}

void minidocker::container::limit_memory(std::string upper_limit){
    char cupDir[128];
    sprintf(cupDir, "/sys/fs/cgroup/memory/minidocker");
    std::fstream _file;
    _file.open(cupDir, std::ios::in);
    if (!_file && mkdir(cupDir, 0755) == -1){
        std::cout << "create '/sys/fs/cgroup/memory/minidocker' fail" << std::endl;
        return;
    }else{
        unsigned long long total_in_KB = this->all2KB(upper_limit);
        if (total_in_KB == 0){
            std::cout << '"' << upper_limit << '"' << "is invalid." << std::endl;
            return;
        }

        char cmd1[128];
        char cmd2[128];
        sprintf(cmd1, "echo %u >> /sys/fs/cgroup/memory/minidocker/tasks", this->child_pid);
        sprintf(cmd2, "echo %lluB > /sys/fs/cgroup/memory/minidocker/memory.limit_in_bytes", total_in_KB);

        system(cmd1); //将本container的子进程放到cgroup的memory子系统
        system(cmd2); //设置内存上限
    }
}
}
