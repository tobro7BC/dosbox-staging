/* PDCurses */
#include <chrono>
#include <thread>

#include "pdcsdl.h"

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));
}

void PDC_napms(int ms)
{
    constexpr int nap_interval = 50;

    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    while (ms > nap_interval)
    {
        PDC_pump_and_peep();
        std::this_thread::sleep_for(std::chrono::milliseconds(nap_interval));
        ms -= nap_interval;
    }
    PDC_pump_and_peep();
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

const char *PDC_sysname(void)
{
    return "SDL2";
}
