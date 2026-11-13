#include "VMatrix.h"
#include <iostream>
#include <stdlib.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

/*
 *  Round a 2I.6Q fixed point to 2I.3Q
 */
uint8_t fixed_point_round_to_even(uint8_t num) {
   // In middle -> rount to even
   if ((num & 0x07) == 0x04) {
      if ((num & 0x08) == 0x08)
         return (num + 0x08) & ~(0x07);
      else
         return (num) & ~(0x07);
      // Isn't middle, must round up
   } else if ((num & 0x04) == 0x04) {
      return (num + 0x08) & ~(0x07);
      // Otherwise round down
   } else {
      return num & ~(0x07);
   }
}

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
      uint8_t addend1_exp = this->get_exponent();
      int16_t addend1 = (this->get_fraction() | 1 << 3) << 3;
      if (this->get_exponent() == 0 && this->get_fraction() == 0)
         addend1 = 0;

      uint16_t addend2_exp = n.get_exponent();
      int16_t addend2 = (n.get_fraction() | 1 << 3) << 3;
      if (n.get_exponent() == 0 && n.get_fraction() == 0)
         addend2 = 0;

      // Align Exponents
      while (addend1_exp != addend2_exp) {
         if (addend1_exp < addend2_exp) {
            addend1 >>= 1;
            addend1_exp++;
         } else {
            addend2 >>= 1;
            addend2_exp++;
         }
      }

      // Negate if necessary
      if (this->get_sign()) {
         addend1 = ~addend1 + 1;
      }
      if (n.get_sign()) {
         addend2 = ~addend2 + 1;
      }

      // addends are now in 2's complement fixed point 2I.3Q
      int16_t sum = addend1 + addend2;
      uint8_t sum_sign = sum < 0;
      int sum_exp = addend1_exp;

      // Convert to magnitude if result is negative
      if (sum_sign) {
         sum = ~(sum) + 1;
      }

      while (true) {
         // If it is zero return 0 form
         if (sum == 0)
            return CFloat8(0);

         // Fraction overflow? 1x.xxxxxx
         if (sum & 0x80) {
            sum >>= 1;
            sum_exp++;
         }

         // Normalize it x0.xxxxxx -> x1.xxxxxx
         while (!(sum & 0x40)) {
            sum <<= 1;
            sum_exp--;
         }

         if (sum_exp < 0)
            return CFloat8(0);
         if (sum_exp > 15)
            return CFloat8(0xF, sum_sign, 0xF);

         // Round, shifting over 3 to reuse function
         sum = fixed_point_round_to_even(sum);

         // Return result if normalized
         if ((sum & 0x40))
            return CFloat8(sum_exp, sum_sign, sum >> 3);
      }
   }

   CFloat8 operator*(const CFloat8 &n) {
      uint8_t product_exponent =
          n.get_exponent() + this->get_exponent() - this->bias;

      uint8_t product_sign = this->get_sign() ^ n.get_sign();

      // Multiplicand and Multiplier contain implicit 1
      uint8_t multiplicand = n.get_fraction() | (1 << 3);
      uint8_t multiplier = this->get_fraction() | (1 << 3);
      if (this->get_exponent() == 0 && this->get_fraction() == 0)
         multiplicand = 0;
      if (n.get_exponent() == 0 && n.get_fraction() == 0)
         multiplier = 0;

      // Product is now in form 2I.6Q
      uint8_t product = multiplicand * multiplier;

      // Until steady, loop
      while (true) {
         if (product == 0)
            return CFloat8(0, product_sign, 0);

         // Check fraction overflow eg. 1x.xxxxxx
         if ((product & 0x80) == 0x80) {
            product >>= 1;
            product_exponent += 1;
         }

         // Normalize if necessary eg. 0.xxxxxx -> 1.xxxxxx
         while (!(product & 0x40)) {
            product <<= 1;
            product_exponent -= 1;
         }

         // Check exponent overflow
         if (product_exponent > 15)
            return CFloat8(0xF, product_sign, 0xF);

         product = fixed_point_round_to_even(product);

         // If normalized return
         if ((product & 0x40) == 0x40)
            return CFloat8(product_exponent, product_sign, product >> 3);
      }
   }
};

class CFloatMat3 {
 public:
   CFloat8 array[3][3];

   CFloatMat3()
       : array{{CFloat8(0), CFloat8(0), CFloat8(0)},
               {CFloat8(0), CFloat8(0), CFloat8(0)},
               {CFloat8(0), CFloat8(0), CFloat8(0)}} {}

   CFloatMat3 operator*(const CFloatMat3 &n) {
      CFloat8 result[3][3] = {{CFloat8(0), CFloat8(0), CFloat8(0)},
                              {CFloat8(0), CFloat8(0), CFloat8(0)},
                              {CFloat8(0), CFloat8(0), CFloat8(0)}};

      for (int i = 0; i < 3; i++) {

         for (int j = 0; j < 3; j++) {

            for (int k = 0; k < 3; k++) {
               result[i][j] =
                   result[i][j] + (this->array[i][k] * n.array[k][j]);
            }
         }
      }

      CFloatMat3 out;
      for (int i = 0; i < 3; i++)
         for (int j = 0; j < 3; j++)
            out.array[i][j] = result[i][j];
      return out;
   }

