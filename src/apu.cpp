#include "apu.hh"

#include "cpu.hh"

#include <cstdint>

static const uint8_t LENGTH_TABLE[32] = {10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
                                         12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};

APU::APU(CPU6502& c): m_cpu(c), m_pulse({0, 1}) {}

APU::~APU() {}

void APU::PulseChannel::step() {
  if (_timerCounter == 0) {
    _timerCounter  = _timerReload;
    _sequencerStep = (_sequencerStep + 1) & 0x07;
    return;
  }

  _timerCounter -= 1;
}

uint16_t APU::PulseChannel::sweepPeriod() const {
  uint16_t const periodChange = _timerReload >> _sweepShift;
  if (_sweepNegate) {
    return _timerReload - periodChange - (_id == 0 ? 1 : 0);
  } else {
    return _timerReload + periodChange;
  }
}

void APU::PulseChannel::clockEnvelope() {
  if (_envelopeStart) {
    _envelopeStart   = false;
    _decayLevel      = 15;
    _envelopeDivider = _volume;
  } else if (_envelopeDivider > 0) {
    _envelopeDivider--;
  } else {
    _envelopeDivider = _volume;
    if (_decayLevel > 0) {
      _decayLevel--;
    } else if (_haltCounter) {
      _decayLevel = 15;
    }
  }
}

void APU::PulseChannel::clock() {
  uint16_t const target = sweepPeriod();

  if (_sweepCounter == 0 && _sweepEnabled && _sweepShift > 0 && _timerReload >= 8 && target <= 0x07FF) {
    _timerReload = target;
  }

  if (_sweepCounter == 0 || _sweepReloadFlag) {
    _sweepCounter    = _sweepPeriod;
    _sweepReloadFlag = false;
    return;
  }

  _sweepCounter -= 1;

  if (!_haltCounter && _length > 0) _length--;
}

uint8_t APU::PulseChannel::output() const {
  if (!_enabled || _length == 0 || _timerReload < 8 || sweepPeriod() > 0x7FF) return 0;
  static const uint8_t DUTY_TABLES[4][8] = {
      {0, 1, 0, 0, 0, 0, 0, 0},
      {0, 1, 1, 0, 0, 0, 0, 0},
      {0, 1, 1, 1, 1, 0, 0, 0},
      {1, 0, 0, 1, 1, 1, 1, 1},
  };
  return DUTY_TABLES[_duty][_sequencerStep] * (_constantVolume ? _volume : _decayLevel);
}

void APU::PulseChannel::operation(uint8_t opCode, uint8_t value) {
  if (opCode == 0x00) /* Volume / Duty */ {
    _duty           = (value >> 6) & 0b11;
    _haltCounter    = (value & 0b00100000) > 0;
    _constantVolume = (value & 0b00010000) > 0;
    _volume         = (value & 0b00001111);
  } else if (opCode == 0x01) /* Sweep */ {
    _sweepEnabled = (value & 0b10000000) > 0;
    _sweepPeriod  = (value & 0b01110000) >> 4;
    _sweepNegate  = (value & 0b00001000) > 0;
    _sweepShift   = (value & 0b00000111);
  } else if (opCode == 0x02) /* Timer Low */ {
    _timerReload = (_timerReload & 0x0700) | value;
  } else if (opCode == 0x03) /* Timer High / Length Load */ {
    _timerReload   = (_timerReload & 0x00FF) | ((value & 0b111) << 8);
    _length        = LENGTH_TABLE[value >> 3];
    _envelopeStart = true;
  }
}

void APU::TriangleChannel::step() {
  if (_timerCounter == 0) {
    _timerCounter = _timerReload;

    if (_length > 0 && _linearCounter > 0) _sequencerStep = (_sequencerStep + 1) & 0x1F;
    return;
  }

  _timerCounter -= 1;
}

void APU::TriangleChannel::advanceLength() {
  if (!_haltCounter && _length > 0) _length--;
}

void APU::TriangleChannel::clock() {
  if (_linearReloadFlag) {
    _linearCounter = _counterReload;
  } else if (_linearCounter > 0) {
    _linearCounter -= 1;
  }

  if (!_haltCounter) _linearReloadFlag = false;
}

