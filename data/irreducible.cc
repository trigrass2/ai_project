#include "sys.h"
#include "debug.h"
#include <iostream>
#include <libecc/fieldmath.h>
#include <libecc/polynomial.h>
#include <map>

using libecc::polynomial;
using libecc::bitset;

#include "input.hcc"	// Generated by 'test_primitive'.
int const factor_strings_size = sizeof(factor_strings) / sizeof(char const*);

std::ostream& operator<<(std::ostream& os, mpz_srcptr ptr)
{
  static char output_buf[1024];
  mpz_get_str(output_buf, 10, ptr);
  os << output_buf;
  return os;
}

typedef polynomial<m, k> poly_t;
poly_t const zero("0");
poly_t const one("1");
poly_t const g("2");

class square_functor {
  private:
    mutable libecc::bitset_digit_t buf[poly_t::square_digits];
  public:
    void operator()(poly_t& p) const { p = p.square(buf); }
};

int main()
{
  std::ios::sync_with_stdio(false);
  Debug( check_configuration() );
  Debug( libcw_do.on() );
  Debug( libcw_do.set_ostream(&std::cout) );
  //Debug( dc::notice.on() );

  poly_t p = g;

  libecc::bitset_digit_t tmp[poly_t::square_digits];
  for(int i = 0; i < m; ++i)
  {
#ifdef CWDEBUG
    if (m < 64)
      Dout(dc::notice, g << "^(2^" << i << ") = " << p);
#endif
    p = p.square(tmp);
  }
#ifdef CWDEBUG
  if (m < 64)
    Dout(dc::notice, g << "^(2^" << m << ") = " << p);
#endif

  std::cout << m << "\t" << k << "\t";
  if (p != g)
  {
    std::cout << "NOT irreducible." << std::endl;
    return 0;
  }
  for (int d = 1; d < m; ++d)
  {
    if (m % d == 0)
    {
      mpz_class power_of_two;
      mpz_ui_pow_ui(power_of_two.get_mpz_t(), 2, d);
      poly_t x(2);
      square_functor sqftr;
      poly_t e(exponentiation(x, power_of_two, sqftr) - x);
      bitset<m + 1> y(e.get_bitset());
      bitset<m + 1> z(1);
      z.set(k);
      z.set(m);
      bitset<m+1>& r(gcd(y, z));
      Dout(dc::notice, "[Divisor " << d << ": GCD(t^" << m << " + t^" << k << " + 1, t^" << power_of_two << " - t) = " << r << ']');
      r.flip(0);
      if (r.any())
      {
	std::cout << "NOT irreducible." << std::endl;
	return 0;
      }
    }
  }

  if (factor_strings_size == 0)
  {
    std::cout << "Irreducible.  Factorization of 2^" << m <<
        " - 1 is not known, cannot check if this irreducible trinomial is primitive or not." << std::endl;
    return 0;
  }

  mpz_class n(1);
  mpz_class f[factor_strings_size];
  for(int i = 0; i < factor_strings_size; ++i)
    f[i] = 1;
  bool composite = false;
  std::map<mpz_class, int> factors;
  size_t number_of_factors = factor_strings_size;
  for(int i = 0; i < factor_strings_size; ++i)
  {
    mpz_class p(factor_strings[i]);
    if (p == 1)	// If the last factor was composite, then the last factor in the list is set to 1.
    {
      composite = true;
      --number_of_factors;
      break;
    }
    n *= p;
    std::pair<std::map<mpz_class, int>::iterator, bool> res = factors.insert(std::map<mpz_class, int>::value_type(p, 1));
    if (!res.second)
      ++(*res.first).second;
  }
  square_functor sqftr;
  assert( n != 0 && exponentiation(g, n + 1, sqftr) == g );

  mpz_class period(n);
  unsigned int cnt = 0;
  bool composite_is_factor_of_period = false;
  for(std::map<mpz_class, int>::iterator iter = factors.begin(); iter != factors.end(); ++iter)
  {
    mpz_class prime_pwr((*iter).first);
    for (int i = 1; i < (*iter).second; ++i)
      prime_pwr *= (*iter).first;
    period /= prime_pwr;
    poly_t a1(exponentiation(g, period, sqftr));
    composite_is_factor_of_period = false;
    while (a1 != one)
    {
      a1 = exponentiation(a1, (*iter).first, sqftr);
      period *= (*iter).first;
      composite_is_factor_of_period = composite;
      ++cnt;
    }
  }

  if (cnt == number_of_factors)
  {
    if (composite)
      std::cout << "Possibly primitive.  " << n / factors.rbegin()->first << " < order <= 2^" << m << " - 1." << std::endl;
    else
      std::cout << "Primitive." << std::endl;
  }
  else
  {
    std::cout << "Irreducible but not primitive.  ";
    if (composite_is_factor_of_period)
      std::cout << period / factors.rbegin()->first << " < order <= " << period << '.' << std::endl;
    else
      std::cout << "The order is " << period << '.' << std::endl;
  }

  return 0;
}
