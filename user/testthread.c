#include <inc/lib.h>
#include <inc/x86.h>

void func(){
    printf("1\n");
    return;
}

void umain(int argc, char** argv){
    thread_create(func);
    printf("2\n");
    return;

}