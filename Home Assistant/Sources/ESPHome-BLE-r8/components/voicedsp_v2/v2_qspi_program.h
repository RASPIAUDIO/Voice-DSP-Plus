#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::voicedsp_v2 {
// Transport/clock injection keeps the exact wire protocol testable without GPIO.
// The caller must stop audio, acquire the bus and load the matching RAM writer.
enum class QspiPhase { VALIDATING, WAITING, ERASING, WRITING, VERIFYING, VERIFIED };
enum class QspiResult { OK, ARGUMENT, REUSED, CANCELLED, IMAGE_CRC, IDENTITY, WRITER_ERROR, TIMEOUT };

inline uint32_t qspi_u32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline void qspi_put32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i));
}
inline uint32_t qspi_crc(uint32_t crc, const uint8_t *p, size_t size) {
  while (size--) {
    crc ^= *p++;
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}

template<class IO> class QspiProgram {
 public:
  explicit QspiProgram(IO &io) : io_(io) {}
  QspiResult run(const uint8_t *image, size_t size, uint32_t crc) {
    if (used_) return QspiResult::REUSED;
    used_ = true;
    if (!image || size < 4096 || size > SETTINGS_BEGIN || size % 4096) return QspiResult::ARGUMENT;
    expected_crc_ = crc;
    io_.progress(QspiPhase::VALIDATING, 0, size);
    uint32_t computed = 0xffffffffu;
    for (size_t a = 0; a < size; a += 4096) {
      if (io_.cancelled()) return QspiResult::CANCELLED;
      computed = qspi_crc(computed, image + a, 4096);
      io_.wait(1);
    }
    if ((computed ^ 0xffffffffu) != crc) return QspiResult::IMAGE_CRC;
    io_.progress(QspiPhase::WAITING, 0, size);
    auto result = poll(1, 0, 10000);
    if (result != QspiResult::OK) return result;
    uint8_t payload[128]{};
    qspi_put32(payload, crc);
    io_.progress(QspiPhase::ERASING, 0, SETTINGS_BEGIN);
    result = command(1, uint32_t(size), payload, 2, 4096);
    if (result != QspiResult::OK) return result;
    for (uint32_t a = 4096; a < SETTINGS_BEGIN; a += 4096) {
      const bool last = a + 4096 == SETTINGS_BEGIN;
      result = command(2, a, nullptr, last ? 3 : 2, last ? 0 : a + 4096);
      if (result != QspiResult::OK) return result;
      io_.progress(QspiPhase::ERASING, a + 4096, SETTINGS_BEGIN);
    }
    for (uint32_t a = 0; a < size; a += 128) {
      result = command(3, a, image + a, 3, a + 128);
      if (result != QspiResult::OK) return result;
      io_.progress(QspiPhase::WRITING, a + 128, size);
    }
    result = command(4, uint32_t(size), nullptr, 4, 0);
    if (result != QspiResult::OK) return result;
    for (uint32_t a = 0; a < size; a += 4096) {
      const bool last = a + 4096 == size;
      result = command(5, a, nullptr, last ? 5 : 4, a + 4096);
      if (result != QspiResult::OK) return result;
      io_.progress(QspiPhase::VERIFYING, a + 4096, size);
    }
    io_.progress(QspiPhase::VERIFIED, size, size);
    return QspiResult::OK;
  }

 private:
  static constexpr uint32_t SETTINGS_BEGIN = 0x200000;
  IO &io_;
  bool used_{};
  uint32_t expected_crc_{};

  QspiResult poll(unsigned state, uint32_t progress, uint32_t timeout) {
    const uint32_t began = io_.now();
    while (uint32_t(io_.now() - began) < timeout) {
      if (io_.cancelled()) return QspiResult::CANCELLED;
      io_.wait(1);
      const uint8_t request[]{0x30, 0x94, 17};
      if (io_.transfer(0x2c, request, sizeof request, nullptr, 0)) continue;
      io_.wait(1);
      uint8_t response[17]{};
      if (io_.transfer(0x2c, nullptr, 0, response, sizeof response)) continue;
      if (response[0] == 64) continue;
      if (response[0] || std::memcmp(response + 1, "RUP2", 4) ||
          qspi_u32(response + 13) != expected_crc_) return QspiResult::IDENTITY;
      if (response[6] || response[5] == 6) return QspiResult::WRITER_ERROR;
      if (response[5] == state && qspi_u32(response + 9) == progress) return QspiResult::OK;
    }
    return QspiResult::TIMEOUT;
  }

  QspiResult command(unsigned op, uint32_t offset, const uint8_t *data,
                     unsigned state, uint32_t progress) {
    if (io_.cancelled()) return QspiResult::CANCELLED;
    uint8_t packet[135]{0x30, 19, 132};
    qspi_put32(packet + 3, (uint32_t(op) << 24) | offset);
    if (data) std::memcpy(packet + 7, data, 128);
    // An uncertain ACK can mean accepted. Never resend a destructive command.
    (void) io_.transfer(0x2c, packet, sizeof packet, nullptr, 0);
    return poll(state, progress, 6000);
  }
};
}  // namespace esphome::voicedsp_v2
