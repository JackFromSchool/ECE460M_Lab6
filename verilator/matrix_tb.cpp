#include "VMatrix.h"
#include <ostream>
#include <stdlib.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

#define SSHIFT4(num) (num & 0x8) ? (num >> 1) & 0x80 : num >> 1;

class CFloat8 {
 private:
   uint8_t num;
   uint8_t bias;

 public:
   CFloat8(uint8_t exponent, uint8_t sign, uint8_t fraction) {
      this->num = (sign & 1) << 7;
      this->num = (exponent & 0xF) << 3;
      this->num = (fraction & 0x7);
   }

   CFloat8(uint8_t num) { this->num = num; }

   uint8_t get_sign() const { return (this->num & 0x80) >> 7; }

   uint8_t get_exponent() const { return (this->num & 0x78) >> 3; }

   uint8_t get_fraction() const { return (this->num & 0x7); }

   friend std::ostream &operator<<(std::ostream &out, CFloat8 &a) {
      if (a.get_exponent() == 0 && a.get_fraction() == 0) {
         out << 0;
      } else {
         int8_t exponent = a.get_exponent();
         int8_t biased_exponent = exponent - a.bias;
         float number = (1.0 + ((float)(a.get_fraction()) / 8.0)) *
                        std::pow(2.0, biased_exponent);
         if (a.get_sign()) {
            number *= -1;
         }
         out << number;
      }
      return out;
   }

   CFloat8 operator+(const CFloat8 &n) {
      uint8_t e1 = this->get_exponent();
      int8_t f1 = (this->get_fraction() << 4) | (this->get_sign() << 7);
      uint8_t e2 = n.get_exponent();
      int8_t f2 = (n.get_fraction() << 4) | (this->get_sign() << 7);

      while (e1 != e2) {
         if (e1 < e2) {
            f1 >>= 1;
            e1++;
         } else {
            f2 >>= 1;
            e2++;
         }
      }

      uint8_t esum = e1;
      int8_t fsum = f1 + f2;
      // If zero return normalized zero
      if (fsum == 0)
         return CFloat8(0, 0, 0);

      // If fraction overflow, shift right
      if (this->get_sign() == n.get_sign() && (fsum & 0x80) == 0x80) {
         fsum >>= 1;
         // Get rid of carried over sign
         fsum &= ~(0x80);
         esum++;
      }

      // Saturate if exponent overflowed.
      if (esum > 15) {
         if ((fsum & 0x80) == 0x80) {
            return CFloat8(0xFF);
         } else {
            return CFloat8(0x7F);
         }
      }

      // Normalize the fraction
      while ((fsum & 0x40) != 0x40) {
         fsum <<= 1;
         esum--;
      }
   }
};

int main() {
   VMatrix *dut = new VMatrix;

   // clang-format off
   int test_matrix[3][3] = {
      {0, 0, 0},
      {0, 1, 1},
      {0, 1, 1}
   };
   // clang-format on

   Verilated::traceEverOn(true);
   VerilatedVcdC *m_trace = new VerilatedVcdC;
   dut->trace(m_trace, 5);
   m_trace->open("waveform.vcd");

   m_trace->close();
   delete dut;
}
