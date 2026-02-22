#include "core/Bus.h"
#include "core/CPU.h"
#include <gtest/gtest.h>

class CPUTest : public ::testing::Test {
protected:
  Bus bus;
  CPU cpu{bus};

  void SetUp() override { cpu.reset(); }
};

TEST_F(CPUTest, InitialState) {
  EXPECT_EQ(cpu.AF.hi, 0x01);
  EXPECT_EQ(cpu.AF.lo, 0xB0);
  EXPECT_EQ(cpu.BC.reg16, 0x0013);
  EXPECT_EQ(cpu.DE.reg16, 0x00D8);
  EXPECT_EQ(cpu.HL.reg16, 0x014D);
  EXPECT_EQ(cpu.SP, 0xFFFE);
  EXPECT_EQ(cpu.PC, 0x0100);
}

TEST_F(CPUTest, NopInstruction) {
  bus.write(0x0100, 0x00); // NOP
  int cycles = cpu.tick();
  EXPECT_EQ(cycles, 4);
  EXPECT_EQ(cpu.PC, 0x0101);
}
