#include <eq_fct.h>

#include <cassert>

__attribute__((weak)) void interrupt_usr1_prolog(int) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void interrupt_usr1_epilog(int) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void interrupt_usr2_prolog(void) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void interrupt_usr2_epilog(void) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void post_init_prolog(void) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void post_init_epilog(void) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void eq_cancel(void) {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void refresh_prolog() {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void refresh_epilog() {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void eq_init_prolog() {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) void eq_init_epilog() {
  // Code should not be reached
  assert(false);
}

__attribute__((weak)) EqFct* eq_create(int, void*) {
  // Code should not be reached
  assert(false);

  return nullptr;
}
