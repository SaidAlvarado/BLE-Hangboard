#include "board.h"
#include "periph/gpio.h"
#include "xtimer.h"

#define LED GPIO_PIN(0, 13)
#define INTERVAL (500 * 1000U)

int main(void) {
    gpio_init(LED, GPIO_OUT);

    while (1) {
        gpio_toggle(LED);
        xtimer_usleep(INTERVAL);
        gpio_toggle(LED);
        xtimer_usleep(INTERVAL);
    }

    return 0;
}
