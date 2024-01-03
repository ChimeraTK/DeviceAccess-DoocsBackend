#include "eq_dummy.h"
#include <d_fct.h>
#include <eq_client.h>
#include <eq_fct.h>

const char* object_name = "dummy_server";

void eq_init_prolog() {
  std::ostringstream os;

  printftostderr("eq_init_prolog", "DOOCS dummy server starting");
}

void eq_init_epilog() {}

#define NEW_LOCATION(loc_class)                                                                                        \
  case loc_class::fctCode:                                                                                             \
    eqn = new loc_class();                                                                                             \
    break;

EqFct* eq_create(int eq_code, void*) {
  EqFct* eqn = NULL;
  switch(eq_code) { NEW_LOCATION(eq_dummy); }
  return eqn;
}

void interrupt_usr1_prolog(int) {}

void interrupt_usr1_epilog(int) {}

void interrupt_usr2_prolog(void) {}

void interrupt_usr2_epilog(void) {}

void post_init_prolog(void) {}

void post_init_epilog(void) {}

void eq_cancel(void) {}

void refresh_prolog() {}

void refresh_epilog() {}
