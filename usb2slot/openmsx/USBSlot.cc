#include "USBSlot.hh"

#include "CliComm.hh"
#include "CommandController.hh"
#include "DeviceConfig.hh"
#include "MSXCliComm.hh"
#include "EmuDuration.hh"
#include "CPURegs.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "Timer.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "strCat.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "slotlink.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace openmsx {

// One connection per COM port, shared by the USBSlot devices of slot 1 and slot 2
struct USBSlotLink {
	slotlink::Link link;
	std::string port;
	bool failed = false;                // connection lost; reopened at the next reset or power-up
	EmuTime lastConnect = EmuTime::zero();
	bool connectTried = false;
	EmuTime lastRun = EmuTime::zero();
	uint64_t lastRunReal = 0;           // real time of the last heartbeat (us)
	EmuTime lastReset = EmuTime::zero();
	bool resetDone = false;
	bool intLow = false;                // /INT is one wired-OR line for the whole bridge
	const USBSlot* irqOwner = nullptr;  // the device that raises it as an IRQ
	uint8_t protocol = 2;               // 3: spaced block reads (MRDNG)
};

static std::mutex registryMutex;
static std::map<std::string, std::weak_ptr<USBSlotLink>> registry;

// Opens the COM port, switches the bridge to the binary protocol, checks its version and starts the
// heartbeat; an empty result means success
static std::string connect(USBSlotLink& l)
{
	if (!l.link.open(l.port)) {
		return strCat("cannot open ", l.port, ": ", l.link.error());
	}
	if (!l.link.enterBinary()) {
		return strCat("no answer from the bridge on ", l.port, ": ", l.link.error());
	}
	uint8_t version = 0;
	uint16_t maxBlock = 0;
	if (!l.link.hello(version, maxBlock) || version < 2 || version > 3) {
		return strCat("unsupported bridge protocol version ", static_cast<int>(version));
	}
	l.protocol = version;
	l.link.run();
	l.lastRunReal = Timer::getTime();
	l.failed = false;
	l.resetDone = false;
	return {};
}

static std::shared_ptr<USBSlotLink> acquireLink(const std::string& port)
{
	std::lock_guard<std::mutex> lock(registryMutex);

	if (auto it = registry.find(port); it != registry.end()) {
		if (auto existing = it->second.lock()) {
			return existing;
		}
	}

	auto created = std::make_shared<USBSlotLink>();
	created->port = port;
	if (auto error = connect(*created); !error.empty()) {
		throw MSXException("USBSlot: ", error);
	}
	registry[port] = created;
	return created;
}
USBSlot::USBSlot(const DeviceConfig& config)
	: MSXDevice(config)
	, syncRun(*this, getScheduler())
	, irq(getMotherBoard(), "USBSlot.IRQ")
{
	std::string port(getDeviceConfig().getChildData("port", "COM16"));
	slotNumber = static_cast<unsigned>(getDeviceConfig().getChildDataAsInt("slot", 1));
	if (slotNumber != 1 && slotNumber != 2) {
		throw MSXException("USBSlot: <slot> must be 1 or 2");
	}
	expander = getDeviceConfig().getChildDataAsBool("expander", false);
	parseRules(getDeviceConfig(), rules[0]);
	for (const auto* sub : getDeviceConfig().getChildren("subslot")) {
		int id = sub->getAttributeValueAsInt("id", -1);
		if (!expander || id < 0 || id > 3) {
			throw MSXException("USBSlot: <subslot id=\"0..3\"> needs <expander>true</expander>");
		}
		parseRules(*sub, rules[id]);
	}
	link = acquireLink(port);
	if (!link->irqOwner) {
		link->irqOwner = this;
	}
}

USBSlot::~USBSlot()
{
	syncRun.removeSyncPoint();
	if (link && link->irqOwner == this) {
		link->irqOwner = nullptr;
	}
}

