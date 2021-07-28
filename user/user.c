#include <unistd.h>

int main(int argc, char** argv){

    char s[] = "ciao";
    syscall(134,1,2);
    syscall(174,1,2, s, sizeof(s));
    syscall(182,1,2, s, sizeof(s));
    syscall(183,1,2);

}

