#include "RP5C01/RP5C01.hpp"

#include <cstring>
#include <type_traits>

namespace rp5c01 {

namespace {

constexpr std::uint8_t ModeRegister = 13;
constexpr std::uint8_t TestRegister = 14;
constexpr std::uint8_t ResetRegister = 15;
constexpr std::uint8_t TimerEnable = 0x08;
constexpr std::uint8_t ResetAlarm = 0x01;
constexpr std::uint8_t ResetFraction = 0x02;

constexpr std::size_t TimeBlock = 0;
constexpr std::size_t AlarmBlock = 1;

// Time block registers.
constexpr std::size_t Second1 = 0;
constexpr std::size_t Second10 = 1;
constexpr std::size_t Minute1 = 2;
constexpr std::size_t Minute10 = 3;
constexpr std::size_t Hour1 = 4;
constexpr std::size_t Hour10 = 5;
constexpr std::size_t Weekday = 6;
constexpr std::size_t Day1 = 7;
constexpr std::size_t Day10 = 8;
constexpr std::size_t Month1 = 9;
constexpr std::size_t Month10 = 10;
constexpr std::size_t Year1 = 11;
constexpr std::size_t Year10 = 12;

// Alarm block registers.
constexpr std::size_t HourMode = 10;
constexpr std::size_t LeapYear = 11;
constexpr std::uint8_t Hour24 = 0x01;
constexpr std::uint8_t HourPm = 0x02;

// Implemented bits per register, "Address Modes" table of the data book; others read 0.
constexpr std::array<std::array<std::uint8_t, RegistersPerBlock>, BlockCount> Masks = {{
    {0xF, 0x7, 0xF, 0x7, 0xF, 0x3, 0x7, 0xF, 0x3, 0xF, 0x1, 0xF, 0xF},
    {0x0, 0x0, 0xF, 0x7, 0xF, 0x3, 0x7, 0xF, 0x3, 0x0, 0x1, 0x3, 0x0},
    {0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF},
    {0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF},
}};

unsigned daysInMonth(unsigned month, bool leapYear) noexcept
{
    switch (month) {
    case 2:
        return leapYear ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    default:
        return 31;
    }
}

} // namespace

struct RP5C01::Impl {
    std::array<std::array<std::uint8_t, RegistersPerBlock>, BlockCount> registers{};
    std::uint8_t mode = TimerEnable;
    std::uint8_t test = 0;
    std::uint8_t resetControl = 0;
    std::uint64_t fraction = 0;

    std::array<std::uint8_t, RegistersPerBlock>& time() noexcept { return registers[TimeBlock]; }
    [[nodiscard]] const std::array<std::uint8_t, RegistersPerBlock>& time() const noexcept { return registers[TimeBlock]; }

    [[nodiscard]] bool twentyFourHours() const noexcept { return (registers[AlarmBlock][HourMode] & Hour24) != 0; }

    [[nodiscard]] unsigned pair(std::size_t low, std::size_t high) const noexcept
    {
        return time()[high] * 10u + time()[low];
    }

    void setPair(std::size_t low, std::size_t high, unsigned value) noexcept
    {
        time()[low] = static_cast<std::uint8_t>((value % 10) & Masks[TimeBlock][low]);
        time()[high] = static_cast<std::uint8_t>((value / 10) & Masks[TimeBlock][high]);
    }

    // In 12-hour mode the 10-hour register holds the PM flag in D1 and the tens digit in D0.
    [[nodiscard]] unsigned hour24() const noexcept
    {
        if (twentyFourHours()) return pair(Hour1, Hour10);
        const unsigned hour12 = (time()[Hour10] & 0x01) * 10u + time()[Hour1];
        return hour12 % 12 + ((time()[Hour10] & HourPm) ? 12u : 0u);
    }

    void setHour24(unsigned hour) noexcept
    {
        if (twentyFourHours()) {
            setPair(Hour1, Hour10, hour);
            return;
        }
        const unsigned hour12 = hour % 12 == 0 ? 12 : hour % 12;
        time()[Hour1] = static_cast<std::uint8_t>(hour12 % 10);
        time()[Hour10] = static_cast<std::uint8_t>((hour12 / 10) | (hour >= 12 ? HourPm : 0));
    }

    [[nodiscard]] bool leapYear() const noexcept { return (registers[AlarmBlock][LeapYear] & 0x03) == 0; }

    void countSecond() noexcept
    {
        unsigned second = pair(Second1, Second10) + 1;
        if (second < 60) {
            setPair(Second1, Second10, second);
            return;
        }
        setPair(Second1, Second10, 0);
        unsigned minute = pair(Minute1, Minute10) + 1;
        if (minute < 60) {
            setPair(Minute1, Minute10, minute);
            return;
        }
        setPair(Minute1, Minute10, 0);
        const unsigned hour = hour24() + 1;
        if (hour < 24) {
            setHour24(hour);
            return;
        }
        setHour24(0);
        countDay();
    }

