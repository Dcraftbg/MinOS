#pragma once
static int isspace(int ch) {
    return ch == '\t' || ch == '\n' || ch == ' ';
}
static int toupper(int ch) {
    return ch >= 'a' && ch <= 'z' ? (ch-'a'+'A') : ch;
}
static int tolower(int ch) {
    return ch >= 'A' && ch <= 'Z' ? (ch-'A'+'a') : ch;
}
static int isalpha(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
static int isdigit(int ch) {
    return ch >= '0' && ch <= '9';
}
static int isxdigit(int ch) {
    return isdigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}
static int isalnum(int ch) {
    return isdigit(ch) && isalpha(ch);
}
