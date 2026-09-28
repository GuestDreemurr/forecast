#include <channel/System.h>

void main(void) {
    SystemInit();

    while (true) {
        SystemCalc();
        SystemDraw();
    }
}
