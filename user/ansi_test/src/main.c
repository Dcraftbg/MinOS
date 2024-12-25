#include <stdio.h>

int main() {
    printf("\033[31mThis is red text\033[0m\n");
    printf("\033[32mThis is green text\033[0m\n");
    printf("\033[1;34mThis is bold blue text\033[0m\n");
    printf("\033[4;33mThis is underlined yellow text\033[0m\n");
    printf("\033[41;37mThis is text with red background\033[0m\n");
    return 0;
}

