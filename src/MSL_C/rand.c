unsigned int next = 1;

unsigned int rand(void) {
    next = (next * 0x41C64E6D) + 0x3039;

    return (next >> 16) & 0x7FFF;
}

unsigned int srand(unsigned int seed) {
    next = seed;
}