void USBSlot::parseRules(const XMLElement& element, Rules& r)
{
	for (const auto* n : element.getChildren("nocache")) {
		unsigned base = n->getAttributeValueAsInt("base", 0);
		unsigned size = n->getAttributeValueAsInt("size", 0);
		if (size == 0 || base >= 0x10000 || base + size > 0x10000) {
			throw MSXException("USBSlot: <nocache> must lie in [0x0000,0x10000)");
		}
		for (unsigned line = base / LINE_SIZE; line <= (base + size - 1) / LINE_SIZE; ++line) {
			r.uncached.set(line);
		}
	}
	if (const auto* n = element.findChild("nocacheswitch")) {
		unsigned base = n->getAttributeValueAsInt("base", 0);
		unsigned size = n->getAttributeValueAsInt("size", 0);
		if (size == 0 || base >= 0x10000 || base + size > 0x10000) {
			throw MSXException("USBSlot: <nocacheswitch> must lie in [0x0000,0x10000)");
		}
		r.switchAddress = n->getAttributeValueAsInt("address", -1);
		if (r.switchAddress < 0 || r.switchAddress > 0xFFFF) {
			throw MSXException("USBSlot: <nocacheswitch address=\"0x0000..0xFFFF\">");
		}
		for (unsigned line = base / LINE_SIZE; line <= (base + size - 1) / LINE_SIZE; ++line) {
			r.switchedUncached.set(line);
		}
	}
	if (const auto* s = element.findChild("spiblock")) {
		r.spiBlockStart = s->getAttributeValueAsInt("start", -1);
		r.spiBlockEnd = static_cast<uint16_t>(s->getAttributeValueAsInt("end", 0));
		r.spiBlockGap = static_cast<byte>(s->getAttributeValueAsInt("gap", 0));
		if (r.spiBlockStart < 0 || r.spiBlockStart > r.spiBlockEnd || r.spiBlockGap == 0) {
			throw MSXException("USBSlot: <spiblock start=\"..\" end=\"..\" gap=\"..\"> with start <= end and gap > 0");
		}
	}
	if (const auto* b = element.findChild("bank")) {
		r.bankPage = b->getAttributeValueAsInt("page", -1);
		r.bankAddress = static_cast<uint16_t>(b->getAttributeValueAsInt("address", 0));
		r.bankMask = static_cast<byte>(b->getAttributeValueAsInt("mask", 0xFF));
		if (r.bankPage < 0 || r.bankPage > 3) {
			throw MSXException("USBSlot: <bank page=\"0..3\">");
		}
	}
	if (const auto* m = element.findChild("mapper")) {
		r.mapper = true;
		r.mapperPort = static_cast<uint16_t>(m->getAttributeValueAsInt("port", 0xFC) & 0xFC);
		r.mapperMask = static_cast<byte>(m->getAttributeValueAsInt("mask", 0xFF));
	}
	if (const auto* m = element.findChild("megaram")) {
		r.megaram = true;
		r.megaramPort = static_cast<uint16_t>(m->getAttributeValueAsInt("port", 0x8E) & 0xFE);
	}
}

void USBSlot::linkFailed()
{
	if (!link->failed) {
		link->failed = true;
		getCliComm().printWarning("USBSlot: connection to the bridge lost: ", link->link.error(),
		                          "; reconnecting at the next reset");
	}
}

void USBSlot::pollInt()
{
	uint8_t level;
	while (link->link.popInt(level)) {
		link->intLow = (level == 0);
	}
	if (link->irqOwner == this) {
		irq.set(link->intLow);
	}
}

// /INT level from a SYNC answer; changes reported before it are older and are dropped
void USBSlot::takeIntLevel(const slotlink::SyncResult& sr)
{
	uint8_t older;
	while (link->link.popInt(older)) {
	}
	link->intLow = (sr.intLevel == 0);
}

void USBSlot::heartbeat(EmuTime time)
{
	if (!link->failed) {
		if (link->lastRun + EmuDuration::msec(1) <= time) {
			checkBridgeOff();
			link->link.run();
			link->lastRun = time;
			link->lastRunReal = Timer::getTime();
		}
		link->link.poll();
		pollInt();
	}
	syncRun.setSyncPoint(time + EmuDuration::msec(1));
}

