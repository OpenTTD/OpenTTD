#include <stdio.h>

// This pretty simple file checks if the system is LITTLE_ENDIAN or BIG_ENDIAN
//  it does that by putting a 1 and a 0 in an array, and read it out as one
//  number. If it is 1, it is LITTLE_ENDIAN, if it is 256, it is BIG_ENDIAN
//
// After that it outputs the contents of an include files (endian.h)
//  that says or TTD_LITTLE_ENDIAN, or TTD_BIG_ENDIAN. Makefile takes
//  care of the real writing to the file.

int main () {
  unsigned char EndianTest[2] = { 1, 0 };
  printf("#ifndef ENDIAN_H\n#define ENDIAN_H\n");
  if( *(short *) EndianTest == 1 )
    printf("#define TTD_LITTLE_ENDIAN\n");
  else
    printf("#define TTD_BIG_ENDIAN\n");

  printf("#endif\n");

  return 0;
}
