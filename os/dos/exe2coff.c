/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
/* Updated 2008 to use fread/fopen and friends instead of read/open so it compiles with GCC on Unix (Rubidium) */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


static void
exe2aout(char *fname)
{
  unsigned short header[3];
  FILE *ifile;
  FILE *ofile;
  char buf[4096];
  int rbytes;
  char *dot = strrchr(fname, '.');
  if (!dot || strlen(dot) != 4
      || tolower(dot[1]) != 'e'
      || tolower(dot[2]) != 'x'
      || tolower(dot[3]) != 'e')
  {
    fprintf(stderr, "%s: Arguments MUST end with a .exe extension\n", fname);
    return;
  }

  ifile = fopen(fname, "rb");
  if (!ifile)
  {
    perror(fname);
    return;
  }
  fread(header, sizeof(header), 1, ifile);
  if (header[0] == 0x5a4d)
  {
    long header_offset = (long)header[2]*512L;
    if (header[1])
      header_offset += (long)header[1] - 512L;
    fseek(ifile, header_offset, SEEK_SET);
    header[0] = 0;
    fread(header, sizeof(header), 1, ifile);
    if ((header[0] != 0x010b) && (header[0] != 0x014c))
    {
      fprintf(stderr, "`%s' does not have a COFF/AOUT program appended to it\n", fname);
      return;
    }
    fseek(ifile, header_offset, SEEK_SET);
  }
  else
  {
    fprintf(stderr, "`%s' is not an .EXE file\n", fname);
    return;
  }

  *dot = 0;
  ofile = fopen(fname, "w+b");
  if (!ofile)
  {
    perror(fname);
    return;
  }

  while ((rbytes=fread(buf, 1, 4096, ifile)) > 0)
  {
    int wb = fwrite(buf, 1, rbytes, ofile);
    if (wb < 0)
    {
      perror(fname);
      break;
    }
    if (wb < rbytes)
    {
      fprintf(stderr, "`%s': disk full\n", fname);
      exit(1);
    }
  }
  fclose(ifile);
  fclose(ofile);
}

int
main(int argc, char **argv)
{
  int i;
  if (argc == 1) printf("Usage: %s <exename>", argv[0]);
  for (i=1; i<argc; i++)
    exe2aout(argv[i]);
  return 0;
}

