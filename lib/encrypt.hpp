#ifndef __SM__ENCRYPTOR__
#define __SM__ENCRYPTOR__


#include <iostream>
#include <string_view>
#include <string>
#include <cmath>


const long long mod = 1e6 + 7;
const long long alphabet_size = 70LL;


long long encrypt(const std::string& s) {
  long long h = 0;
  int m = static_cast<int>(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    int symbol = s[i] - 'a' + 1;
    h = (h + m * symbol) % mod;
    m = ((m % mod) * (alphabet_size % mod)) % mod;
  }
  return h;
}

#endif //__SM__ENCRYPTOR__