    void countDay() noexcept
    {
        time()[Weekday] = static_cast<std::uint8_t>((time()[Weekday] + 1) % 7);
        unsigned month = pair(Month1, Month10);
        const unsigned day = pair(Day1, Day10) + 1;
        if (day <= daysInMonth(month, leapYear())) {
            setPair(Day1, Day10, day);
            return;
        }
        setPair(Day1, Day10, 1);
        if (++month <= 12) {
            setPair(Month1, Month10, month);
            return;
        }
        setPair(Month1, Month10, 1);
        setPair(Year1, Year10, (pair(Year1, Year10) + 1) % 100);
        registers[AlarmBlock][LeapYear] = static_cast<std::uint8_t>((registers[AlarmBlock][LeapYear] + 1) & 0x03);
    }
};

RP5C01::RP5C01()
    : impl(std::make_unique<Impl>())
{
}

RP5C01::~RP5C01() = default;
RP5C01::RP5C01(RP5C01&&) noexcept = default;
RP5C01& RP5C01::operator=(RP5C01&&) noexcept = default;

void RP5C01::reset() noexcept
{
    impl->mode = TimerEnable;
    impl->test = 0;
    impl->resetControl = 0;
}

std::uint8_t RP5C01::read(std::uint8_t address) const noexcept
{
    address &= 0x0F;
    if (address == ModeRegister) return impl->mode;
    // The test and reset registers are write-only.
    if (address > ModeRegister) return 0x0F;
    const std::size_t block = impl->mode & 0x03;
    return static_cast<std::uint8_t>(impl->registers[block][address] & Masks[block][address]);
}

void RP5C01::write(std::uint8_t address, std::uint8_t value) noexcept
{
    address &= 0x0F;
    value &= 0x0F;
    switch (address) {
    case ModeRegister:
        impl->mode = value;
        break;
    case TestRegister:
        impl->test = value;
        break;
    case ResetRegister:
        impl->resetControl = value;
        if (value & ResetAlarm) {
            for (std::size_t index = 2; index <= 8; ++index) impl->registers[AlarmBlock][index] = 0;
        }
        if (value & ResetFraction) impl->fraction = 0;
        break;
    default: {
        const std::size_t block = impl->mode & 0x03;
        impl->registers[block][address] = static_cast<std::uint8_t>(value & Masks[block][address]);
        break;
    }
    }
}

void RP5C01::advance(std::uint32_t elapsed, std::uint32_t clockHz) noexcept
{
    if (clockHz == 0) return;
    impl->fraction += elapsed;
    while (impl->fraction >= clockHz) {
        impl->fraction -= clockHz;
        // With the timer disabled the count stops at the next second.
        if (impl->mode & TimerEnable) impl->countSecond();
    }
}

void RP5C01::setDateTime(const DateTime& value) noexcept
{
    impl->setPair(Second1, Second10, value.second % 60);
    impl->setPair(Minute1, Minute10, value.minute % 60);
    impl->setHour24(value.hour % 24);
    impl->registers[TimeBlock][Weekday] = static_cast<std::uint8_t>(value.weekday % 7);
    impl->setPair(Day1, Day10, value.day);
    impl->setPair(Month1, Month10, value.month);
    impl->setPair(Year1, Year10, value.year % 100);
}

DateTime RP5C01::dateTime() const noexcept
{
    DateTime result;
    result.second = impl->pair(Second1, Second10);
    result.minute = impl->pair(Minute1, Minute10);
    result.hour = impl->hour24();
    result.weekday = impl->registers[TimeBlock][Weekday];
    result.day = impl->pair(Day1, Day10);
    result.month = impl->pair(Month1, Month10);
    result.year = impl->pair(Year1, Year10);
    return result;
}

Image RP5C01::image() const noexcept
{
    Image result{};
    for (std::size_t block = 0; block < BlockCount; ++block) {
        for (std::size_t index = 0; index < RegistersPerBlock; ++index) {
            result[block * RegistersPerBlock + index] = impl->registers[block][index];
        }
    }
    return result;
}

void RP5C01::loadImage(const Image& value) noexcept
{
    for (std::size_t block = 0; block < BlockCount; ++block) {
        for (std::size_t index = 0; index < RegistersPerBlock; ++index) {
            impl->registers[block][index] =
                static_cast<std::uint8_t>(value[block * RegistersPerBlock + index] & Masks[block][index]);
        }
    }
}

std::vector<std::uint8_t> RP5C01::saveState() const
{
    static_assert(std::is_trivially_copyable_v<Impl>, "the RTC state is copied as bytes");
    std::vector<std::uint8_t> state(sizeof(Impl));
    std::memcpy(state.data(), impl.get(), sizeof(Impl));
    return state;
}

bool RP5C01::loadState(const std::vector<std::uint8_t>& state)
{
    if (state.size() != sizeof(Impl)) return false;
    std::memcpy(impl.get(), state.data(), sizeof(Impl));
    return true;
}

} // namespace rp5c01
