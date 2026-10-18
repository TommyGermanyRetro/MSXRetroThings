#include "Z8530/Z8530.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using z8530::ChannelA;
using z8530::ChannelB;
using z8530::LineSymbol;

bool fail(const char* message)
{
    std::cerr << message << '\n';
    return false;
}

// Register access through the control port: WR0 points to the register (with Point High for 8-15).
void writeReg(z8530::Z8530& scc, int channel, int reg, std::uint8_t value)
{
    if (reg != 0) scc.writeControl(channel, static_cast<std::uint8_t>(reg < 8 ? reg : 0x08 | (reg - 8)));
    scc.writeControl(channel, value);
}

std::uint8_t readReg(z8530::Z8530& scc, int channel, int reg)
{
    if (reg != 0) scc.writeControl(channel, static_cast<std::uint8_t>(reg < 8 ? reg : 0x08 | (reg - 8)));
    return scc.readControl(channel);
}

// Asynchronous x16, one stop bit, 8 bits, baud rate generator on PCLK with time constant 0
// (bit time 64 PCLK cycles), local loopback.
void asyncLoopback(z8530::Z8530& scc, int channel, std::uint8_t wr4 = 0x44, std::uint8_t wr5 = 0x68)
{
    writeReg(scc, channel, 4, wr4);
    writeReg(scc, channel, 3, 0xC1);
    writeReg(scc, channel, 5, wr5);
    writeReg(scc, channel, 11, 0x50);
    writeReg(scc, channel, 12, 0x00);
    writeReg(scc, channel, 13, 0x00);
    writeReg(scc, channel, 14, 0x13);
}

// Register values after a hardware reset (Z85X30 Register Reset Value table), pointer handling,
// the images of the NMOS version and the vector in RR2.
bool resetPass()
{
    z8530::Z8530 scc;
    if (readReg(scc, ChannelA, 0) != 0x44 || readReg(scc, ChannelB, 0) != 0x44) return fail("RR0 after reset must be 44h (Tx empty, Tx underrun).");
    if (readReg(scc, ChannelA, 1) != 0x07) return fail("RR1 after reset must be 07h (residue 011, all sent).");
    if (readReg(scc, ChannelA, 3) != 0x00 || readReg(scc, ChannelA, 10) != 0x00) return fail("RR3 and RR10 must be 0 after reset.");
    if (readReg(scc, ChannelA, 15) != 0xF8) return fail("RR15 after reset must be F8h.");
    writeReg(scc, ChannelA, 12, 0x34);
    writeReg(scc, ChannelA, 13, 0x12);
    if (readReg(scc, ChannelA, 12) != 0x34 || readReg(scc, ChannelA, 13) != 0x12) return fail("RR12/RR13 must return the time constant.");
    if (readReg(scc, ChannelA, 9) != 0x12 || readReg(scc, ChannelA, 11) != 0xF8 || readReg(scc, ChannelA, 14) != 0x00 ||
        readReg(scc, ChannelA, 4) != 0x44 || readReg(scc, ChannelA, 5) != 0x07 || readReg(scc, ChannelB, 7) != 0x00) {
        return fail("RR4-RR7, RR9, RR11 and RR14 must be images of RR0-RR3, RR13, RR15 and RR10.");
    }
    scc.writeControl(ChannelA, 0x0C);
    if (scc.readControl(ChannelA) != 0x34 || scc.readControl(ChannelA) != 0x44) return fail("The pointer must return to 0 after an access.");
    writeReg(scc, ChannelA, 15, 0x05);
    if (readReg(scc, ChannelA, 15) != 0x00) return fail("The unused bits 0 and 2 of RR15 must read 0.");

    writeReg(scc, ChannelB, 2, 0xA0);
    if (readReg(scc, ChannelA, 2) != 0xA0) return fail("RR2 of channel A must return WR2 unmodified.");
    if (readReg(scc, ChannelB, 2) != 0xA6) return fail("RR2 of channel B without interrupt must carry status 011 in V3-V1.");
    writeReg(scc, ChannelA, 9, 0x10);
    if (readReg(scc, ChannelB, 2) != 0xE0) return fail("With Status High the status 110 must sit in V6-V4.");

    // Channel reset A touches channel A only.
    writeReg(scc, ChannelA, 15, 0x00);
    writeReg(scc, ChannelB, 15, 0x00);
    writeReg(scc, ChannelA, 5, 0x82);
    if (!scc.dtr(ChannelA) || !scc.rts(ChannelA)) return fail("WR5 bits 7 and 1 must drive /DTR and /RTS.");
    writeReg(scc, ChannelA, 9, 0x80);
    if (readReg(scc, ChannelA, 15) != 0xF8 || readReg(scc, ChannelB, 15) != 0x00) return fail("Channel reset A must reset channel A only.");
    if (scc.dtr(ChannelA) || scc.rts(ChannelA)) return fail("A channel reset must release the modem outputs.");
    return true;
}

