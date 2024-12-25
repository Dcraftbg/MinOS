#include <stdio.h>
#include <string.h>

void print_holiday(const char* msg) {
    size_t msg_len = strlen(msg);
    for(size_t i = 0; i < msg_len; ++i) {
        const char* color = "\033[0m";
        if(i % 3 == 0)       color = "\033[92m";
        else if (i % 3 == 1) color = "\033[91m";
        printf("%s%c\033[0m", color, msg[i]);
    }
    printf("\n");
}
#define RESET        "\033[0m"
#define BRIGHT_GREEN "\033[92m"
#define DARK_BLUE    "\033[36m"
#define LIGHT_BLUE   "\033[96m"
#define PURPLE       "\033[35m"
#define ORANGE       "\033[33m"
int main() {
    printf(LIGHT_BLUE"#include"RESET" "BRIGHT_GREEN"<stdio.h>"RESET"\n");
    printf(
        "%s",
        DARK_BLUE "void"RESET" print_holiday(" DARK_BLUE "const char" RESET "* msg) {" "\n"
        "    "DARK_BLUE "size_t"RESET" msg_len = strlen(msg);" "\n"
        "    "PURPLE "for"RESET"("DARK_BLUE"size_t"RESET" i = "ORANGE"0"RESET"; i < msg_len; ++i) {" "\n"
        "        "DARK_BLUE"const char"RESET"* color = "BRIGHT_GREEN"\""DARK_BLUE"\\033"BRIGHT_GREEN"[0m\""RESET";" "\n"
        "        "PURPLE"if"RESET"(i % "ORANGE"3"RESET" == "ORANGE"0"RESET")       color = "BRIGHT_GREEN"\""DARK_BLUE"\\033"BRIGHT_GREEN"[92m\""RESET";" "\n"
        "        "PURPLE"else"RESET" "PURPLE"if"RESET" (i % "ORANGE"3"RESET" == "ORANGE"1"RESET") color = "BRIGHT_GREEN"\""DARK_BLUE"\\033"BRIGHT_GREEN"[91m\""RESET";" "\n"
        "        "RESET"printf("BRIGHT_GREEN"\""DARK_BLUE"%s%c\\033"BRIGHT_GREEN"[0m\""RESET", color, msg[i]);""\n"
        "    }""\n"
        "    printf("BRIGHT_GREEN"\""DARK_BLUE"\\n"BRIGHT_GREEN"\""RESET");" "\n"
        "}""\n" 
    );


    printf("\033[31mThis is red text\033[0m\n");
    printf("\033[32mThis is green text\033[0m\n");
    printf("Hey :D\n");
    printf("\033[41;37mThis is text with red background\033[0m\n");
    print_holiday("Merry Christmas!!");
    printf("Blocks:\n");
    for(size_t i = 0; i < 8; ++i) {
        printf("\033[%zum   \033[0m", 40+i);
    }
    printf("\n");
    printf("Blocks (Bright):\n");
    for(size_t i = 0; i < 8; ++i) {
        printf("\033[%zum   \033[0m", 100+i);
    }
    printf("\n");
    return 0;

}