   void display() {
      std::cout << "[\n";
      for (int row = 0; row < 3; row++) {
         for (int col = 0; col < 3; col++) {
            std::cout << this->array[row][col] << " ";
         }
         std::cout << std::endl;
      }
      std::cout << "]\n";
   }
};

int main() {

   // Test 1: Identity * Identity = Identity
   CFloatMat3 identity;
   identity.array[0][0] = CFloat8(0, 0, 0); // Will just use simple values
   // Build identity matrix: diagonal = 1.0 (exp=0 w/ bias=0 means 1.0 =
   // exp=0,frac=0 -> actually need to think) CFloat8(exponent, sign, fraction):
   // value = 1.fraction * 2^exponent 1.0 = 1.000 * 2^0 -> exp=0, sign=0, frac=0
   // => CFloat8(0,0,0) = 0 since exp==0 && frac==0 is zero Looking at display:
   // zero check is exp==0 && frac==0, so 1.0 needs exp with bias... bias=0,
   // so 1.0 = 1.000 * 2^0 -> exp=0, frac=0 but that's the zero encoding Use
   // exp=1 for a known nonzero: 1.000 * 2^1 = 2.0 Let's just use simple test
   // values

   // Test 1: simple known matrices
   CFloatMat3 A, B;
   // A = [[2, 0, 0], [0, 2, 0], [0, 0, 2]] (2*identity)
   // 2.0 = 1.000 * 2^1 -> CFloat8(1, 0, 0)
   A.array[0][0] = CFloat8(1, 0, 0); // 2.0
   A.array[1][1] = CFloat8(1, 0, 0); // 2.0
   A.array[2][2] = CFloat8(1, 0, 0); // 2.0

   // B = [[1, 0, 0], [0, 1, 0], [0, 0, 1]] -- can't encode 1.0 with bias=0,
   // use 2.0 again B = 2*identity as well, so A*B should = 4*identity 4.0
   // = 1.000 * 2^2 -> CFloat8(2, 0, 0)
   B.array[0][0] = CFloat8(1, 0, 0); // 2.0
   B.array[1][1] = CFloat8(1, 0, 0); // 2.0
   B.array[2][2] = CFloat8(1, 0, 0); // 2.0

   std::cout << "=== Test 1: (2*I) * (2*I) = 4*I ===\n";
   std::cout << "A:\n";
   A.display();
   std::cout << "B:\n";
   B.display();
   CFloatMat3 C = A * B;
   std::cout << "A * B:\n";
   C.display();

   // Test 2: non-diagonal matrices
   // 1.5 = 1.100 * 2^0 -> CFloat8(0, 0, 4) -- but exp=0,frac!=0 is nonzero?
   // Looking at zero check: exp==0 && frac==0 -> zero. So exp=0,frac=4 is valid
   // nonzero. value = (1 + 4/8) * 2^0 = 1.5
   CFloatMat3 X, Y;
   X.array[0][0] = CFloat8(1, 0, 0); // 2.0
   X.array[0][1] = CFloat8(0, 0, 4); // 1.5
   X.array[0][2] = CFloat8(1, 0, 4); // 3.0 = 1.100*2^1
   X.array[1][0] = CFloat8(0, 1, 0); // -1.0... wait bias=0 exp=0 frac=0 is zero
   // -2.0 = CFloat8(1, 1, 0)
   X.array[1][0] = CFloat8(1, 1, 0); // -2.0
   X.array[1][1] = CFloat8(1, 0, 0); // 2.0
   X.array[1][2] = CFloat8(0, 0, 0); // 0
   X.array[2][0] = CFloat8(0, 0, 0); // 0
   X.array[2][1] = CFloat8(0, 0, 4); // 1.5
   X.array[2][2] = CFloat8(1, 0, 0); // 2.0

   Y.array[0][0] = CFloat8(1, 0, 0); // 2.0
   Y.array[0][1] = CFloat8(0, 0, 0); // 0
   Y.array[0][2] = CFloat8(0, 0, 4); // 1.5
   Y.array[1][0] = CFloat8(0, 0, 4); // 1.5
   Y.array[1][1] = CFloat8(1, 0, 0); // 2.0
   Y.array[1][2] = CFloat8(0, 0, 0); // 0
   Y.array[2][0] = CFloat8(1, 1, 0); // -2.0
   Y.array[2][1] = CFloat8(0, 0, 4); // 1.5
   Y.array[2][2] = CFloat8(1, 0, 0); // 2.0

   std::cout << "\n=== Test 2: general matrices ===\n";
   std::cout << "X:\n";
   X.display();
   std::cout << "Y:\n";
   Y.display();
   CFloatMat3 Z = X * Y;
   std::cout << "X * Y:\n";
   Z.display();

   VMatrix *dut = new VMatrix;

   Verilated::traceEverOn(true);
   VerilatedVcdC *m_trace = new VerilatedVcdC;
   dut->trace(m_trace, 5);
   m_trace->open("waveform.vcd");

   dut->a00 = X.array[0][0].get_raw();
   dut->a01 = X.array[0][1].get_raw();

   m_trace->close();
   delete dut;
}