// Asynchronous character through the local loopback with the timing of the baud rate generator.
bool asyncLoopPass()
{
    z8530::Z8530 scc;
    asyncLoopback(scc, ChannelA);
    scc.writeData(ChannelA, 0x55);
    if (readReg(scc, ChannelA, 0) & 0x04) return fail("A written character must fill the transmit buffer.");
    unsigned cycles = 0;
    while ((readReg(scc, ChannelA, 0) & 0x01) == 0 && cycles < 5000) {
        scc.tick();
        ++cycles;
    }
    // 1 start, 8 data, 1 stop bit at 16 x 2 x (0 + 2) PCLK cycles each.
    if (cycles < 640 || cycles > 645) return fail("A 10-bit character must take 640 PCLK cycles at time constant 0, x16.");
    if (!scc.hasTransmitted(ChannelA) || scc.takeTransmitted(ChannelA).value != 0x55) return fail("Local loopback must also drive TxD.");
    if (scc.readData(ChannelA) != 0x55) return fail("The receiver must get the character back.");
    if ((readReg(scc, ChannelA, 0) & 0x05) != 0x04 || (readReg(scc, ChannelA, 1) & 0x01) == 0) {
        return fail("Afterwards the receive FIFO is empty and the transmitter all sent.");
    }

    // Five-or-less coding: 1111000D sends one data bit (start, 1 bit, stop = 3 bit times).
    z8530::Z8530 shortChar;
    asyncLoopback(shortChar, ChannelA, 0x44, 0x08);
    shortChar.writeData(ChannelA, 0xF1);
    cycles = 0;
    while (!shortChar.hasTransmitted(ChannelA) && cycles < 5000) {
        shortChar.tick();
        ++cycles;
    }
    if (cycles < 192 || cycles > 194 || shortChar.takeTransmitted(ChannelA).value != 0x01) {
        return fail("1111000Dh must send one data bit in three bit times.");
    }
    return true;
}

