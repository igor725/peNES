#pragma once

#include "cpu.hh"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

class APU {
  static constexpr double   CYCLES_PER_SAMPLE   = 37.2869375;
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
    uint16_t _frequency     = 0;
    uint8_t  _outputLevel   = 0;
    uint16_t _sampleLength  = 0;
    uint16_t _sampleAddress = 0;

    void    step();
    uint8_t output() const;
    void    operation(uint8_t opCode, uint8_t value);
    void    advanceLength();
  };

  public:
  using APUHandler = std::function<void(float sample)>;

  APU(CPU6502& c);
  ~APU();

  std::optional<uint8_t> handleRead(uint16_t addr);

  bool handleWrite(uint16_t addr, uint8_t value);

  void step(uint8_t cycles);

  void onData(APUHandler&& handler) { m_handler = std::move(handler); }

  protected:
  float mixChannels() const;

  private:
  CPU6502& m_cpu;

  bool     m_5stepSequence = false;
  bool     m_irqInhibit    = false;
  bool     m_frameIrq      = false;
  uint8_t  m_step          = 0;
  uint16_t m_cycles        = 0;

  double   m_cycleAccumulator  = 0.0;
  float    m_sampleAccumulator = 0.0f;
  uint32_t m_sampleCount       = 0;

  std::array<PulseChannel, 2> m_pulse;
  TriangleChannel             m_triangle;
  NoiseChannel                m_noise;
  DeltaModChannel             m_dmc;
  APUHandler                  m_handler;
};
