#ifndef MINIDOCKER_H
#define MINIDOCKER_H

#include <string>
#define STACK_SIZE 512 * 512

namespace minidocker{


typedef struct{
    std::string host;//主机名
    std::string root_dir;//根目录
    std::string ip;// 容器 IP
    std::string bridge_name;// 网桥名
    std::string bridge_ip;// 网桥 IP
} container_conf;

class container{
  private:
    container_conf conf; //配置变量
    char child_stack[STACK_SIZE];//子进程栈
    char *veth1;
    char *veth2;
    
    void set_hostname();
    void set_procsys();
    void start_bash();
    void set_rootdir();
    void set_network();
    void set_usermap();

  public:
    container(container_conf &_conf);
    ~container();
    void run();
    void limit_cpu();
    void limit_memory();
};

}

#endif