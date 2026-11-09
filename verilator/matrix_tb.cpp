#include "VMatrix.h"
#include <iostream>
#include <stdlib.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

class CFloat8 {
 private:
   uint8_t num;
   uint8_t bias = 0;

 public:
   CFloat8(uint8_t exponent, uint8_t sign, uint8_t fraction) {
      this->num = (sign & 1) << 7;
      this->num |= (exponent & 0xF) << 3;
      this->num |= (fraction & 0x7);
   }

   CFloat8(uint8_t num) { this->num = num; }

   uint8_t get_sign() const { return (this->num & 0x80) >> 7; }

   uint8_t get_exponent() const { return (this->num & 0x78) >> 3; }

   uint8_t get_fraction() const { return (this->num & 0x7); }

   uint8_t get_raw() const { return this->num; }

   std::string display_binary() const {
      std::string str;
      if (this->get_sign())
         str.append("-");

      str.append("1.");
      uint8_t fraction = this->get_fraction();
      for (int i = 0; i < 3; i++) {
         if (fraction & 0x4)
            str.append("1");
         else
            str.append("0");

         fraction <<= 1;
      }

      str.append(" 2^");
      int exponent = this->get_exponent();
      exponent = exponent - this->bias;
      str.append(std::to_string(exponent));

      return str;
   }

   friend std::ostream &operator<<(std::ostream &out, CFloat8 &a) {
      if (a.get_exponent() == 0 && a.get_fraction() == 0) {
         out << 0;
      } else {
         int8_t exponent = a.get_exponent();
         int8_t biased_exponent = exponent - a.bias;
         double number = (1.0 + ((double)(a.get_fraction()) / 8.0)) *
                         std::pow(2.0, biased_exponent);
         if (a.get_sign()) {
            number *= -1;
         }
         out << number;
      }
      return out;
   }

   CFloat8 operator+(const CFloat8 &n) {
      typedef struct {
         uint16_t frac;
         uint8_t exp;
      } Intermediate;

      Intermediate num0 = {
          .frac = (uint16_t)(this->get_fraction() << 4 | this->get_sign() << 7),
          .exp = this->get_exponent(),
      };
      Intermediate num1 = {
          .frac = (uint16_t)(n.get_fraction() << 4 | n.get_sign() << 7),
          .exp = n.get_exponent()};

      // Shift exponents until they match
      while (num0.exp != num1.exp) {
         if (num0.exp < num1.exp) {
            num0.frac = (num0.frac >> 1) | (num0.frac & 0x80);
            num0.exp++;
         } else {
            num1.frac = (num1.frac >> 1) | (num1.frac & 0x80);
            num1.exp++;
         }
      }

      Intermediate sum = {
          .frac = (uint16_t)(num0.frac + num1.frac),
          .exp = num0.exp,
      };

      while (true) {
         if (sum.frac == 0)
            return CFloat8(0, 0, 0);

         // If signs match but sum's sign doesn't we overflowed
         if ((num0.frac & 0x80) == (num1.frac & 0x80) &&
             (sum.frac & 0x80) != (num0.frac & 0x80)) {
            sum.frac >>= 1;
            sum.exp++;
         }

         // Normalize
         while ((num0.frac & 0x40) != 0x40) {
            sum.frac <<= 1;
            sum.exp--;
         }

         // If overflowed saturate
         if (sum.exp > 15) {
            if ((sum.frac & 0x80) == 0x80)
               return CFloat8(0xFF);
            else
               return CFloat8(0x7F);
         }

         // Round down and return
         if ((sum.frac & 0x0F) == 0)
            return CFloat8(sum.exp, (sum.frac & 0x80) >> 7, sum.frac >> 4);

         // Tie to even
         if ((sum.frac & 0x0F) == 0x08) {
            if ((sum.frac & 0x10) == 0x10)
               sum.frac = (sum.frac + 0x10) & 0xFFF0;
            else
               return CFloat8(sum.exp, (sum.frac & 0x80) >> 7, sum.frac >> 4);
         } else
            sum.frac = (sum.frac + 0x10) & 0xFFF0;

         // If normalized return
         if ((sum.frac & 0x40) == 0x40)
            return CFloat8(sum.exp, (sum.frac & 0x80) >> 7, sum.frac >> 4);
      }
   }
};

int main() {
   // VMatrix *dut = new VMatrix;
   //
   // Verilated::traceEverOn(true);
   // VerilatedVcdC *m_trace = new VerilatedVcdC;
   // dut->trace(m_trace, 5);
   // m_trace->open("waveform.vcd");
   //
   // m_trace->close();
   // delete dut;

   CFloat8 num0(7, 1, 0b110);
   CFloat8 num1(1, 1, 0b101);
   CFloat8 sum = num0 + num1;

   std::cout << num0 << std::endl;
   std::cout << num1 << std::endl;
   std::cout << sum << std::endl;
}
