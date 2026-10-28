#ifndef USBSLOT_HH
#define USBSLOT_HH

#include "MSXDevice.hh"
#include "IRQHelper.hh"
#include "Schedulable.hh"

#include <array>
#include <bitset>
#include <cstdint>
#include <map>
#include <memory>

namespace slotlink { struct SyncResult; }

namespace openmsx {

class XMLElement;
struct USBSlotLink;

/** Cartridge slot of the USB slot bridge (Teensy 4.1) as an MSX device.
  *
  * All memory and I/O accesses that reach this device are forwarded over USB to the bridge,
  * which executes them as real Z80 bus cycles on a physical cartridge slot. A heartbeat is sent
  * once per emulated millisecond; without it the bridge switches its bus off. The /INT line of
  * the cartridges is reported back by the bridge and raised as an IRQ. If the bridge switched
  * the cartridges off while the emulation stood still, the machine is reset. A lost connection is
  * reopened at the next reset or power-up of the machine.
  *
  * Memory reads are cached in 256-byte lines: the first read of a line fetches the whole line in
  * one block transfer and hands it to the CPU read cache. A write marks its byte as stale; the
  * next read of that byte is a single bus cycle, the rest of the line stays cached. A reset drops
  * all lines.
  *
  * The cache follows the switching state of the cartridge as described in the extension XML,
  * either directly below the device (no slot expander) or per <subslot id="0..3"> when the
  * cartridge contains its own slot expander (<expander>true</expander>, register FFFFh):
  *   <nocache base=".." size=".."/>         always read from the cartridge (registers, ports)
  *   <nocacheswitch address="0x6001" base="0x4000" size="0x0801"/>
  *                                          read from the cartridge only while bit 0 of the last
  *                                          write to the address is set (SD Mapper: SPI port
  *                                          instead of the Nextor ROM)
  *   <spiblock start="0x4001" end="0x41FF" gap="24"/>
  *                                          when an LDI of this device reads start through the
  *                                          switched port, the LDI behind it in the code are read
  *                                          in one block, gap clocks per byte (bridge protocol 3)
  *   <bank page="1" address="0x6000" mask="0x07"/>
  *                                          a write to the address selects the ROM bank of the page
  *   <mapper port="0xFC" mask="0x1F"/>      memory mapper RAM, segments set by the ports
  *                                          port..port+3, writes go through to the cache
  *   <megaram port="0x8E"/>                 MegaRAM: four 8 KB bank registers selected by A14-A13;
  *                                          OUT to port/port+1 enables bank switching (any write
  *                                          sets the register), IN enables RAM writes
  * The cache works on 8 KB windows, so that every bank register of a MegaRAM has its own image.
  */
class USBSlot final : public MSXDevice
{
public:
	explicit USBSlot(const DeviceConfig& config);
	~USBSlot() override;

	void powerUp(EmuTime time) override;
	void powerDown(EmuTime time) override;
	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	static constexpr unsigned PAGE_SIZE = 0x4000;
	static constexpr unsigned WINDOW_SIZE = 0x2000;
	static constexpr unsigned LINE_SIZE = 256;
	static constexpr unsigned WINDOW_LINES = WINDOW_SIZE / LINE_SIZE;
	static constexpr uint64_t OFF_CHECK_US = 100000;    // half the heartbeat time limit of the bridge

	// Switching rules of one subslot (or of the whole slot without expander)
	struct Rules {
		std::bitset<0x10000 / LINE_SIZE> uncached;
		std::bitset<0x10000 / LINE_SIZE> switchedUncached;
		int switchAddress = -1;
		int spiBlockStart = -1;
		uint16_t spiBlockEnd = 0;
		byte spiBlockGap = 0;
		int bankPage = -1;
		uint16_t bankAddress = 0;
		byte bankMask = 0xFF;
		bool mapper = false;
		uint16_t mapperPort = 0;
		byte mapperMask = 0xFF;
		bool megaram = false;
		uint16_t megaramPort = 0;
	};

	// Cached contents of one 8 KB window image (half of a ROM bank or mapper segment, one MegaRAM
	// bank, or a fixed window)
	struct WindowCache {
		std::array<byte, WINDOW_SIZE> data = {};
		std::bitset<WINDOW_LINES> valid;
		std::bitset<WINDOW_SIZE> stale;
		std::array<uint16_t, WINDOW_LINES> staleCount = {};
	};

	void heartbeat(EmuTime time);
	void checkBridgeOff();
	void resetBridge(EmuTime time);
	void resetState();
	void pollInt();
	void takeIntLevel(const slotlink::SyncResult& sr);
	void linkFailed();
	static void parseRules(const XMLElement& element, Rules& rules);

	[[nodiscard]] unsigned subslotOf(unsigned page) const;
	[[nodiscard]] uint32_t imageKey(unsigned window) const;
	[[nodiscard]] const WindowCache* findImage(unsigned window) const;
	WindowCache& image(unsigned window);
	[[nodiscard]] bool isUncached(uint16_t address) const;
	bool fetchLine(WindowCache& wc, uint16_t address);
	byte readCycle(uint16_t address, EmuTime time);
	void invalidatePage(unsigned page);
	void invalidateWindow(unsigned window);
	void spiBroken();
	int cachedByte(uint16_t address);
	unsigned ldiChainLength(unsigned limit);

	struct SyncRun final : Schedulable {
		explicit SyncRun(USBSlot& owner_, Scheduler& s)
			: Schedulable(s), owner(owner_) {}
		void executeUntil(EmuTime time) override { owner.heartbeat(time); }
		using Schedulable::setSyncPoint;
		using Schedulable::removeSyncPoint;
		USBSlot& owner;
	} syncRun;

	IRQHelper irq;
	std::shared_ptr<USBSlotLink> link;
	unsigned slotNumber = 1;

	bool expander = false;
	std::array<Rules, 4> rules;
	byte expanderReg = 0;
	std::array<byte, 4> bank = {};
	std::array<byte, 4> segment = {0, 1, 2, 3};
	std::array<byte, 4> megaramBank = {0, 1, 2, 3};
	bool megaramWrite = false;
	std::array<bool, 4> switchedOn = {};
	std::vector<byte> spiBuffer;
	size_t spiPosition = 0;
	uint16_t spiNext = 0;
	unsigned long spiBreaks = 0;
	std::map<uint32_t, std::unique_ptr<WindowCache>> images;
};

} // namespace openmsx

#endif