// Receive formats: parity in D7 of a 7-bit character, latched parity error, unused bits as 1,
// framing error, the three-character FIFO with overrun.
bool asyncFormatPass()
{
    z8530::Z8530 scc;
    writeReg(scc, ChannelB, 4, 0x47);
    writeReg(scc, ChannelB, 3, 0x41);
    scc.receive(ChannelB, {LineSymbol::Kind::Data, 0x41, false});
    scc.receive(ChannelB, {LineSymbol::Kind::Data, 0x43, false});
    if ((readReg(scc, ChannelB, 1) & 0x10) != 0 || scc.readData(ChannelB) != 0x41) return fail("41h with even parity 0 must arrive without error.");
    if ((readReg(scc, ChannelB, 1) & 0x10) == 0) return fail("43h with parity bit 0 must report a parity error.");
    if (scc.readData(ChannelB) != 0x43) return fail("The parity bit must be passed in D7.");
    if ((readReg(scc, ChannelB, 1) & 0x10) == 0) return fail("The parity error must stay latched.");
    scc.writeControl(ChannelB, 0x30);
    if ((readReg(scc, ChannelB, 1) & 0x10) != 0) return fail("Error Reset must clear the parity error.");

    writeReg(scc, ChannelB, 4, 0x44);
    writeReg(scc, ChannelB, 3, 0x01);
    scc.receive(ChannelB, {LineSymbol::Kind::Data, 0x15});
    if (scc.readData(ChannelB) != 0xF5) return fail("Unused bits of a 5-bit character must read as 1.");
    scc.receive(ChannelB, {LineSymbol::Kind::Data, 0x15, false, false});
    if ((readReg(scc, ChannelB, 1) & 0x40) == 0) return fail("A missing stop bit must report a framing error.");
    (void)scc.readData(ChannelB);

    writeReg(scc, ChannelB, 3, 0xC1);
    for (std::uint8_t value = 1; value <= 4; ++value) scc.receive(ChannelB, {LineSymbol::Kind::Data, value});
    if (scc.readData(ChannelB) != 1 || scc.readData(ChannelB) != 2) return fail("The FIFO must keep the first characters.");
    if ((readReg(scc, ChannelB, 1) & 0x20) == 0 || scc.readData(ChannelB) != 4) return fail("The fourth character must overwrite the third with overrun.");
    if ((readReg(scc, ChannelB, 0) & 0x01) != 0 || (readReg(scc, ChannelB, 1) & 0x20) == 0) return fail("The overrun must stay latched.");
    scc.writeControl(ChannelB, 0x30);
    if ((readReg(scc, ChannelB, 1) & 0x20) != 0) return fail("Error Reset must clear the overrun.");

    // Break: Break/Abort follows the line, the end leaves one null character.
    scc.receive(ChannelB, {LineSymbol::Kind::BreakStart});
    if ((readReg(scc, ChannelB, 0) & 0x80) == 0) return fail("A break must set Break/Abort.");
    scc.writeControl(ChannelB, 0x10);
    scc.receive(ChannelB, {LineSymbol::Kind::BreakEnd});
    scc.writeControl(ChannelB, 0x10);
    if ((readReg(scc, ChannelB, 0) & 0x81) != 0x01 || scc.readData(ChannelB) != 0x00) return fail("The end of a break must leave a null character.");
    return true;
}

