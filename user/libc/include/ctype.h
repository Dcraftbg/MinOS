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
// Taken from this table:
// https://en.cppreference.com/w/c/string/byte
static int iscntrl(int ch) {
    return (ch >= 0 && ch <= 31) || ch == 127;
}
static int isgraph(int ch) {
    return ch >= 33 && ch <= 126;
}
static int islower(int ch) {
    return ch >= 'a' && ch <= 'z';
}
static int isupper(int ch) {
    return ch >= 'A' && ch <= 'Z';
}
static int isprint(int ch) {
    return ch >= 32 && ch <= 126;
}
static int ispunct(int ch) {
    return (ch >= 33  && ch <= 47 ) ||
           (ch >= 58  && ch <= 64 ) ||
           (ch >= 91  && ch <= 96 ) ||
           (ch >= 123 && ch <= 126);
}