// While the emulation stands still (pause, debugger) no heartbeat is sent. After its time limit the
// bridge switches the cartridges off like a powered-down MSX and keeps /RESET LOW; they have lost
// their state then, so the machine is reset as after switching it on again. The reset runs as a
// command between two CPU instructions, like a reset from the console.
void USBSlot::checkBridgeOff()
{
	if (Timer::getTime() - link->lastRunReal < OFF_CHECK_US) {
		return;
	}
	slotlink::SyncResult sr;
	if (!link->link.sync(sr)) {
		linkFailed();
		return;
	}
	takeIntLevel(sr);
	if (!sr.running) {
		getCliComm().printWarning("USBSlot: the bridge switched the cartridges off while the emulation "
		                          "stood still; resetting the machine");
		try {
			getCommandController().executeCommand("after realtime 0 reset");
		} catch (MSXException& e) {
			getCliComm().printWarning("USBSlot: machine reset failed: ", e.getMessage());
		}
	}
}

unsigned USBSlot::subslotOf(unsigned page) const
{
	return expander ? (expanderReg >> (2 * page)) & 3 : 0;
}

// Key of the window image (8 KB) that the cartridge currently shows in the window: a mapper
// segment and a MegaRAM bank are the same memory wherever they appear, a ROM bank belongs to its
// page, everything else is fixed
uint32_t USBSlot::imageKey(unsigned window) const
{
	const unsigned page = window / 2;
	const unsigned half = window & 1;
	const unsigned sub = subslotOf(page);
	const Rules& r = rules[sub];
	if (r.mapper) {
		return (sub << 24) | (1u << 20) | (segment[page] << 1) | half;
	}
	if (r.megaram) {
		return (sub << 24) | (3u << 20) | megaramBank[window & 3];
	}
	if (r.bankPage == static_cast<int>(page)) {
		return (sub << 24) | (2u << 20) | (page << 16) | (bank[sub] << 1) | half;
	}
	return (sub << 24) | (window << 16);
}

const USBSlot::WindowCache* USBSlot::findImage(unsigned window) const
{
	auto it = images.find(imageKey(window));
	return (it != images.end()) ? it->second.get() : nullptr;
}

USBSlot::WindowCache& USBSlot::image(unsigned window)
{
	auto& wc = images[imageKey(window)];
	if (!wc) {
		wc = std::make_unique<WindowCache>();
	}
	return *wc;
}

bool USBSlot::isUncached(uint16_t address) const
{
	unsigned sub = subslotOf(address / PAGE_SIZE);
	unsigned line = address / LINE_SIZE;
	return rules[sub].uncached.test(line) || (switchedOn[sub] && rules[sub].switchedUncached.test(line));
}

void USBSlot::invalidatePage(unsigned page)
{
	invalidateDeviceRCache(page * PAGE_SIZE, PAGE_SIZE);
}

void USBSlot::invalidateWindow(unsigned window)
{
	invalidateDeviceRCache(window * WINDOW_SIZE, WINDOW_SIZE);
}

bool USBSlot::fetchLine(WindowCache& wc, uint16_t address)
{
	std::vector<uint8_t> block;
	uint16_t waits = 0;
	uint8_t status = 0;
	const uint16_t start = address & 0xFF00;
	if (!link->link.memReadBlock(static_cast<uint8_t>(slotNumber), start, LINE_SIZE, block, waits, status)) {
		linkFailed();
		return false;
	}
	if (status != slotlink::StOk || block.size() != LINE_SIZE) {
		return false;
	}
	const unsigned offset = start % WINDOW_SIZE;
	const unsigned line = offset / LINE_SIZE;
	std::copy(block.begin(), block.end(), wc.data.begin() + offset);
	for (unsigned a = offset; a < offset + LINE_SIZE; ++a) {
		wc.stale.reset(a);
	}
	wc.staleCount[line] = 0;
	wc.valid.set(line);
	// The line with the expander register is never handed to the CPU cache: FFFFh is answered here
	if (!(expander && start == 0xFF00)) {
		fillDeviceRCache(start, LINE_SIZE, &wc.data[offset]);
	}
	return true;
}

void USBSlot::resetState()
{
	invalidateDeviceRCache();
	images.clear();
	expanderReg = 0;
	bank.fill(0);
	segment = {0, 1, 2, 3};
	megaramBank = {0, 1, 2, 3};
	megaramWrite = false;
	switchedOn.fill(false);
	spiBuffer.clear();
}

