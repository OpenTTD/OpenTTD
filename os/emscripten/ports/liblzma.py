import os
import logging

VERSION = '5.4.6'
HASH = '495cc890d25c075c927c907b77e60d86dd8a4c377cea5b1172c8e916984149a7bb5fb32db25091f7219346b83155b47e4bc0404cc8529d992014cd7ed0c278b7'

URL = 'https://github.com/tukaani-project/xz'
DESCRIPTION = 'liblzma provides a general-purpose data-compression library.'
LICENSE = 'LGPL-2.1'

def get(ports, settings, shared):
  ports.fetch_project('contrib.liblzma', f'https://github.com/tukaani-project/xz/releases/download/v{VERSION}/xz-{VERSION}.tar.xz', sha512hash=HASH)

  def create(final):
    logging.info('building port: contrib.liblzma')

    ports.clear_project_build('contrib.liblzma')

    source_path = os.path.join(ports.get_dir(), 'contrib.liblzma', f'xz-{VERSION}', 'src', 'liblzma')
    ports.write_file(os.path.join(source_path, 'config.h'), config_h)
    ports.install_headers(os.path.join(source_path, 'api'), pattern='lzma.h')
    ports.install_headers(os.path.join(source_path, 'api', 'lzma'), pattern='*.h', target='lzma')

    build_flags = ['-DHAVE_CONFIG_H', '-DTUKLIB_SYMBOL_PREFIX=lzma_', '-fvisibility=hidden']
    exclude_files = ['crc32_small.c', 'crc64_small.c', 'crc32_tablegen.c', 'crc64_tablegen.c', 'price_tablegen.c', 'fastpos_tablegen.c',
                     'tuklib_exit.c', 'tuklib_mbstr_fw.c', 'tuklib_mbstr_width.c', 'tuklib_open_stdxxx.c', 'tuklib_progname.c']
    include_dirs_rel = ['../common', 'api', 'check', 'common', 'delta', 'lz', 'lzma', 'rangecoder', 'simple']

    include_dirs = [os.path.join(source_path, p) for p in include_dirs_rel]
    ports.build_port(source_path, final, 'contrib.liblzma', flags=build_flags, exclude_files=exclude_files, includes=include_dirs)

  return [shared.cache.get_lib('liblzma.a', create, what='port')]


def clear(ports, settings, shared):
  shared.cache.erase_lib('liblzma.a')


def process_args(ports):
  return []


config_h = '''
#define ASSUME_RAM 128
#define ENABLE_NLS 1
#define HAVE_CHECK_CRC32 1
#define HAVE_CHECK_CRC64 1
#define HAVE_CHECK_SHA256 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DCGETTEXT 1
#define HAVE_DECL_CLOCK_MONOTONIC 1
#define HAVE_DECL_PROGRAM_INVOCATION_NAME 1
#define HAVE_DECODERS 1
#define HAVE_DECODER_ARM 1
#define HAVE_DECODER_ARMTHUMB 1
#define HAVE_DECODER_DELTA 1
#define HAVE_DECODER_IA64 1
#define HAVE_DECODER_LZMA1 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_DECODER_POWERPC 1
#define HAVE_DECODER_SPARC 1
#define HAVE_DECODER_X86 1
#define HAVE_DLFCN_H 1
#define HAVE_ENCODERS 1
#define HAVE_ENCODER_ARM 1
#define HAVE_ENCODER_ARMTHUMB 1
#define HAVE_ENCODER_DELTA 1
#define HAVE_ENCODER_IA64 1
#define HAVE_ENCODER_LZMA1 1
#define HAVE_ENCODER_LZMA2 1
#define HAVE_ENCODER_POWERPC 1
#define HAVE_ENCODER_SPARC 1
#define HAVE_ENCODER_X86 1
#define HAVE_FCNTL_H 1
#define HAVE_FUTIMENS 1
#define HAVE_GETOPT_H 1
#define HAVE_GETOPT_LONG 1
#define HAVE_GETTEXT 1
#define HAVE_IMMINTRIN_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MBRTOWC 1
#define HAVE_MEMORY_H 1
#define HAVE_MF_BT2 1
#define HAVE_MF_BT3 1
#define HAVE_MF_BT4 1
#define HAVE_MF_HC3 1
#define HAVE_MF_HC4 1
#define HAVE_OPTRESET 1
#define HAVE_POSIX_FADVISE 1
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#define HAVE_PTHREAD_PRIO_INHERIT 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UINTPTR_T 1
#define HAVE_UNISTD_H 1
#define HAVE_VISIBILITY 1
#define HAVE_WCWIDTH 1
#define HAVE__BOOL 1
#define HAVE___BUILTIN_ASSUME_ALIGNED 1
#define HAVE___BUILTIN_BSWAPXX 1
#define MYTHREAD_POSIX 1
#define NDEBUG 1
#define PACKAGE "xz"
#define PACKAGE_BUGREPORT "lasse.collin@tukaani.org"
#define PACKAGE_NAME "XZ Utils"
#define PACKAGE_STRING "XZ Utils 5.4.0"
#define PACKAGE_TARNAME "xz"
#define PACKAGE_VERSION "5.4.0"
#define SIZEOF_SIZE_T 4
#define STDC_HEADERS 1
#define TUKLIB_CPUCORES_SYSCONF 1
#define TUKLIB_FAST_UNALIGNED_ACCESS 1
#define TUKLIB_PHYSMEM_SYSCONF 1
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
#define VERSION "5.4.0"
'''