// Interrupts: priority, vector with status low and high, IUS and Reset Highest IUS, the receive
// modes, Tx interrupt and Reset Tx Int Pending, the Ext/Status latches, no vector.
bool interruptPass()
{
    z8530::Z8530 scc;
    writeReg(scc, ChannelA, 2, 0x00);
    writeReg(scc, ChannelA, 9, 0x09);
    writeReg(scc, ChannelA, 4, 0x44);
    writeReg(scc, ChannelA, 3, 0xC1);
    writeReg(scc, ChannelA, 1, 0x10);
    writeReg(scc, ChannelB, 15, 0x20);
    writeReg(scc, ChannelB, 1, 0x01);
    scc.setCts(ChannelB, true);
    scc.receive(ChannelA, {LineSymbol::Kind::Data, 0x22});
    if (!scc.interruptRequest() || readReg(scc, ChannelA, 3) != 0x21) return fail("Rx A and Ext/Status B must be pending.");
    if (readReg(scc, ChannelB, 2) != 0x0C) return fail("RR2B must show Rx A (110) as the highest pending source.");
    if (scc.interruptAcknowledge() != 0x0C) return fail("The acknowledge must return Rx A first.");
    if (scc.interruptRequest()) return fail("The IUS of Rx A must hold off the lower sources.");
    (void)scc.readData(ChannelA);
    if (scc.interruptRequest() || readReg(scc, ChannelA, 3) != 0x01) return fail("Reading the character must clear Rx A pending.");
    scc.writeControl(ChannelA, 0x38);
    if (!scc.interruptRequest() || scc.interruptAcknowledge() != 0x02) return fail("After Reset Highest IUS Ext/Status B (001) must follow.");
    if ((readReg(scc, ChannelB, 0) & 0x20) == 0) return fail("RR0 must show the latched CTS.");
    scc.setCts(ChannelB, false);
    if ((readReg(scc, ChannelB, 0) & 0x20) == 0) return fail("While latched RR0 must keep CTS.");
    scc.writeControl(ChannelB, 0x10);
    scc.writeControl(ChannelB, 0x38);
    if ((readReg(scc, ChannelB, 0) & 0x20) != 0 || !scc.interruptRequest()) return fail("A persisting change must latch again and interrupt.");
    writeReg(scc, ChannelA, 9, 0x19);
    if (scc.interruptAcknowledge() != 0x40) return fail("With Status High the status 001 must sit in V6-V4.");
    scc.writeControl(ChannelB, 0x10);
    scc.writeControl(ChannelB, 0x38);

    // First character mode: one interrupt, again after Enable Int on Next Rx Character; a special
    // condition locks the FIFO until Error Reset.
    writeReg(scc, ChannelA, 9, 0x09);
    writeReg(scc, ChannelA, 1, 0x08);
    scc.receive(ChannelA, {LineSymbol::Kind::Data, 0x31});
    scc.receive(ChannelA, {LineSymbol::Kind::Data, 0x32});
    if ((readReg(scc, ChannelA, 3) & 0x20) == 0 || scc.readData(ChannelA) != 0x31) return fail("The first character must interrupt.");
    if ((readReg(scc, ChannelA, 3) & 0x20) != 0) return fail("Later characters must not interrupt in first character mode.");
    scc.writeControl(ChannelA, 0x20);
    if ((readReg(scc, ChannelA, 3) & 0x20) == 0 || scc.readData(ChannelA) != 0x32) return fail("Enable Int on Next Rx Character must re-arm.");
    scc.receive(ChannelA, {LineSymbol::Kind::Data, 0x33, false, false});
    scc.writeControl(ChannelA, 0x20);
    if (scc.readData(ChannelA) != 0x33 || scc.readData(ChannelA) != 0x33) return fail("A special condition must lock the FIFO.");
    if (scc.interruptAcknowledge() != 0x0E) return fail("The locked character must give the special condition vector (111).");
    scc.writeControl(ChannelA, 0x30);
    scc.writeControl(ChannelA, 0x38);
    if ((readReg(scc, ChannelA, 0) & 0x01) != 0 || scc.interruptRequest()) return fail("Error Reset must unlock and drop the character.");

    // Tx interrupt when the buffer empties, cleared by Reset Tx Int Pending until the next character.
    asyncLoopback(scc, ChannelB);
    writeReg(scc, ChannelB, 1, 0x02);
    scc.writeData(ChannelB, 0x41);
    scc.tick();
    if ((readReg(scc, ChannelA, 3) & 0x02) == 0 || scc.interruptAcknowledge() != 0x00) return fail("The emptied buffer must request Tx B (000).");
    scc.writeControl(ChannelB, 0x28);
    scc.writeControl(ChannelB, 0x38);
    for (int t = 0; t < 1000; ++t) scc.tick();
    if ((readReg(scc, ChannelA, 3) & 0x02) != 0) return fail("Reset Tx Int Pending must hold until the next character.");

    writeReg(scc, ChannelA, 9, 0x0B);
    scc.receive(ChannelB, {LineSymbol::Kind::Data, 0x41});
    writeReg(scc, ChannelB, 1, 0x10);
    if (!scc.interruptRequest() || scc.interruptAcknowledge() != 0xFF) return fail("With NV set the vector must not be driven.");
    writeReg(scc, ChannelA, 9, 0x00);
    if (scc.interruptRequest()) return fail("Without MIE there must be no interrupt.");
    return true;
}