// The sector read was broken off: the bridge has clocked bytes the CPU will not read
void USBSlot::spiBroken()
{
	spiBuffer.clear();
	if (spiBreaks++ == 0) {
		getCliComm().printWarning("USBSlot: a block read of the cartridge port was broken off");
	}
}

// Byte of a cacheable area through the cache (fetching its line), -1 when it cannot be read that way
int USBSlot::cachedByte(uint16_t address)
{
	if (isUncached(address) || (expander && address == 0xFFFF)) {
		return -1;
	}
	WindowCache& wc = image(address / WINDOW_SIZE);
	unsigned offset = address % WINDOW_SIZE;
	if (!wc.valid.test(offset / LINE_SIZE) && !fetchLine(wc, address)) {
		return -1;
	}
	return wc.stale.test(offset) ? -1 : wc.data[offset];
}

// During the memory access of an LDI the PC of openMSX still points to its second opcode byte (A0h). The
// instruction that reads now is an LDI of this device when ED A0 stands at PC-1 in a page shown by this device;
// every further ED A0 directly behind it reads the next byte. Anything else gives 1: only this byte.
unsigned USBSlot::ldiChainLength(unsigned limit)
{
	uint16_t pc = getCPU().getRegisters().getPC();
	if (getCPUInterface().getVisibleMSXDevice(((pc - 1) & 0xFFFF) / PAGE_SIZE) != this ||
	    cachedByte(pc - 1) != 0xED || cachedByte(pc) != 0xA0) {
		return 1;
	}
	pc += 1;
	unsigned count = 1;
	while (count < limit && getCPUInterface().getVisibleMSXDevice(((pc + 1) & 0xFFFF) / PAGE_SIZE) == this &&
	       cachedByte(pc) == 0xED && cachedByte(pc + 1) == 0xA0) {
		++count;
		pc += 2;
	}
	return count;
}

const byte* USBSlot::getReadCacheLine(uint16_t start) const
{
	if ((expander && start == 0xFF00) || isUncached(start)) {
		return nullptr;
	}
	const WindowCache* wc = findImage(start / WINDOW_SIZE);
	const unsigned offset = start % WINDOW_SIZE;
	const unsigned line = offset / LINE_SIZE;
	return (wc && wc->valid.test(line) && wc->staleCount[line] == 0) ? &wc->data[offset] : nullptr;
}

void USBSlot::powerUp(EmuTime time)
{
	// openMSX calls only powerUp() when the machine is switched on, not reset(); a device has to
	// reset its hardware itself, otherwise the cartridge stays in reset (the bridge starts with /RESET LOW)
	resetState();
	resetBridge(time);
	syncRun.setSyncPoint(time + EmuDuration::msec(1));
}

void USBSlot::powerDown(EmuTime /*time*/)
{
	syncRun.removeSyncPoint();
	irq.reset();
}

void USBSlot::reset(EmuTime time)
{
	resetState();
	resetBridge(time);
}

void USBSlot::resetBridge(EmuTime time)
{
	// After a lost connection (bridge unplugged or restarted) the port is reopened once per reset;
	// all devices on the link are reset at the same emulated time
	if (link->failed && (!link->connectTried || link->lastConnect != time)) {
		link->connectTried = true;
		link->lastConnect = time;
		if (auto error = connect(*link); error.empty()) {
			getCliComm().printInfo("USBSlot: connection to the bridge on ", link->port, " restored");
		} else {
			getCliComm().printWarning("USBSlot: ", error);
		}
	}
	if (link->failed) {
		return;
	}
	// Both USBSlot devices are reset at the same emulated time; one /RESET pulse is enough
	if (!link->resetDone || link->lastReset != time) {
		// The bridge may have switched itself off while the machine was loading (no heartbeat
		// for more than its time limit); it rejects RESET in that state, so wake it first.
		link->link.run();
		link->lastRun = time;
		link->lastRunReal = Timer::getTime();
		uint8_t status = 0;
		if (!link->link.reset(status)) {
			linkFailed();
			return;
		}
		link->lastReset = time;
		link->resetDone = true;
		// The bridge reports only changes of /INT; take the current level after the reset
		slotlink::SyncResult sr;
		if (link->link.sync(sr)) {
			takeIntLevel(sr);
		}
	}
	pollInt();
}