uint8_t APU::TriangleChannel::output() const {
  if (!_enabled || _timerReload < 2 || _length == 0) return 0;

  static constexpr uint8_t TRIANGLE_SEQUENCE[] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  return TRIANGLE_SEQUENCE[_sequencerStep];
}

void APU::TriangleChannel::operation(uint8_t opCode, uint8_t value) {
  if (opCode == 0x00) /* Linear Counter */ {
    _haltCounter   = (value & 0b10000000) > 0;
    _counterReload = (value & 0b01111111);
  } else if (opCode == 0x01) /* NOP */ {
  } else if (opCode == 0x02) /* Timer Low */ {
    _timerReload = (_timerReload & 0x0700) | value;
  } else if (opCode == 0x03) /* Timer High / Length Load */ {
    _timerReload      = (_timerReload & 0x00FF) | ((value & 0b111) << 8);
    _length           = LENGTH_TABLE[value >> 3];
    _linearReloadFlag = true;
  }
}

void APU::NoiseChannel::step() {
  if (_timerCounter == 0) {
    _timerCounter   = _timerReload;
    uint8_t nextBit = (_shiftReg & 0b1) ^ (_mode ? (_shiftReg >> 6 & 0b1) : (_shiftReg >> 1 & 0b1));
    _shiftReg >>= 1;
    _shiftReg |= (nextBit << 14);
    return;
  }

  if (!_haltCounter) _timerCounter -= 1;
}

void APU::NoiseChannel::advanceLength() {
  if (!_haltCounter && _length > 0) _length--;
}

uint8_t APU::NoiseChannel::output() const {
  if (!_enabled || (_shiftReg & 0b1) == 1 || _length == 0) return 0;
  return _volume;
}

void APU::NoiseChannel::operation(uint8_t opCode, uint8_t value) {
  static uint16_t NTSC_NOISE_TABLE[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068};

  if (opCode == 0x00) /* Volume */ {
    _haltCounter    = (value & 0b00100000) > 0;
    _constantVolume = (value & 0b00010000) > 0;
    _volume         = (value & 0b00001111);
  } else if (opCode == 0x01) /* NOP */ {
  } else if (opCode == 0x02) /* Period / Mode */ {
    _mode        = (value & 0b10000000) > 0;
    _timerReload = NTSC_NOISE_TABLE[value & 0b00001111];
  } else if (opCode == 0x03) /* Length Load */ {
    _length = LENGTH_TABLE[value >> 3];
  }
}

void APU::DeltaModChannel::step() {}

uint8_t APU::DeltaModChannel::output() const {
  if (!_enabled) return 0;
  return 0;
}

void APU::DeltaModChannel::operation(uint8_t opCode, uint8_t value) {
  static const uint16_t DMC_NTSC_TABLE[16] = {428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54};

  if (opCode == 0x00) /* Flags / Rate */ {
    _irqEnable = (value & 0b10000000) > 0;
    _looping   = (value & 0b01000000) > 0;
    _frequency = DMC_NTSC_TABLE[value & 0b00001111];
  } else if (opCode == 0x01) /* Direct Load */ {
    _outputLevel = value & 0b01111111;
  } else if (opCode == 0x02) /* Sample Address */ {
    _sampleAddress = 0xC000 + (value * 64);
  } else if (opCode == 0x03) /* Sample Length */ {
    _sampleLength = (value * 16) + 1;
  }
}

std::optional<uint8_t> APU::handleRead(uint16_t addr) {
  if (addr == 0x4015) /* APU Status */ {
    bool temp  = m_frameIrq;
    m_frameIrq = false;
    return (m_dmc._irqTriggered << 7) | (temp << 6) | (m_dmc._enabled << 4) | ((m_noise._length > 0) << 3) | ((m_triangle._length > 0) << 2) |
           ((m_pulse[1]._length > 0) << 1) | (m_pulse[0]._length > 0);
  }

  return {};
}

