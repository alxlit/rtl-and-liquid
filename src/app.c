#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

#include <stdint.h>

#include "controller.h"
#include "demod.h"
#include "macros.h"
#include "rtl.h"
#include "scanner.h"
#include "websocket.h"

static bool exiting = false;

static void sighandler(int signum)
{
  DEBUG("Signal %d received, exiting!\n", signum);
  exiting = true;
}

int main()
{
  //  uint8_t x = 254;
  //  printf("%d \n", ((int8_t) x) + 128);
  
  //  return 0;

  // initialize signal handler
  struct sigaction sigact;

  sigact.sa_handler = sighandler;
  sigact.sa_flags = 0;
  sigemptyset( & sigact.sa_mask);
  sigaction(SIGINT,  & sigact, NULL);
  sigaction(SIGTERM, & sigact, NULL);
  sigaction(SIGQUIT, & sigact, NULL);
  sigaction(SIGPIPE, & sigact, NULL);
  
  // initialize components
  controller ctrl;
  demod dem = demod_create();
  rtl r = rtl_create(-1);
  scanner scan = scanner_create();
  websocket ws = websocket_create();

  ctrl = controller_create(dem, r, scan, ws);
  
  // execute the controller  
  while ( ! exiting) { controller_execute(ctrl); }
  
  demod_exit(dem);
    
  // TODO make the order arbitrary (at the moment demod must be destroyed
  // before the RTL, otherwise it hangs)
  demod_destroy(dem);
  websocket_destroy(ws);
  rtl_destroy(r);
  scanner_destroy(scan);
  controller_destroy(ctrl);
  
  return 0;
}