byte USBSlot::readMem(uint16_t address, EmuTime time)
{
	if (link->failed) {
		return 0xFF;
	}
	if (expander && address == 0xFFFF) {
		return static_cast<byte>(~expanderReg);
	}
	if (isUncached(address)) {
		if (!spiBuffer.empty()) {
			if (address == spiNext && !getCPU().isM1Cycle(address)) {
				byte value = spiBuffer[spiPosition++];
				++spiNext;
				if (spiPosition == spiBuffer.size()) {
					spiBuffer.clear();
				}
				return value;
			}
			spiBroken();
		}
		unsigned sub = subslotOf(address / PAGE_SIZE);
		const Rules& r = rules[sub];
		if (switchedOn[sub] && r.spiBlockStart == address && link->protocol >= 3 &&
		    !getCPU().isM1Cycle(address)) {
			unsigned count = ldiChainLength(r.spiBlockEnd - address + 1);
			if (count > 1) {
				std::vector<uint8_t> block;
				uint16_t waits = 0;
				uint8_t status = 0;
				if (!link->link.memReadBlock(static_cast<uint8_t>(slotNumber), address,
				                             static_cast<uint16_t>(count), block, waits, status,
				                             r.spiBlockGap)) {
					linkFailed();
					return 0xFF;
				}
				if (status == slotlink::StOk && block.size() == count) {
					spiBuffer = std::move(block);
					spiPosition = 1;
					spiNext = address + 1;
					pollInt();
					return spiBuffer[0];
				}
			}
		}
		return readCycle(address, time);
	}
	WindowCache& wc = image(address / WINDOW_SIZE);
	const unsigned offset = address % WINDOW_SIZE;
	const unsigned line = offset / LINE_SIZE;
	if (wc.valid.test(line)) {
		if (wc.stale.test(offset)) {
			byte value = readCycle(address, time);
			wc.data[offset] = value;
			wc.stale.reset(offset);
			if (--wc.staleCount[line] == 0 && !(expander && (address & 0xFF00) == 0xFF00)) {
				fillDeviceRCache(address & 0xFF00, LINE_SIZE, &wc.data[offset & ~(LINE_SIZE - 1)]);
			}
			return value;
		}
		return wc.data[offset];
	}
	if (fetchLine(wc, address)) {
		return wc.data[offset];
	}
	return readCycle(address, time);
}

byte USBSlot::readCycle(uint16_t address, EmuTime time)
{
	if (link->failed) {
		return 0xFF;
	}
	slotlink::ReadResult r;
	const bool m1 = getCPU().isM1Cycle(address);
	if (!link->link.memRead(static_cast<uint8_t>(slotNumber), address, m1, r)) {
		linkFailed();
		return 0xFF;
	}
	if (r.waits) {
		(void)getCPU().waitCyclesZ80(time, r.waits);
	}
	pollInt();
	return r.data;
}

