bool bruh_bochs(Build* b) {
    if(!build(b)) return false;
    if(!run_bochs(b)) return false;
    return true;
}
