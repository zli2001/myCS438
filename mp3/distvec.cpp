#include <cstdio>

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fclose(fpOut);

    // You may choose to use std::fstream instead
    // std::ofstream fpOut("output.txt");

    return 0;
}