bool APU::handleWrite(uint16_t addr, uint8_t value) {
  if (addr >= 0x4000 && addr <= 0x4007) /* Pulse 1 + Pulse 2 */ {
    addr -= 0x4000;
    m_pulse[(addr >> 2) & 0b1].operation(addr & 0b11, value);
    return true;
  } else if (addr >= 0x4008 && addr <= 0x400B) /* Triangle */ {
    addr -= 0x4008;
    m_triangle.operation(addr, value);
    return true;
  } else if (addr >= 0x400C && addr <= 0x400F) /* Noise */ {
    addr -= 0x400C;
    m_noise.operation(addr, value);
    return true;
  } else if (addr >= 0x4010 && addr <= 0x4013) /* DMC */ {
    addr -= 0x4010;
    m_dmc.operation(addr, value);
    return true;
  } else if (addr == 0x4015) /* APU Status */ {
    if ((m_pulse[0]._enabled = value & 0b00000001) == 0) m_pulse[0]._length = 0;
    if ((m_pulse[1]._enabled = value & 0b00000010) == 0) m_pulse[1]._length = 0;
    if ((m_triangle._enabled = value & 0b00000100) == 0) m_triangle._length = 0;
    if ((m_noise._enabled = value & 0b00001000) == 0) m_noise._length = 0;
    if ((m_dmc._enabled = value & 0b00010000) == 0) m_dmc._sampleLength = 0;
    return true;
  } else if (addr == 0x4017) /* Frame Counter */ {
    m_5stepSequence = (value & 0b10000000) > 0;
    m_irqInhibit    = (value & 0b01000000) > 0;
    return true;
  }

  return false;
}

float APU::mixChannels() const {
  double const pulseOut    = 95.88 / ((8128.0 / ((double)m_pulse[0].output() + (double)m_pulse[1].output())) + 100.0);
  double const triangleOut = ((double)m_triangle.output() / 8227.0);
  double const noiseOut    = ((double)m_noise.output() / 12241.0);
  double const dmcOut      = ((double)m_dmc.output() / 22638.0);
  double const apprx       = 1.0 / (triangleOut + noiseOut + dmcOut);
  double const tndOut      = 159.59 / (apprx + 100.0);
  return pulseOut + tndOut;
}

void APU::step(uint8_t cycles) {
  while (cycles--) {
    if (m_cycles % 2 == 0) {
      for (uint8_t i = 0; i < 2; ++i) {
        m_pulse[i].step();
      }
      m_noise.step();
    }
    m_triangle.step();
    m_dmc.step();

    if ((m_cycles++ % (CPU6502::BASE_CLOCK_FREQUENCY / BASE_FCNT_FREQUENCY)) == 0) {
      const auto envelope = [&]() {
        m_triangle.clock();
        for (uint8_t i = 0; i < 2; ++i) {
          m_pulse[i].clockEnvelope();
        }
      };
      const auto length = [&]() {
        m_triangle.advanceLength(), m_noise.advanceLength();
        for (uint8_t i = 0; i < 2; ++i) {
          m_pulse[i].clock();
        }
      };
      const auto irq = [&]() {
        if (m_irqInhibit) return;
        m_frameIrq = true;
        m_cpu.triggerIRQ();
      };
      const auto special = [&]() {
        if (m_5stepSequence) return;
        irq(), length(), envelope();
      };
      switch (m_step++ % (m_5stepSequence ? 5 : 4)) {
        case 0: envelope(); break;
        case 1: length(), envelope(); break;
        case 2: envelope(); break;
        case 3: special(); break;
        case 4: length(), envelope(); break;
      }
    }

    if (m_outEnabled && m_handler) {
      auto const currSample = mixChannels();

      m_sampleAccumulator += currSample;
      m_sampleCount += 1;
      if ((m_cycleAccumulator += 1.0) >= CYCLES_PER_SAMPLE) {
        m_handler(m_sampleAccumulator / m_sampleCount);
        m_sampleAccumulator = 0.0f;
        m_sampleCount       = 0;
        m_cycleAccumulator -= CYCLES_PER_SAMPLE;
      }
    }
  }
}