void USBSlot::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	if (!link->failed) {
		link->link.memWrite(static_cast<uint8_t>(slotNumber), address, value);
	}
	if (expander && address == 0xFFFF) {
		const byte changed = expanderReg ^ value;
		expanderReg = value;
		for (unsigned page = 0; page < 4; ++page) {
			if ((changed >> (2 * page)) & 3) {
				invalidatePage(page);
			}
		}
		return;
	}
	const unsigned page = address / PAGE_SIZE;
	const unsigned sub = subslotOf(page);
	const Rules& r = rules[sub];
	if (r.bankPage >= 0 && address == r.bankAddress) {
		bank[sub] = value & r.bankMask;
		if (subslotOf(r.bankPage) == sub) {
			invalidatePage(r.bankPage);
		}
		return;
	}
	if (!spiBuffer.empty() && (isUncached(address) || (r.switchAddress >= 0 && address == r.switchAddress))) {
		spiBroken();
	}
	if (r.switchAddress >= 0 && address == r.switchAddress) {
		bool on = (value & 1) != 0;
		if (on && !switchedOn[sub]) {
			// The switched lines now belong to the cartridge port: the CPU must not read them from
			// its cache any more
			for (unsigned line = 0; line < 0x10000 / LINE_SIZE; ++line) {
				if (r.switchedUncached.test(line) && subslotOf(line * LINE_SIZE / PAGE_SIZE) == sub) {
					invalidateDeviceRCache(line * LINE_SIZE, LINE_SIZE);
				}
			}
		}
		switchedOn[sub] = on;
		return;
	}
	// A write to a line read from the cartridge (registers, the SPI port) leaves the cache alone
	if (isUncached(address)) {
		return;
	}
	if (r.megaram && !megaramWrite) {
		// Bank switching mode: every write sets the bank register selected by A14-A13
		const unsigned reg = (address >> 13) & 3;
		megaramBank[reg] = value & 0x3F;
		for (unsigned window = reg; window < 8; window += 4) {
			if (subslotOf(window / 2) == sub) {
				invalidateWindow(window);
			}
		}
		return;
	}
	const unsigned window = address / WINDOW_SIZE;
	auto it = images.find(imageKey(window));
	if (it == images.end()) {
		return;
	}
	WindowCache& wc = *it->second;
	const unsigned offset = address % WINDOW_SIZE;
	const unsigned line = offset / LINE_SIZE;
	if (!wc.valid.test(line)) {
		return;
	}
	if (r.mapper || r.megaram) {
		// RAM: the cartridge now holds the written value
		wc.data[offset] = value;
		if (wc.stale.test(offset)) {
			wc.stale.reset(offset);
			if (--wc.staleCount[line] == 0) {
				fillDeviceRCache(address & 0xFF00, LINE_SIZE, &wc.data[offset & ~(LINE_SIZE - 1)]);
			}
		}
	} else if (!wc.stale.test(offset)) {
		wc.stale.set(offset);
		if (wc.staleCount[line]++ == 0) {
			invalidateDeviceRCache(address & 0xFF00, LINE_SIZE);
		}
	}
}

byte USBSlot::peekMem(uint16_t address, EmuTime /*time*/) const
{
	// Reading a real cartridge can have side effects, so the debugger only sees cached bytes
	if (expander && address == 0xFFFF) {
		return static_cast<byte>(~expanderReg);
	}
	const WindowCache* wc = findImage(address / WINDOW_SIZE);
	const unsigned offset = address % WINDOW_SIZE;
	return (wc && wc->valid.test(offset / LINE_SIZE) && !wc->stale.test(offset)) ? wc->data[offset] : 0xFF;
}

byte USBSlot::readIO(uint16_t port, EmuTime time)
{
	if (link->failed) {
		return 0xFF;
	}
	slotlink::ReadResult r;
	if (!link->link.ioRead(static_cast<uint8_t>(port & 0xFF), r)) {
		linkFailed();
		return 0xFF;
	}
	if (r.waits) {
		(void)getCPU().waitCyclesZ80(time, r.waits);
	}
	pollInt();
	// MegaRAM: reading its port enables RAM writes (bank registers keep their values)
	for (const auto& rule : rules) {
		if (rule.megaram && (port & 0xFE) == rule.megaramPort) {
			megaramWrite = true;
			break;
		}
	}
	return r.data;
}

void USBSlot::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (!link->failed) {
		// Sent at once and answered: collected I/O writes reach the cartridge faster than a Z80
		// issues them (chips like the PCF8584 on the RCX I2C card then drop commands), and the
		// answer carries the wait cycles a cartridge inserts with /WAIT
		uint8_t status = 0;
		uint32_t waits = 0;
		if (!link->link.ioWriteWait(static_cast<uint8_t>(port & 0xFF), value, status, waits)) {
			linkFailed();
		} else if (waits) {
			(void)getCPU().waitCyclesZ80(time, waits);
		}
		pollInt();
	}
	for (unsigned sub = 0; sub < 4; ++sub) {
		const Rules& r = rules[sub];
		if (r.megaram && (port & 0xFE) == r.megaramPort) {
			// MegaRAM: writing its port enables bank switching
			megaramWrite = false;
			break;
		}
		if (r.mapper && (port & 0xFC) == r.mapperPort) {
			const unsigned page = port & 3;
			segment[page] = value & r.mapperMask;
			if (subslotOf(page) == sub) {
				invalidatePage(page);
			}
			break;
		}
	}
}

byte USBSlot::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return 0xFF;
}

template<typename Archive>
void USBSlot::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(USBSlot);
REGISTER_MSXDEVICE(USBSlot, "USBSlot");

} // namespace openmsx
