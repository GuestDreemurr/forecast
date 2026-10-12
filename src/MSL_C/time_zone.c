// Daylight saving flag for tm_isdst. This platform has no time zone or DST rules, so it is always 0
long __get_isdst(void) {
    return 0;
}
