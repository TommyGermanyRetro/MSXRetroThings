#include "RP5C01/RP5C01.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

constexpr std::uint32_t ClockHz = 1000;

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void selectBlock(rp5c01::RP5C01& rtc, std::uint8_t block, std::uint8_t flags = 0x08)
{
    rtc.write(13, static_cast<std::uint8_t>(flags | block));
}

bool same(const rp5c01::DateTime& a, const rp5c01::DateTime& b)
{
    return a.year == b.year && a.month == b.month && a.day == b.day && a.weekday == b.weekday && a.hour == b.hour
        && a.minute == b.minute && a.second == b.second;
}

void testRegisterAccess()
{
    rp5c01::RP5C01 rtc;
    require(rtc.read(13) == 0x08, "power-on selects block 0 with the timer enabled");
    selectBlock(rtc, 2);
    rtc.write(3, 0x5A);
    require(rtc.read(3) == 0x0A, "RAM blocks store 4-bit values");
    selectBlock(rtc, 3);
    rtc.write(3, 0x07);
    require(rtc.read(3) == 0x07, "block 3 is a second RAM block");
    selectBlock(rtc, 2);
    require(rtc.read(3) == 0x0A, "the mode register selects the block");
    selectBlock(rtc, 1);
    rtc.write(0, 0x0F);
    rtc.write(5, 0x0F);
    require(rtc.read(0) == 0x00 && rtc.read(5) == 0x03, "unimplemented bits of the alarm block read 0");
    selectBlock(rtc, 0);
    rtc.write(1, 0x0F);
    require(rtc.read(1) == 0x07, "the 10-second counter has three bits");
    require(rtc.read(14) == 0x0F && rtc.read(15) == 0x0F, "the test and reset registers are write-only");

    rtc.write(13, 0x02);
    rtc.reset();
    require(rtc.read(13) == 0x08, "reset selects block 0 with the timer enabled");
    selectBlock(rtc, 2);
    require(rtc.read(3) == 0x0A, "RAM survives a reset");

    const rp5c01::Image image = rtc.image();
    require(image[2 * rp5c01::RegistersPerBlock + 3] == 0x0A && image[3 * rp5c01::RegistersPerBlock + 3] == 0x07,
            "the image holds all four blocks");
    rp5c01::RP5C01 copy;
    rp5c01::Image masked = image;
    masked[rp5c01::RegistersPerBlock + 0] = 0x0F;
    copy.loadImage(masked);
    selectBlock(copy, 3);
    require(copy.read(3) == 0x07, "an image restores the RAM");
    selectBlock(copy, 1);
    require(copy.read(0) == 0x00, "an image is masked like register writes");
}

void testCalendar()
{
    rp5c01::RP5C01 rtc;
    selectBlock(rtc, 1);
    rtc.write(10, 0x01);
    rtc.write(11, 0x03);
    selectBlock(rtc, 0);
    rtc.setDateTime({99, 12, 31, 6, 23, 59, 59});
    selectBlock(rtc, 0);
    require(rtc.read(0) == 9 && rtc.read(1) == 5 && rtc.read(4) == 3 && rtc.read(5) == 2 && rtc.read(12) == 9,
            "the time block holds BCD digits");
    rtc.advance(ClockHz - 1, ClockHz);
    require(rtc.dateTime().second == 59, "a second needs the full clock count");
    rtc.advance(1, ClockHz);
    require(same(rtc.dateTime(), {0, 1, 1, 0, 0, 0, 0}), "the year rolls over from 99 to 00");
    selectBlock(rtc, 1);
    require(rtc.read(11) == 0x00, "the leap year counter counts with the year");

    selectBlock(rtc, 0);
    rtc.setDateTime({0, 2, 28, 1, 23, 59, 59});
    rtc.advance(ClockHz, ClockHz);
    require(rtc.dateTime().month == 2 && rtc.dateTime().day == 29, "February has 29 days when the leap counter is 0");
    rtc.advance(ClockHz * 86400, ClockHz);
    require(rtc.dateTime().month == 3 && rtc.dateTime().day == 1 && rtc.dateTime().weekday == 3,
            "the day after 29 February is 1 March");
    selectBlock(rtc, 1);
    rtc.write(11, 0x01);
    selectBlock(rtc, 0);
    rtc.setDateTime({1, 2, 28, 0, 23, 59, 59});
    rtc.advance(ClockHz, ClockHz);
    require(rtc.dateTime().month == 3 && rtc.dateTime().day == 1, "February has 28 days otherwise");
    rtc.setDateTime({1, 4, 30, 0, 23, 59, 59});
    rtc.advance(ClockHz, ClockHz);
    require(rtc.dateTime().month == 5 && rtc.dateTime().day == 1, "April has 30 days");
}

void testTwelveHourMode()
{
    rp5c01::RP5C01 rtc;
    selectBlock(rtc, 1);
    rtc.write(10, 0x00);
    selectBlock(rtc, 0);
    rtc.setDateTime({20, 6, 10, 2, 11, 59, 59});
    rtc.advance(ClockHz, ClockHz);
    require(rtc.read(4) == 2 && rtc.read(5) == 0x03 && rtc.dateTime().hour == 12, "11:59:59 AM becomes 12 PM");
    rtc.setDateTime({20, 6, 10, 2, 12, 59, 59});
    rtc.advance(ClockHz, ClockHz);
    require(rtc.read(4) == 1 && rtc.read(5) == 0x02 && rtc.dateTime().hour == 13, "12:59:59 PM becomes 1 PM");
    rtc.setDateTime({20, 6, 10, 2, 23, 59, 59});
    rtc.advance(ClockHz, ClockHz);
    require(rtc.read(4) == 2 && rtc.read(5) == 0x01 && rtc.dateTime().day == 11, "11:59:59 PM becomes 12 AM of the next day");
}

void testControl()
{
    rp5c01::RP5C01 rtc;
    selectBlock(rtc, 1);
    rtc.write(10, 0x01);
    rtc.write(2, 0x05);
    selectBlock(rtc, 0);
    rtc.setDateTime({26, 10, 10, 6, 12, 0, 0});
    rtc.write(13, 0x00);
    rtc.advance(ClockHz * 5, ClockHz);
    require(rtc.dateTime().second == 0, "the time stops while the timer is disabled");
    rtc.write(13, 0x08);
    rtc.advance(ClockHz / 2, ClockHz);
    rtc.write(15, 0x02);
    rtc.advance(ClockHz / 2, ClockHz);
    require(rtc.dateTime().second == 0, "reset bit 1 clears the divider below one second");
    rtc.advance(ClockHz / 2, ClockHz);
    require(rtc.dateTime().second == 1, "the divider counts on after the reset");
    rtc.write(15, 0x01);
    selectBlock(rtc, 1);
    require(rtc.read(2) == 0x00 && rtc.read(10) == 0x01, "reset bit 0 clears the alarm registers only");
}

} // namespace

int main()
{
    testRegisterAccess();
    testCalendar();
    testTwelveHourMode();
    testControl();
    std::cout << "RP5C01 library passed.\n";
}