// Zero count of the baud rate generator as Ext/Status source.
bool zeroCountPass()
{
    z8530::Z8530 scc;
    writeReg(scc, ChannelA, 15, 0x02);
    writeReg(scc, ChannelA, 1, 0x01);
    writeReg(scc, ChannelA, 12, 3);
    writeReg(scc, ChannelA, 13, 0);
    writeReg(scc, ChannelA, 14, 0x03);
    int cycles = 0;
    while ((readReg(scc, ChannelA, 3) & 0x08) == 0 && cycles < 100) {
        scc.tick();
        ++cycles;
    }
    if (cycles == 0 || cycles > 5) return fail("The counter must reach zero after time constant + 2 PCLK cycles.");
    return true;
}

// SDLC: channel A sends frames over the line to channel B with address search.
void sdlcSetup(z8530::Z8530& scc)
{
    for (const int channel : {ChannelA, ChannelB}) {
        writeReg(scc, channel, 4, 0x20);
        writeReg(scc, channel, 10, 0x80);
        writeReg(scc, channel, 7, 0x7E);
    }
    writeReg(scc, ChannelA, 11, 0x10);
    writeReg(scc, ChannelA, 12, 0);
    writeReg(scc, ChannelA, 13, 0);
    writeReg(scc, ChannelA, 14, 0x03);
    writeReg(scc, ChannelA, 5, 0x69);
    writeReg(scc, ChannelB, 6, 0x42);
    writeReg(scc, ChannelB, 3, 0xCD);
    // As in the initialisation sequences of the manual: Reset Ext/Status twice opens the latches
    // closed by the mode change.
    for (const int channel : {ChannelA, ChannelB}) {
        scc.writeControl(channel, 0x10);
        scc.writeControl(channel, 0x10);
    }
}

// What channel B received: the bytes read from the FIFO as they arrive (it holds three) and RR1
// read before the last one.
struct Received {
    std::vector<std::uint8_t> bytes;
    std::uint8_t lastStatus = 0;
};

void drain(z8530::Z8530& scc, Received& received)
{
    while ((readReg(scc, ChannelB, 0) & 0x01) != 0) {
        received.lastStatus = readReg(scc, ChannelB, 1);
        received.bytes.push_back(scc.readData(ChannelB));
    }
}

std::vector<z8530::LineSymbol> sendFrame(z8530::Z8530& scc, const std::vector<std::uint8_t>& frame, Received& received)
{
    std::vector<LineSymbol> line;
    std::size_t next = 0;
    // The NMOS SCC does not preset the CRC generator by itself: Reset Tx CRC before each frame.
    scc.writeControl(ChannelA, 0x80);
    for (int t = 0; t < 20000; ++t) {
        if (next < frame.size() && (readReg(scc, ChannelA, 0) & 0x04) != 0) {
            scc.writeData(ChannelA, frame[next]);
            if (next++ == 0) scc.writeControl(ChannelA, 0xC0);
        }
        scc.tick();
        while (scc.hasTransmitted(ChannelA)) {
            line.push_back(scc.takeTransmitted(ChannelA));
            scc.receive(ChannelB, line.back());
            drain(scc, received);
        }
        if (next == frame.size() && line.size() >= frame.size() + 4) break;
    }
    return line;
}

