#pragma once

#include <istream>
#include <ostream>

namespace mcmini::real_world {

template <typename T>
struct remote_address final {
 private:
  T* remote_addr;

 public:
  T* get() const { return remote_addr; }

  // template <typename U>
  // friend std::ostream& operator<<(std::ostream& os, remote_address<U>& ra) {
  //   return (os << static_cast<void*>(ra.remote_addr));
  // }
  // template <typename U>
  // friend std::istream& operator>>(std::istream& is, remote_address<U>& ra) {
  //   return (is >> static_cast<const void*>(ra.get()));
  // }
};

template <typename T>
using remote_object = remote_address<T>;

}  //  namespace mcmini::real_world