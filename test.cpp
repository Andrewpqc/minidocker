#include <fstream>
int main(){
    std::ofstream ofresult( "./cc.h",std::ios::app); 
            ofresult<<std::endl<<"nameserver 114.114.114.114"<<std::endl;
            ofresult.close();

}