bool sdlcPass()
{
    z8530::Z8530 scc;
    sdlcSetup(scc);
    if ((readReg(scc, ChannelB, 0) & 0x10) == 0) return fail("The enabled receiver must hunt.");
    Received frame;
    const auto line = sendFrame(scc, {0x42, 'H', 'I'}, frame);
    if (line.size() != 7 || line.front().kind != LineSymbol::Kind::Flag || line.back().kind != LineSymbol::Kind::Flag) {
        return fail("The frame must be flag, address, data, two CRC bytes, flag.");
    }
    if ((readReg(scc, ChannelA, 0) & 0x40) == 0) return fail("The underrun must set Tx Underrun/EOM.");
    if ((readReg(scc, ChannelB, 0) & 0x10) != 0) return fail("The opening flag must end the hunt.");
    const auto& bytes = frame.bytes;
    if (bytes.size() != 5 || bytes[0] != 0x42 || bytes[1] != 'H' || bytes[2] != 'I') return fail("Address, data and CRC must reach the FIFO.");
    if ((frame.lastStatus & 0xC0) != 0x80 || (frame.lastStatus & 0x0E) != 0x06) {
        return fail("The last byte must carry end of frame, CRC good, residue 011.");
    }

    // Another address is rejected, the broadcast address FFh accepted.
    scc.writeControl(ChannelB, 0x30);
    Received other;
    (void)sendFrame(scc, {0x55, 0x01}, other);
    if (!other.bytes.empty()) return fail("Address search must reject a frame for another station.");
    Received broadcast;
    (void)sendFrame(scc, {0xFF, 0x01}, broadcast);
    if (broadcast.bytes.size() != 4 || (broadcast.lastStatus & 0xC0) != 0x80) return fail("The broadcast address must be accepted.");

    // A corrupted frame ends with a CRC error.
    scc.writeControl(ChannelB, 0x30);
    Received corrupted;
    for (const auto& symbol : {LineSymbol{LineSymbol::Kind::Flag, 0x7E}, LineSymbol{LineSymbol::Kind::Data, 0x42},
                               LineSymbol{LineSymbol::Kind::Data, 0x01}, LineSymbol{LineSymbol::Kind::Data, 0x02},
                               LineSymbol{LineSymbol::Kind::Data, 0x03}, LineSymbol{LineSymbol::Kind::Flag, 0x7E}}) {
        scc.receive(ChannelB, symbol);
        drain(scc, corrupted);
    }
    if (corrupted.bytes.size() != 4 || (corrupted.lastStatus & 0xC0) != 0xC0) return fail("A wrong CRC must be reported with the end of frame.");

    // An abort interrupts through Break/Abort and returns the receiver to hunt.
    writeReg(scc, ChannelB, 1, 0x01);
    writeReg(scc, ChannelA, 9, 0x08);
    scc.receive(ChannelB, {LineSymbol::Kind::Abort});
    if ((readReg(scc, ChannelA, 3) & 0x01) == 0) return fail("An abort must interrupt through Break/Abort.");
    scc.writeControl(ChannelB, 0x10);
    if ((readReg(scc, ChannelB, 0) & 0x10) == 0) return fail("After an abort the receiver must hunt.");

    // Send Abort empties the buffer and sets Tx Underrun/EOM.
    scc.writeData(ChannelA, 0x42);
    scc.writeControl(ChannelA, 0x18);
    for (int t = 0; t < 100; ++t) scc.tick();
    bool aborted = false;
    while (scc.hasTransmitted(ChannelA)) aborted = aborted || scc.takeTransmitted(ChannelA).kind == LineSymbol::Kind::Abort;
    if (!aborted || (readReg(scc, ChannelA, 0) & 0x44) != 0x44) return fail("Send Abort must send an abort and empty the buffer.");
    return true;
}

// Auto Enables: /CTS enables the transmitter.
bool autoEnablePass()
{
    z8530::Z8530 scc;
    asyncLoopback(scc, ChannelA);
    writeReg(scc, ChannelA, 14, 0x03);
    writeReg(scc, ChannelA, 3, 0xE1);
    scc.writeData(ChannelA, 0x5A);
    for (int t = 0; t < 2000; ++t) scc.tick();
    if (scc.hasTransmitted(ChannelA)) return fail("Without /CTS the transmitter must wait.");
    scc.setCts(ChannelA, true);
    for (int t = 0; t < 2000; ++t) scc.tick();
    if (!scc.hasTransmitted(ChannelA) || scc.takeTransmitted(ChannelA).value != 0x5A) return fail("/CTS must start the transmitter.");
    return true;
}

} // namespace

int main()
{
    if (!resetPass() || !asyncLoopPass() || !asyncFormatPass() || !interruptPass() || !zeroCountPass() ||
        !sdlcPass() || !autoEnablePass()) {
        return EXIT_FAILURE;
    }
    std::cout << "Z8530 passed.\n";
    return EXIT_SUCCESS;
}
