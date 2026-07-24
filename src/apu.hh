#pragma once

#include "cpu.hh"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

class APU {
  static constexpr double   BASE_APU_FREQUENCY  = 1789773.0;
  static constexpr uint32_t BASE_FCNT_FREQUENCY = 240;

  struct PulseChannel {
    uint8_t  _id             = 0;
    bool     _enabled        = false;
    uint8_t  _duty           = 0;
    uint16_t _timerReload    = 0;
    bool     _haltCounter    = false;
    uint8_t  _length         = 0;
    bool     _constantVolume = false;
    uint8_t  _volume         = 0;

    bool    _sweepEnabled    = false;
    uint8_t _sweepPeriod     = 0;
    uint8_t _sweepShift      = 0;
    bool    _sweepNegate     = false;
    uint8_t _sweepCounter    = 0;
    bool    _sweepReloadFlag = false;

    uint8_t _envelopeDivider = 0;
    uint8_t _decayLevel      = 0;
    bool    _envelopeStart   = false;

    uint16_t _timerCounter  = 0;
    uint8_t  _sequencerStep = 0;

    void     step();
    uint8_t  output() const;
    void     operation(uint8_t opCode, uint8_t value);
    uint16_t sweepPeriod() const;
    void     clockEnvelope();
    void     clock();

    PulseChannel(uint8_t id): _id(id) {}
  };

  struct TriangleChannel {
    bool     _enabled          = false;
    bool     _haltCounter      = false;
    bool     _linearReloadFlag = false;
    uint8_t  _counterReload    = 0;
    uint8_t  _length           = 0;
    uint16_t _timerReload      = 0;

    uint16_t _timerCounter  = 0;
    uint8_t  _sequencerStep = 0;
    uint8_t  _linearCounter = 0;

    void    step();
    uint8_t output() const;
    void    operation(uint8_t opCode, uint8_t value);
    void    advanceLength();
    void    clock();
  };

  struct NoiseChannel {
    bool     _enabled        = false;
    bool     _haltCounter    = false;
    bool     _mode           = false;
    bool     _constantVolume = false;
    uint8_t  _volume         = 0;
    uint8_t  _length         = 0;
    uint16_t _timerReload    = 0;
    uint16_t _shiftReg       = 1;
    uint16_t _timerCounter   = 0;

    void    step();
    uint8_t output() const;
    void    operation(uint8_t opCode, uint8_t value);
    void    advanceLength();
  };

  struct DeltaModChannel {
    bool     _enabled       = false;
    bool     _irqEnable     = false;
    bool     _looping       = false;
    bool     _irqTriggered  = false;
    bool     _bufferEmpty   = true;
    bool     _silenceFlag   = true;
    uint16_t _frequency     = 0;
    uint16_t _sampleLength  = 0;
    uint16_t _sampleAddress = 0;

    uint16_t _timerCounter   = 0;
    uint16_t _currentAddress = 0;
    uint16_t _currentLength  = 0;

    uint8_t _outputLevel   = 0;
    uint8_t _shiftRegister = 0;
    uint8_t _bitsRemaining = 8;
    uint8_t _sampleBuffer  = 0;

    void    step(CPU6502& cpu);
    uint8_t output() const;
    void    operation(uint8_t opCode, uint8_t value);
    void    advanceLength();
  };

  public:
  struct APUState {
    bool     extendedState = false;
    bool     irqInhibit    = false;
    bool     frameIrq      = false;
    bool     outEnabled    = true;
    uint8_t  step          = 0;
    uint16_t cycles        = 0;

    double   cyclesPerSample   = 0.0;
    double   cycleAccumulator  = 0.0;
    float    sampleAccumulator = 0.0f;
    uint32_t sampleCount       = 0;

    std::array<PulseChannel, 2> pulse;
    TriangleChannel             triangle;
    NoiseChannel                noise;
    DeltaModChannel             dmc;
  };

  using APUHandler = std::function<void(std::span<float const> samples)>;

  APU(CPU6502& c);
  ~APU();

  std::optional<uint8_t> handleRead(uint16_t addr);

  bool handleWrite(uint16_t addr, uint8_t value);

  void setOutputEnabled(bool state) { m_state.outEnabled = state; }

  void setSamplingRate(double value) { m_state.cyclesPerSample = BASE_APU_FREQUENCY / value; }

  void step(uint8_t cycles);

  void onData(size_t batchSize, APUHandler&& handler) {
    m_samples.reserve(batchSize);
    m_handler   = std::move(handler);
    m_batchSize = batchSize;
  }

  APUState dumpState() const { return m_state; }

  void restoreState(APUState& state) { m_state = state; }

  protected:
  float mixChannels() const;

  private:
  CPU6502&           m_cpu;
  APUHandler         m_handler;
  APUState           m_state;
  size_t             m_batchSize;
  std::vector<float> m_samples;
};
