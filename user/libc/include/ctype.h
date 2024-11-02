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
