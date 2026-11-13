#pragma once
// Diagnostic boot only, before any host transaction. NXP UM10204 bus clear.
// Callers must provide released open-drain pins (0=SDA, 1=SCL), never push-pull.
template<class Read, class Drive, class Wait>
int v2_aec_bus_clear(Read read, Drive drive, Wait wait) {
  if (!read(1)) return -1;
  if (read(0)) return 0;
  for (unsigned i=0;i<100;++i) {
    wait(50);
    if (!read(1) || read(0)) return -2; // Bus activity: do not interfere.
  }
  for (unsigned i=0;i<9;++i) {
    drive(1,false);wait(50);drive(1,true);
    unsigned n=0;
    while(!read(1) && n++<100)wait(10);
    if(!read(1))return -1;
    wait(50);
  }
  if(!read(0))return -3;
  drive(1,false);drive(0,false);wait(50);drive(1,true);wait(50);
  if(!read(1)){drive(0,true);return -1;}
  drive(0,true);wait(50);
  return read(0)&&read(1)?1:-3;
}
