#include <cstdio>
#include <cstring>
#include <cmath>
#include <iostream>

auto nextInt () -> int;
auto readN (int *array, int n) -> void;
template <typename T>
auto print (const T &) -> void;
template <typename T>
auto println (const T &) -> void;
template <typename T>
auto printsp (const T &) -> void;

const int kMaxn = 1e6 + 10;
const int kMaxm = 2e3 + 10;
int count[kMaxm];
int a[kMaxn];

auto main () -> int {
  int n = nextInt(), m = nextInt();
  int l = 0, r = -1;
  readN(a, n);
  int min = kMaxn + 1, minAt = 0;
  while (r < n) {
    for (int i = 0; i < m; ++i) {
      while (count[i] == 0) {
        ++r;
        if (r >= n) goto final;
        ++count[a[r] - 1];
      }
    }
    while (true) {
      if (min > r - l + 1) {
        min = r - l + 1;
        minAt = l;
      }
      if (--count[a[l++] - 1] == 0) break;
    }
  }
  final:
  printsp(minAt + 1);
  printsp(min + minAt);
  return 0;
}




auto nextInt () -> int {
  int i = 0, sign = 1;
  char c;
  while (!isdigit(c = getchar())) if (c == '-') sign = -1;
  do {
    i = i * 10 + c - '0';
  } while (isdigit(c = getchar()));
  return i * sign;
}

auto readN (int *array, int n) -> void {
  for (int i = 0; i < n; ++i) array[i] = nextInt();
}

template <>
auto print<int> (const int &val) -> void {
  printf("%d", val);
}
template <>
auto print<char> (const char &val) -> void {
  putchar(val);
}
template <>
auto print<char *> (char * const &val) -> void {
  printf("%s", val);
}
template <>
auto print<const char *> (const char * const &val) -> void {
  printf("%s", val);
}
template <>
auto print<long long> (const long long &val) -> void {
  printf("%lld", val);
}
template <>
auto print<unsigned long long> (const unsigned long long &val) -> void {
  printf("%llu", val);
}
template <typename T>
auto println (const T &val) -> void {
  print(val);
  putchar('\n');
}
template <typename T>
auto printsp (const T &val) -> void {
  print(val);
  putchar(' ');
}
