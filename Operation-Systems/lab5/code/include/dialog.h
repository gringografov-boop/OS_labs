#pragma once
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using Function = float (*)(float,float);
inline bool ParseFloat(const std::string& s,float& v) {
    errno=0;
    char* end=nullptr;
    v=std::strtof(s.c_str(),&end);
    return end!=s.c_str() && *end=='\0' && errno!=ERANGE && std::isfinite(v);
}
template<class Switch>
int Dialogue(Function& derivative,Function& square,Switch change) {
    std::cout<<std::setprecision(9);
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line);
        std::string cmd,a,b,extra;
        if(!(in>>cmd)) continue;
        if(cmd=="0") {
            if(in>>extra) std::cerr<<"Command 0 takes no arguments\n";
            else change();
            continue;
        }
        if(cmd!="1" && cmd!="2") {
            std::cerr<<"Unknown command\n"; continue;
        }
        float x=0,y=0;
        if(!(in>>a>>b) || (in>>extra) || !ParseFloat(a,x) || !ParseFloat(b,y)) {
            std::cerr<<"Expected two finite real arguments\n"; continue;
        }
        if((cmd=="1" && y==0) || (cmd=="2" && (x<=0 || y<=0))) {
            std::cerr<<"Use nonzero deltaX and positive sides\n"; continue;
        }
        const float result=(cmd=="1" ? derivative : square)(x,y);
        if(!std::isfinite(result)) std::cerr<<"Result outside finite float range\n";
        else std::cout<<result<<std::endl;
    }
    return std::cin.eof() ? 0 : 1;
}
