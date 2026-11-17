// Offline renderer for VC-Clone calibration: drives the Virus A/B/C device (Osirus) as a black box.
// Started once (boots the firmware from the user's ROM), then reads one job per stdin line:
//   R <single.syx> <out.wav> <hostRate> <lengthSec> <settleSec> <events>
//   events: comma separated  n:note:start:dur:vel  |  c:time:cc:value   ("-" = none)
// Each job: all-sound-off + 2 s of silence (tails die), single dump to the edit buffer, settle,
// then the events. Output: 32-bit float stereo WAV of lengthSec, latency removed.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "synthLib/plugin.h"
#include "virusLib/device.h"
#include "virusLib/romloader.h"

using namespace synthLib;

static std::vector<uint8_t> readFile(const std::string& p)
{
	std::ifstream f(p, std::ios::binary);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
	std::ofstream f(path, std::ios::binary);
	const uint32_t frames = (uint32_t)L.size(), dataBytes = frames * 8;
	auto u32 = [&](uint32_t v){ f.write((const char*)&v, 4); };
	auto u16 = [&](uint16_t v){ f.write((const char*)&v, 2); };
	f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
	f.write("fmt ", 4); u32(16); u16(3); u16(2); u32(sr); u32(sr * 8); u16(8); u16(32);
	f.write("data", 4); u32(dataBytes);
	for (uint32_t i = 0; i < frames; ++i) { f.write((const char*)&L[i], 4); f.write((const char*)&R[i], 4); }
}

struct Ev { double t; std::vector<uint8_t> bytes; };

int main(int argc, char** argv)
{
	if (argc < 2) { std::fprintf(stderr, "usage: osirusRender <rom.bin>\n"); return 2; }
	// argv[1]: a folder that holds the ROM (only that file)
	auto roms = virusLib::ROMLoader::findROMs(std::string(argv[1]), virusLib::DeviceModel::ABC);
	if (roms.empty() || !roms.front().isValid()) { std::fprintf(stderr, "no valid Virus A/B/C ROM in %s\n", argv[1]); return 1; }
	const auto& rom = roms.front();

	DeviceCreateParams params;
	params.romName = rom.getFilename();
	params.romData = rom.getRomFileData();
	params.customData = static_cast<uint32_t>(rom.getModel());

	virusLib::Device device(params);
	Plugin plugin(&device, {});
	float curRate = 0;
	const uint32_t block = 64;
	plugin.setBlockSize(block);

	std::vector<float> inL(block, 0.f), inR(block, 0.f), outBuf[12];
	for (auto& b : outBuf) b.assign(block, 0.f);
	TAudioInputs ins{}; ins[0] = inL.data(); ins[1] = inR.data(); ins[2] = inL.data(); ins[3] = inR.data();
	TAudioOutputs outs{}; for (int i = 0; i < 12; ++i) outs[i] = outBuf[i].data();

	std::printf("READY devicerate=%f\n", device.getSamplerate()); std::fflush(stdout);

	std::string line;
	while (std::getline(std::cin, line))
	{
		std::istringstream is(line);
		std::string cmd; is >> cmd;
		if (cmd == "Q") break;
		if (cmd != "R") continue;
		std::string syx, out, events; int sr; double len, settle;
		is >> syx >> out >> sr >> len >> settle >> events;

		if (curRate != (float)sr) { plugin.setHostSamplerate((float)sr, 0.0f); curRate = (float)sr; }
		const uint32_t lat = plugin.getLatencyMidiToOutput();

		auto run = [&](size_t samples, std::vector<Ev> evs, std::vector<float>* L, std::vector<float>* R, double t0)
		{
			std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b){ return a.t < b.t; });
			size_t e = 0, pos = 0;
			while (pos < samples)
			{
				const size_t n = std::min<size_t>(block, samples - pos);
				while (e < evs.size() && (size_t)std::llround((evs[e].t - t0) * sr) < pos + n)
				{
					const auto off = (uint32_t)std::max<long long>(0, std::llround((evs[e].t - t0) * sr) - (long long)pos);
					SMidiEvent ev(MidiEventSource::Host);
					ev.offset = off;
					if (evs[e].bytes[0] == 0xF0) ev.sysex.assign(evs[e].bytes.begin(), evs[e].bytes.end());
					else { ev.a = evs[e].bytes[0]; ev.b = evs[e].bytes[1]; ev.c = evs[e].bytes.size() > 2 ? evs[e].bytes[2] : 0; }
					plugin.addMidiEvent(ev);
					++e;
				}
				plugin.process(ins, outs, n, 120.0f, 0.0, false, false);
				std::vector<SMidiEvent> midiOut; plugin.getMidiOut(midiOut);
				if (L) { L->insert(L->end(), outBuf[0].begin(), outBuf[0].begin() + n); R->insert(R->end(), outBuf[1].begin(), outBuf[1].begin() + n); }
				pos += n;
			}
		};

		// 1) silence everything and let tails decay
		run((size_t)(2.0 * sr), { {0.0, {0xB0, 120, 0}}, {0.0, {0xB0, 123, 0}} }, nullptr, nullptr, 0.0);
		// 2) patch to the single edit buffer, settle
		auto dump = readFile(syx);
		if (dump.size() >= 267) { dump[7] = 0x00; dump[8] = 0x40; unsigned s = 0; for (size_t i = 5; i < 265; ++i) s += dump[i]; dump[265] = s & 0x7F; }
		run((size_t)(settle * sr), { {0.0, dump} }, nullptr, nullptr, 0.0);
		// 3) the events, plus latency
		std::vector<Ev> evs;
		if (events != "-")
		{
			std::stringstream ss(events); std::string item;
			while (std::getline(ss, item, ','))
			{
				std::vector<std::string> f; std::stringstream is2(item); std::string tok;
				while (std::getline(is2, tok, ':')) f.push_back(tok);
				if (f[0] == "n" && f.size() >= 5)
				{
					const int note = std::stoi(f[1]), vel = std::stoi(f[4]);
					const double st = std::stod(f[2]), du = std::stod(f[3]);
					evs.push_back({ st, { 0x90, (uint8_t)note, (uint8_t)vel } });
					evs.push_back({ st + du, { 0x80, (uint8_t)note, 0 } });
				}
				else if (f[0] == "c" && f.size() >= 4)
					evs.push_back({ std::stod(f[1]), { 0xB0, (uint8_t)std::stoi(f[2]), (uint8_t)std::stoi(f[3]) } });
			}
		}
		std::vector<float> L, R;
		run((size_t)(len * sr) + lat, evs, &L, &R, 0.0);
		L.erase(L.begin(), L.begin() + std::min<size_t>(lat, L.size()));
		R.erase(R.begin(), R.begin() + std::min<size_t>(lat, R.size()));
		L.resize((size_t)(len * sr)); R.resize((size_t)(len * sr));
		writeWav(out, L, R, sr);
		std::printf("OK %s latency=%u\n", out.c_str(), lat); std::fflush(stdout);
	}
	return 0;
}
